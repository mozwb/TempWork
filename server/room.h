#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace X_Y::Chat
{
    struct Message
    {
        int id = 0;
        std::string name; // 发消息时用的名字，同时也是身份
        std::string text;
        std::string time;
    };

    enum class JoinResult
    {
        Ok,
        Empty,   // 名字为空
        TooLong, // 超过 MaxNameLength 个字
        Taken    // 有人正在用这个名字
    };

    // 一个聊天室：消息缓冲 + 名字租约表。
    //
    // 设计约定：
    //   1. 身份就是名字，没有 userId。重名被 Join 挡住，所以名字可靠。
    //   2. 名字是"租约"：任何带名字的请求都会续期（前端每 1.5 秒轮询一次，
    //      所以页面开着就不会过期）。超过 m_Lease 秒没有任何请求，名字自动释放。
    //      这样关页面、崩溃、断网，最终都能把名字还回来 —— 不依赖浏览器通知。
    //   3. 租约要大于浏览器后台标签页的定时器节流（约 60 秒），默认 120 秒。
    //   4. 名字大小写敏感（"Tom" 和 "tom" 是两个名字）。
    //   5. 单线程使用，不加锁。将来上反应器/多线程要补 Mutex。
    class Room
    {
    public:
        static constexpr std::size_t MaxMessages = 200;  // 只留最近 200 条
        static constexpr std::size_t MaxNameLength = 16; // 按"字"数算，不是字节
        static constexpr int DefaultNameLease = 120;     // 名字多久没动静就释放

        explicit Room(int nameLeaseSeconds = DefaultNameLease);

        // 占一个名字（过期的旧占用会被顶掉）
        JoinResult Join(const std::string &name);

        // 续租；名字不存在返回 false
        bool Touch(const std::string &name);

        // 主动释放
        void Leave(const std::string &name);

        // 说一句话。名字没被占用则失败（防前端 bug 或直接 curl）
        bool Say(const std::string &name, const std::string &text);

        // 比 sinceId 新的消息，拼成 JSON 数组
        std::string MessagesSince(int sinceId) const;

        // 释放所有过期名字，返回释放的个数
        int Sweep();

        std::size_t NameCount() const { return m_Names.size(); }
        std::size_t MessageCount() const { return m_Messages.size(); }

    private:
        int m_Lease;
        int m_NextMessageId = 1;
        std::unordered_map<std::string, std::time_t> m_Names; // 名字 -> 最后活跃时间
        std::vector<Message> m_Messages;                      // 满了丢最旧的那条
    };
}
