#include "router.h"

#include <string>
#include <string_view>

namespace X_Y::Chat
{
    namespace
    {
        using namespace X_Y;

        constexpr std::size_t MaxTextBytes = 2000; // 一条消息最多 2000 字节

        std::string PathOf(const std::string &url)
        {
            const std::size_t mark = url.find('?');
            return mark == std::string::npos ? url : url.substr(0, mark);
        }

        int HexValue(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        // 浏览器用 encodeURIComponent 编码，中文名会变成 %E5%B0%8F%E6%98%8E。
        // 不解码的话，两个"小明"会被当成不同名字，重名检查就失效了。
        std::string UrlDecode(const std::string &text)
        {
            std::string out;
            out.reserve(text.size());

            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const char c = text[i];

                if (c == '+')
                {
                    out += ' ';
                    continue;
                }

                if (c == '%' && i + 2 < text.size())
                {
                    const int high = HexValue(text[i + 1]);
                    const int low = HexValue(text[i + 2]);
                    if (high >= 0 && low >= 0)
                    {
                        out += static_cast<char>(high * 16 + low);
                        i += 2;
                        continue;
                    }
                }

                out += c;
            }
            return out;
        }

        std::string QueryParamRaw(const std::string &target, const std::string &key)
        {
            const std::size_t mark = target.find('?');
            if (mark == std::string::npos)
                return {};

            const std::string_view query(target.data() + mark + 1, target.size() - mark - 1);
            const std::string needle = key + "=";

            std::size_t pos = 0;
            while (pos <= query.size())
            {
                const std::size_t end = query.find('&', pos);
                const std::string_view pair =
                    query.substr(pos, end == std::string_view::npos ? std::string_view::npos : end - pos);

                if (pair.size() >= needle.size() && pair.compare(0, needle.size(), needle) == 0)
                    return UrlDecode(std::string(pair.substr(needle.size())));

                if (end == std::string_view::npos)
                    break;
                pos = end + 1;
            }
            return {};
        }

        int IntParam(const std::string &target, const std::string &key)
        {
            const std::string text = QueryParamRaw(target, key);
            int value = 0;
            for (const char c : text)
            {
                if (c < '0' || c > '9')
                    break;
                value = value * 10 + (c - '0');
            }
            return value;
        }

        const char *ReasonOf(JoinResult result)
        {
            switch (result)
            {
            case JoinResult::Taken: return "taken";
            case JoinResult::Empty: return "empty";
            case JoinResult::TooLong: return "toolong";
            default: return "ok";
            }
        }

        void Json(Response &response, const std::string &json, int status = 200)
        {
            response.status = status;
            SetText(response, json, "application/json; charset=utf-8");
        }
    } // namespace

    std::string QueryParam(const std::string &target, const std::string &key)
    {
        return QueryParamRaw(target, key);
    }

    Response Handle(const Request &request, Room &room, const XPath &roomPage)
    {
        Response response;
        SetHeader(response.headers, "Cache-Control", "no-store"); // 改完 HTML 刷新就生效
        SetHeader(response.headers, "Connection", "close");       // 本层不做 keep-alive

        const std::string &target = request.target;
        const std::string path = PathOf(target);

        // 每个请求顺手清理过期名字：不依赖浏览器通知，也不需要定时器
        room.Sweep();

        // ── 页面 ──
        if (path == "/" || path == "/index.html")
        {
            Buffer page = roomPage.ReadBinary(); // 每个请求重读：改完刷新就生效
            if (page.Size == 0)
            {
                response.status = 500;
                SetText(response, "cannot read page: " + roomPage.Path().string());
                return response;
            }

            SetHeader(response.headers, "Content-Type", "text/html; charset=utf-8");
            response.body = std::move(page);
            return response;
        }

        // ── 占名字 ──
        if (path == "/join")
        {
            const std::string name = QueryParam(target, "name");
            const JoinResult result = room.Join(name);

            if (result == JoinResult::Ok)
            {
                Json(response, "{\"ok\":true}");
            }
            else
            {
                // 409 Conflict：这个名字正在被用
                Json(response, std::string("{\"ok\":false,\"reason\":\"") + ReasonOf(result) + "\"}", 409);
            }
            return response;
        }

        // ── 释放名字 ──
        if (path == "/leave")
        {
            room.Leave(QueryParam(target, "name"));
            Json(response, "{\"ok\":true}");
            return response;
        }

        // ── 发言 ──
        if (path == "/send")
        {
            const std::string name = QueryParam(target, "name");

            std::string text;
            if (request.body.Size > 0 && request.body.Data != nullptr)
                text.assign(reinterpret_cast<const char *>(request.body.Data),
                            static_cast<std::size_t>(request.body.Size));

            if (text.empty())
            {
                Json(response, "{\"ok\":false,\"reason\":\"empty\"}", 400);
            }
            else if (text.size() > MaxTextBytes)
            {
                Json(response, "{\"ok\":false,\"reason\":\"toolong\"}", 413);
            }
            else if (!room.Say(name, text))
            {
                // 名字没占（前端 bug 或直接 curl）
                Json(response, "{\"ok\":false,\"reason\":\"not_joined\"}", 403);
            }
            else
            {
                Json(response, "{\"ok\":true}");
            }
            return response;
        }

        // ── 收消息 ──
        if (path == "/messages")
        {
            // 轮询即续租：只要页面开着，名字就不会过期
            const std::string name = QueryParam(target, "name");
            if (!name.empty())
                room.Touch(name);

            const int since = IntParam(target, "since");
            Json(response, room.MessagesSince(since));
            return response;
        }

        // ── 在线人数 ──
        // 统计的是"当前被占用的名字数"。名字没释放的人（崩溃/断网）会算在里面。
        if (path == "/online")
        {
            Json(response, "{\"count\":" + std::to_string(room.NameCount()) + "}");
            return response;
        }

        response.status = 404;
        SetText(response, "404 not found: " + path);
        return response;
    }
}
