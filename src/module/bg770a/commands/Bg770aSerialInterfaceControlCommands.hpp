/*
 * Bg770aSerialInterfaceControlCommands.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef BG770ASERIALINTERFACECONTROLCOMMANDS_HPP
#define BG770ASERIALINTERFACECONTROLCOMMANDS_HPP

#include "module/at_client/AtParameterParser.hpp"
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
                 * @brief Quectel BG770Aモジュールのシリアルインターフェース制御コマンド
                 *
                 * @tparam MODULE モジュールのクラス
                 *
                 * Quectel BG770Aモジュールのシリアルインターフェース制御コマンドです。
                 */
                template <typename MODULE>
                class Bg770aSerialInterfaceControlCommands
                {
                public:
                    /**
                     * @~Japanese
                     * @brief シリアルポートのフロー制御を取得
                     *
                     * @param [out] dte DTEのフロー制御。nullptrを指定すると値を代入しません。
                     *   @arg -1: 無し
                     *   @arg 0: フロー制御無し
                     *   @arg 2: RTSフロー制御
                     * @param [out] dce DCEのフロー制御。nullptrを指定すると値を代入しません。
                     *   @arg -1: 無し
                     *   @arg 0: フロー制御無し
                     *   @arg 2: RTSフロー制御
                     * @return 実行結果。
                     *
                     * シリアルポートのフロー制御を取得します。
                     *
                     * > BG77xA-GL&BG95xA-GL AT Commands Manual @n
                     * > 3.3. AT+IFC Set TE-TA Local Flow Control
                     */
                    WioCellularResult getFlowControl(int *dte, int *dce)
                    {
                        if (dte)
                            *dte = -1;
                        if (dce)
                            *dce = -1;

                        return static_cast<MODULE &>(*this).queryCommand(
                            "AT+IFC?", [dte, dce](const std::string &response) -> bool
                            {
                                std::string responseParameter;
                                if (internal::stringStartsWith(response, "+IFC: ", &responseParameter))
                                {
                                    at_client::AtParameterParser parser{responseParameter};
                                    if (parser.size() != 2) return false;
                                    if (dte) *dte = std::stoi(parser[0]);
                                    if (dce) *dce = std::stoi(parser[1]);
                                    return true;
                                }
                                return false; },
                            300);
                    }

                    /**
                     * @~Japanese
                     * @brief シリアルポートのフロー制御を設定
                     *
                     * @param [in] dte DTEのフロー制御。
                     *   @arg 0: フロー制御無し
                     *   @arg 2: RTSフロー制御
                     * @param [in] dce DCEのフロー制御。
                     *   @arg 0: フロー制御無し
                     *   @arg 2: RTSフロー制御
                     * @return 実行結果。
                     *
                     * シリアルポートのフロー制御を設定します。
                     *
                     * > BG77xA-GL&BG95xA-GL AT Commands Manual @n
                     * > 3.3. AT+IFC Set TE-TA Local Flow Control
                     */
                    WioCellularResult setFlowControl(int dte, int dce)
                    {
                        assert(dte == 0 || dte == 2);
                        assert(dce == 0 || dce == 2);

                        return static_cast<MODULE &>(*this).executeCommand(internal::stringFormat("AT+IFC=%d,%d", dte, dce), 300);
                    }
                };

            }
        }
    }
}

#endif // BG770ASIMRELATEDCOMMANDS_HPP
