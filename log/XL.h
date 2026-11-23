#pragma once

#include "Log/Log.h"

// 仅供DEBUG时测试用

namespace X_Y
{
    template <typename T>
    using LogBase = LogConfigure::LogConfigure<T>;
    using LogConfigure::DEVICE;
    using LogConfigure::LFile;
#ifdef DEBUG
    // 建议一个类当作一个对象，否则可能模板会冲突
    struct LOG : public LogBase<LOG>
    {
        using LogBase<LOG>::LogBase;
        LOG(str name) : LogBase<LOG>(name)
        {
            this->add(new DEVICE());
            Trace::setModel("[日期][时间][等级]:[内容]");
            Debug::setModel("[日期][时间][等级]:[内容]");
            Info::setModel("[日期][时间][等级]:[内容]");
            Warn::setModel("[日期][时间][等级]:[内容]");
            Error::setModel("[日期][时间][等级]:[内容]");
            Fatal::setModel("[日期][时间][等级]:[内容]");
        }
        static LOG &Instance()
        {
            static LOG logger("log");
            return logger;
        }
    };
#endif
    struct CORELOG : public LogBase<CORELOG>
    {
        using LogBase<CORELOG>::LogBase;
        CORELOG(str name) : LogBase<CORELOG>(name)
        {
            this->add(new DEVICE());
            Trace::setModel("[日期][时间][等级]:[内容]");
            Debug::setModel("[日期][时间][等级]:[内容]");
            Info::setModel("[日期][时间][等级]:[内容]");
            Warn::setModel("[日期][时间][等级]:[内容]");
            Error::setModel("[日期][时间][等级]:[内容]");
            Fatal::setModel("[日期][时间][等级]:[内容]");
        }
        static CORELOG &Instance()
        {
            static CORELOG logger("Core");
            return logger;
        }
    };
}
// ═════════════════════════════════════════════════════════════
// 调用宏
// ═════════════════════════════════════════════════════════════
// 两个 logger 共用这一个底层宏：Log 是 LogConfigure 的成员模板，
// 等级类型就是继承下来的 CORELOG::Info / CORELOG::Warn 这些嵌套类型。
#define X_Y_LOGCALL(LoggerRef, LevelType, ...) \
    (LoggerRef).Log<LevelType>(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

// 正式日志：永远可用，不受 DEBUG 影响
#define CORE_TRACE(...) X_Y_LOGCALL(::X_Y::CORELOG::Instance(), ::X_Y::CORELOG::Trace, __VA_ARGS__)
#define CORE_DEBUG(...) X_Y_LOGCALL(::X_Y::CORELOG::Instance(), ::X_Y::CORELOG::Debug, __VA_ARGS__)
#define CORE_INFO(...) X_Y_LOGCALL(::X_Y::CORELOG::Instance(), ::X_Y::CORELOG::Info, __VA_ARGS__)
#define CORE_WARN(...) X_Y_LOGCALL(::X_Y::CORELOG::Instance(), ::X_Y::CORELOG::Warn, __VA_ARGS__)
#define CORE_ERROR(...) X_Y_LOGCALL(::X_Y::CORELOG::Instance(), ::X_Y::CORELOG::Error, __VA_ARGS__)
#define CORE_FATAL(...) X_Y_LOGCALL(::X_Y::CORELOG::Instance(), ::X_Y::CORELOG::Fatal, __VA_ARGS__)

// 调试日志：DEBUG 没定义时全部展开成空，调用点不用写 #ifdef
#ifdef DEBUG
#define X_TRACE(...) X_Y_LOGCALL(::X_Y::LOG::Instance(), ::X_Y::LOG::Trace, __VA_ARGS__)
#define X_DEBUG(...) X_Y_LOGCALL(::X_Y::LOG::Instance(), ::X_Y::LOG::Debug, __VA_ARGS__)
#define X_INFO(...) X_Y_LOGCALL(::X_Y::LOG::Instance(), ::X_Y::LOG::Info, __VA_ARGS__)
#define X_WARN(...) X_Y_LOGCALL(::X_Y::LOG::Instance(), ::X_Y::LOG::Warn, __VA_ARGS__)
#define X_ERROR(...) X_Y_LOGCALL(::X_Y::LOG::Instance(), ::X_Y::LOG::Error, __VA_ARGS__)
#define X_FATAL(...) X_Y_LOGCALL(::X_Y::LOG::Instance(), ::X_Y::LOG::Fatal, __VA_ARGS__)
#else
#define X_TRACE(...) ((void)0)
#define X_DEBUG(...) ((void)0)
#define X_INFO(...) ((void)0)
#define X_WARN(...) ((void)0)
#define X_ERROR(...) ((void)0)
#define X_FATAL(...) ((void)0)
#endif