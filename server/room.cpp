#include "room.h"

#include <ctime>
#include <utility>

namespace X_Y::Chat
{
    namespace
    {
        std::string NowClock()
        {
            const std::time_t now = std::time(nullptr);
            const std::tm *local = std::localtime(&now);
            if (local == nullptr)
                return "--:--";

            char buf[16] = {};
            std::strftime(buf, sizeof(buf), "%H:%M", local);
            return buf;
        }

        // 按 UTF-8 的"字"数算长度：中文一个字 3 字节，不能拿 size() 当长度
        std::size_t CountCodePoints(const std::string &text)
        {
            std::size_t count = 0;
            for (const char c : text)
            {
                // 0b10xxxxxx 是续字节，不单独算一个字
                if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
                    ++count;
            }
            return count;
        }

        // JSON 字符串里的 " \ 换行 必须转义，否则前端 JSON.parse 直接抛
        std::string JsonEscape(const std::string &text)
        {
            std::string out;
            out.reserve(text.size() + 8);
            for (const char c : text)
            {
                switch (c)
                {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
                }
            }
            return out;
        }
    } // namespace

    Room::Room(int nameLeaseSeconds)
        : m_Lease(nameLeaseSeconds < 1 ? 1 : nameLeaseSeconds)
    {
    }

    JoinResult Room::Join(const std::string &name)
    {
        if (name.empty())
            return JoinResult::Empty;

        if (CountCodePoints(name) > MaxNameLength)
            return JoinResult::TooLong;

        const std::time_t now = std::time(nullptr);
        const auto found = m_Names.find(name);

        // 有人正在用，且还没过期
        if (found != m_Names.end() && now - found->second <= m_Lease)
            return JoinResult::Taken;

        // 不存在，或者上一个占用已经过期 —— 直接接管
        m_Names[name] = now;
        return JoinResult::Ok;
    }

    bool Room::Touch(const std::string &name)
    {
        const auto found = m_Names.find(name);
        if (found == m_Names.end())
            return false;

        found->second = std::time(nullptr);
        return true;
    }

    void Room::Leave(const std::string &name)
    {
        m_Names.erase(name);
    }

    bool Room::Say(const std::string &name, const std::string &text)
    {
        // 没占名字就发不了；顺便续租
        if (!Touch(name))
            return false;

        Message message;
        message.id = m_NextMessageId++;
        message.name = name;
        message.text = text;
        message.time = NowClock();

        m_Messages.push_back(std::move(message));
        if (m_Messages.size() > MaxMessages)
            m_Messages.erase(m_Messages.begin());

        return true;
    }

    std::string Room::MessagesSince(int sinceId) const
    {
        std::string json = "[";
        bool first = true;

        for (const Message &message : m_Messages)
        {
            if (message.id <= sinceId)
                continue;

            if (!first)
                json += ',';
            first = false;

            json += "{\"id\":";
            json += std::to_string(message.id);
            json += ",\"name\":\"";
            json += JsonEscape(message.name);
            json += "\",\"text\":\"";
            json += JsonEscape(message.text);
            json += "\",\"time\":\"";
            json += JsonEscape(message.time);
            json += "\"}";
        }

        json += ']';
        return json;
    }

    int Room::Sweep()
    {
        const std::time_t now = std::time(nullptr);
        int removed = 0;

        for (auto it = m_Names.begin(); it != m_Names.end();)
        {
            if (now - it->second > m_Lease)
            {
                it = m_Names.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }

        return removed;
    }
}
