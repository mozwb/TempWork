#pragma once

#include "Network/http.h"
#include "IO/Files.h"
#include "room.h"

namespace X_Y::Chat
{
    // 从 target 里取查询参数（已 URL 解码）。没有就返回空串。
    // 放在公开处，是因为 main 打日志也要用它把名字解出来。
    std::string QueryParam(const std::string &target, const std::string &key);

    // 一个请求 -> 一个响应。
    // 签名里没有 socket、没有线程：方便单测，也方便以后换 IoContext。
    X_Y::Response Handle(const X_Y::Request &request, Room &room, const X_Y::XPath &roomPage);
}
