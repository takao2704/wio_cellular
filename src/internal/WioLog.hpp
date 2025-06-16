/*
 * WioLog.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef WIOLOG_HPP
#define WIOLOG_HPP

#include <string>

namespace wiocellular::internal
{

    enum class WioLogType
    {
        INFO,
        WARNING,
        AT_CMD,
        AT_ECO,
        AT_INF,
        AT_FRC,
        AT_URC,
        AT_UNK,
    };

#if CFG_LOGGER != 3

#if 0 // 0: For plain text output, 1: For colored output

    template <class... Args>
    static void WioLog(WioLogType type, const char *format, Args... args)
    {
        const char *buf;
        std::string str;
        if constexpr (sizeof...(Args) == 0)
        {
            buf = format;
        }
        else
        {
            const size_t len = snprintf(nullptr, 0, format, args...);
            str.resize(len + 1);
            snprintf(str.data(), len + 1, format, args...);
            str.resize(len);
            buf = str.c_str();
        }

        switch (type)
        {
        case WioLogType::INFO:
            printf("\033[33mINFO: %s\033[0m\n", buf);
            break;
        case WioLogType::WARNING:
            printf("\033[35mWARNING: %s\033[0m\n", buf);
            break;
        case WioLogType::AT_CMD:
            printf("\033[32;1mCMD> %s\033[0m\n", buf);
            break;
        case WioLogType::AT_ECO:
            printf("\033[32mECO> %s\033[0m\n", buf);
            break;
        case WioLogType::AT_INF:
            printf("\033[32mINF> %s\033[0m\n", buf);
            break;
        case WioLogType::AT_FRC:
            printf("\033[32mFRC> %s\033[0m\n", buf);
            break;
        case WioLogType::AT_URC:
            printf("\033[32;1;7mURC> %s\033[0m\n", buf);
            break;
        case WioLogType::AT_UNK:
            printf("\033[31;1munk> %s\033[0m\n", buf);
            break;
        default:
            printf("%s\n", buf);
            break;
        }
    }

#else

    template <class... Args>
    static void WioLog(WioLogType type, const char *format, Args... args)
    {
        const char *buf;
        std::string str;
        if constexpr (sizeof...(Args) == 0)
        {
            buf = format;
        }
        else
        {
            const size_t len = snprintf(nullptr, 0, format, args...);
            str.resize(len + 1);
            snprintf(str.data(), len + 1, format, args...);
            str.resize(len);
            buf = str.c_str();
        }

        switch (type)
        {
        case WioLogType::INFO:
            printf("INFO: %s\n", buf);
            break;
        case WioLogType::WARNING:
            printf("WARNING: %s\n", buf);
            break;
        case WioLogType::AT_CMD:
            printf("CMD> %s\n", buf);
            break;
        case WioLogType::AT_ECO:
            printf("ECO> %s\n", buf);
            break;
        case WioLogType::AT_INF:
            printf("INF> %s\n", buf);
            break;
        case WioLogType::AT_FRC:
            printf("FRC> %s\n", buf);
            break;
        case WioLogType::AT_URC:
            printf("URC> %s\n", buf);
            break;
        case WioLogType::AT_UNK:
            printf("unk> %s\n", buf);
            break;
        default:
            printf("%s\n", buf);
            break;
        }
    }

#endif

#else

    template <class... Args>
    static void WioLog(WioLogType type, const char *format, Args... args)
    {
    }

#endif

}

#endif // WIOLOG_HPP
