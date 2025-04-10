/*
 * Bg770aHardwareRelatedCommands.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef BG770AHARDWARERELATEDCOMMANDS_HPP
#define BG770AHARDWARERELATEDCOMMANDS_HPP

#include <ctime>
#include "internal/Misc.hpp"
#include "WioCellularResult.hpp"

namespace wiocellular
{
    namespace module
    {
        namespace bg770a
        {
            namespace commands
            {

                /**
                 * @~Japanese
                 * @brief Quectel BG770Aモジュールのハードウェア関連コマンド
                 *
                 * @tparam MODULE モジュールのクラス
                 *
                 * Quectel BG770Aモジュールのハードウェア関連コマンドです。
                 */
                template <typename MODULE>
                class Bg770aHardwareRelatedCommands
                {
                public:
                    /**
                     * @~Japanese
                     * @brief 日時を取得
                     *
                     * @param [out] t モジュールのRTCの日時。GMT。
                     * @param [out] diff 時差[時間]。nullptrを指定すると値を代入しません。
                     * @return 実行結果。
                     *
                     * モジュールのRTCの日時を取得します。
                     *
                     * > BG77xA-GL&BG95xA-GL AT Commands Manual @n
                     * > 9.2. AT+CCLK Clock
                     */
                    WioCellularResult getClock(time_t *t, int *diff)
                    {
                        assert(t);

                        return static_cast<MODULE &>(*this).queryCommand(
                            "AT+CCLK?", [t, diff](const std::string &response) -> bool
                            {
                                std::string responseParameter;
                                if (internal::stringStartsWith(response, "+CCLK: ", &responseParameter))
                                {
                                    if (responseParameter[0] != '"') return false;
                                    if (responseParameter[3] != '/') return false;
                                    if (responseParameter[6] != '/') return false;
                                    if (responseParameter[9] != ',') return false;
                                    if (responseParameter[12] != ':') return false;
                                    if (responseParameter[15] != ':') return false;
                                    if (responseParameter[21] != '"') return false;

                                    const int yearOffset = std::stoi(&responseParameter[1]);
                                    const int quarterDiff = std::stoi(&responseParameter[18]);

                                    struct tm tim;
                                    tim.tm_year = (yearOffset >= 80 ? 1900 : 2000) + yearOffset - 1900;
                                    tim.tm_mon = std::stoi(&responseParameter[4]) - 1;
                                    tim.tm_mday = std::stoi(&responseParameter[7]);
                                    tim.tm_hour = std::stoi(&responseParameter[10]);
                                    tim.tm_min = std::stoi(&responseParameter[13]);
                                    tim.tm_sec = std::stoi(&responseParameter[16]);
                                    tim.tm_wday = 0;
                                    tim.tm_yday = 0;
                                    tim.tm_isdst = 0;

                                    *t = mktime(&tim);
                                    if (diff) *diff = quarterDiff / 4;

                                    return true;
                                }
                                return false; },
                            300);
                    }
                };

            }
        }
    }
}

#endif // BG770AHARDWARERELATEDCOMMANDS_HPP
