#include "Network/Endpoints.h"
#include "Network/http.h"
#include "Network/LocalAddresses.h"
#include "IO/Files.h"
#include "XL.h"
#include "room.h"
#include "router.h"

#include <cstdlib>
#include <string>

// 用编译器自带的 _WIN32 判断平台，不依赖 CMake 里自定义的宏
#ifdef _WIN32
// 只为了 SetConsoleOutputCP：不然 UTF-8 的日志会被 GBK 控制台解码成乱码
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

const X_Y::XPath root = X_Y::XPath("D:/workbench/work");
#else
const X_Y::XPath root = X_Y::XPath("/workbench");
#endif

namespace
{
    // 截断到 maxBytes，但不把 UTF-8 的一个字切成两半
    std::string Ellipsis(const std::string &text, std::size_t maxBytes = 40)
    {
        if (text.size() <= maxBytes)
            return text;

        std::size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
            --cut;

        return text.substr(0, cut) + "…";
    }
} // namespace

// 启动入口只做四件事：配置、监听、accept 循环、把连接交给 Router。
// 用法：ChatWeb.exe [名字租约秒数]   不传就用默认 120 秒
int main(int argc, char **argv)
{
    using namespace X_Y;
    using namespace std::chrono_literals;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    int lease = Chat::Room::DefaultNameLease;
    if (argc > 1)
        lease = std::atoi(argv[1]);

    const XPath roomPage = (root / "HTML") / "ChatRoom.html";
    Chat::Room room(lease); // 整个服务器就一个聊天室

    try
    {
        StreamServer Server;
        const auto Addr = SocketAddr::Parse("0.0.0.0:8080");
        if (!Addr)
            return 1;
        if (!Server.Listen(*Addr))
        {
            CORE_ERROR("listen 8080 failed");
            return 1;
        }

        // 谁 5 秒不说话就断开他，别把后面的人堵死
        Server.SetTimeout(SocketTimeoutDirection::Receive, 5s);

        CORE_INFO("名字租约={}s", lease);
        CORE_INFO("page: {}", roomPage.Path().string());

        // 网卡列表：室友要用哪个地址，看这里
        CORE_INFO("网络接口:");
        for (const LocalInterface &nic : LocalInterfaces())
        {
            if (nic.addresses.empty())
                continue;

            std::string ips;
            for (const LocalAddress &address : nic.addresses)
            {
                const std::string text = address.address.ToString(); // "ip:0"
                const std::size_t colon = text.rfind(':');
                if (!ips.empty())
                    ips += ", ";
                ips += (colon == std::string::npos) ? text : text.substr(0, colon);
            }

            CORE_INFO("  [{}] {}{}  网关={}  MTU={}",
                      nic.device, ips,
                      nic.up ? "" : " (未启用)",
                      nic.gateway.empty() ? "-" : nic.gateway,
                      nic.mtu);
        }

        CORE_INFO("本机访问: http://127.0.0.1:8080");
        const std::string primary = PrimaryLocalIPv4();
        if (!primary.empty())
            CORE_INFO("室友访问: http://{}:8080", primary);
        else
            CORE_WARN("拿不到默认出口 IP，室友那边请用 ipconfig 查");

        for (;;)
        {
            auto connection = Server.Accept();
            if (!connection)
                continue;

            HttpParser parser(HttpParser::Kind::Request);
            HttpParseStatus status = HttpParseStatus::NeedMore;
            char buf[4096] = {};

            try
            {
                while (status == HttpParseStatus::NeedMore)
                {
                    const std::size_t n = connection->ReceiveSome(buf, sizeof(buf));
                    if (n == 0)
                        break; // 对端主动关了

                    status = parser.Feed(buf, n);
                }
            }
            catch (const std::exception &e)
            {
                // 超时 / 连接坏了：丢掉这个客户端，服务器继续跑
                CORE_WARN("丢弃连接: {}", e.what());
                connection->Close();
                continue;
            }

            Response response;
            std::string method = "-";
            std::string url = "-";
            std::string sayText; // 本次如果是发言，留一份用来打日志

            if (status == HttpParseStatus::Complete)
            {
                const Request &request = parser.GetRequest();
                method = MethodToString(request.method);
                url = request.target;

                if (url.rfind("/send", 0) == 0 && request.body.Size > 0 && request.body.Data != nullptr)
                    sayText.assign(reinterpret_cast<const char *>(request.body.Data),
                                   static_cast<std::size_t>(request.body.Size));

                response = Chat::Handle(request, room, roomPage);
            }
            else
            {
                SetHeader(response.headers, "Connection", "close");
                response.status = 400;
                SetText(response, status == HttpParseStatus::Error ? parser.GetError()
                                                                   : "incomplete request");
            }

            const std::string wire = ToBytes(response);

            try
            {
                connection->SendAll(wire.data(), wire.size());
            }
            catch (const std::exception &e)
            {
                CORE_WARN("发送失败: {}", e.what());
            }

            connection->Close();

            // 日志按「谁 + 路径」打。
            // 不能打完整 url：里面的 name 是百分号编码的（%E5%B0%8F%E6%98%8E），看不懂。
            // 也不能靠客户端地址：Connection: close 下端口每次都变。
            const std::string path = url.substr(0, url.find('?'));
            std::string who = Chat::QueryParam(url, "name");
            if (who.empty())
                who = "-";

            // 拉消息时把游标也打出来：排查"为什么没收到新消息"就看它
            std::string detail;
            if (path == "/messages")
            {
                const std::string since = Chat::QueryParam(url, "since");
                detail = "?since=" + (since.empty() ? "0" : since);
            }

            CORE_INFO("[{}] {} {}{} -> {} {}B", who, method, path, detail, response.status, wire.size());

            // 业务日志：真的说的话，单独一行。光看 POST /send 200 是不知道内容的。
            if (!sayText.empty() && response.status == 200)
                CORE_INFO("  {}: {}", who, Ellipsis(sayText));
        }
    }
    catch (const std::exception &e)
    {
        CORE_FATAL("启动失败: {}", e.what());
        return 1;
    }
}
