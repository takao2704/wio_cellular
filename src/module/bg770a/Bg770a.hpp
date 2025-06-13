/*
 * Bg770a.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef BG770A_HPP
#define BG770A_HPP

#include "commands/Bg770aExtendedConfigurationCommands.hpp"
#include "commands/Bg770aGeneralCommands.hpp"
#include "commands/Bg770aNetworkServiceCommands.hpp"
#include "commands/Bg770aPacketDomainCommands.hpp"
#include "commands/Bg770aSimRelatedCommands.hpp"
#include "commands/Bg770aTcpipCommands2.hpp"
#include "commands/Bg770aHardwareRelatedCommands.hpp"
#include "commands/Bg770aSerialInterfaceControlCommands.hpp"

#include "module/at_client/AtClient.hpp"
#include "internal/Misc.hpp"
#include "WioCellularResult.hpp"

#if 0
#define COLOR_CMD "\033[32;1m"
#define COLOR_ECO "\033[32m"
#define COLOR_INF "\033[32m"
#define COLOR_FRC "\033[32m"
#define COLOR_URC "\033[32;1;7m"
#define COLOR_UNK "\033[31;1m"
#define COLOR_RESET "\033[0m"
#else
#define COLOR_CMD ""
#define COLOR_ECO ""
#define COLOR_INF ""
#define COLOR_FRC ""
#define COLOR_URC ""
#define COLOR_UNK ""
#define COLOR_RESET ""
#endif

namespace wiocellular
{
    namespace module
    {
        namespace bg770a
        {

            /**
             * @~Japanese
             * @brief Quectel BG770Aモジュール
             *
             * @tparam INTERFACE インターフェースのクラス
             *
             * Quectel BG770Aモジュールのクラスです。
             */
            template <typename INTERFACE>
            class Bg770a : public at_client::AtClient<Bg770a<INTERFACE>>,
                           public commands::Bg770aExtendedConfigurationCommands<Bg770a<INTERFACE>>,
                           public commands::Bg770aGeneralCommands<Bg770a<INTERFACE>>,
                           public commands::Bg770aNetworkServiceCommands<Bg770a<INTERFACE>>,
                           public commands::Bg770aPacketDomainCommands<Bg770a<INTERFACE>>,
                           public commands::Bg770aSimRelatedCommands<Bg770a<INTERFACE>>,
                           public commands::Bg770aTcpipCommands2<Bg770a<INTERFACE>>,
                           public commands::Bg770aHardwareRelatedCommands<Bg770a<INTERFACE>>,
                           public commands::Bg770aSerialInterfaceControlCommands<Bg770a<INTERFACE>>
            {
                friend class at_client::AtClient<Bg770a<INTERFACE>>;

            private:
                INTERFACE &Interface_;

            public:
                /**
                 * @~Japanese
                 * @brief ATコマンドのタイムアウト時間下限[ミリ秒]
                 */
                int commandTimeoutMin;

                /**
                 * @~Japanese
                 * @brief ATコマンドのエコー待ちタイムアウト時間[ミリ秒]
                 */
                int commandEchoTimeout;

                /**
                 * @~Japanese
                 * @brief コンストラクタ
                 *
                 * @param [in] interface インターフェースのインスタンス。
                 *
                 * コンストラクタ。
                 * interfaceにインターフェースのインスタンスを指定します。
                 */
                explicit Bg770a(INTERFACE &interface) : at_client::AtClient<Bg770a<INTERFACE>>{},
                                                        Interface_{interface},
                                                        commandTimeoutMin{10000},
                                                        commandEchoTimeout{60000}
                {
                    at_client::AtClient<Bg770a<INTERFACE>>::registerUrcHandler([](const std::string &response) -> bool
                                                                               {
                                                                                    printf(COLOR_URC "URC> %s" COLOR_RESET "\n", response.c_str());
                                                                                    return false; });
                }

                /**
                 * @~Japanese
                 * @brief インターフェースを取得
                 *
                 * @return インターフェースのインスタンス。
                 *
                 * インターフェースを取得します。
                 */
                INTERFACE &getInterface()
                {
                    return Interface_;
                }

                /**
                 * @~Japanese
                 * @brief 実行コマンドを実行
                 *
                 * @param [in] command コマンド。
                 * @param [in] timeout タイムアウト時間[ミリ秒]。
                 * @return 実行結果。
                 *
                 * 実行コマンドを実行します。
                 * 永久に待機したいときはtimeoutに-1を指定します。
                 */
                WioCellularResult executeCommand(const std::string &command, int timeout)
                {
                    printf(COLOR_CMD "CMD> %s" COLOR_RESET "\n", command.c_str());
                    auto start = millis();
                    if (!at_client::AtClient<Bg770a<INTERFACE>>::writeAndWaitCommand(command, std::max(commandEchoTimeout, commandTimeoutMin)))
                    {
                        return WioCellularResult::WaitCommandTimeout;
                    }
                    printf(COLOR_ECO "ECO> %s ... %lu[ms]" COLOR_RESET "\n", command.c_str(), millis() - start);
                    start = millis();

                    std::string response;
                    while (true)
                    {
                        if ((response = at_client::AtClient<Bg770a<INTERFACE>>::readResponse(std::max(timeout, commandTimeoutMin))).empty())
                        {
                            return WioCellularResult::ReadResponseTimeout;
                        }

                        // Final Result Code
                        if (response == "OK")
                        {
                            printf(COLOR_FRC "FRC> %s ... %lu[ms]" COLOR_RESET "\n", response.c_str(), millis() - start);
                            break;
                        }
                        if (response == "ERROR" || internal::stringStartsWith(response, "+CME ERROR: ") || internal::stringStartsWith(response, "+CMS ERROR: "))
                        {
                            printf(COLOR_FRC "FRC> %s ... %lu[ms]" COLOR_RESET "\n", response.c_str(), millis() - start);
                            return WioCellularResult::CommandRejected;
                        }

                        // Unknown
                        printf(COLOR_UNK "unk> %s" COLOR_RESET "\n", response.c_str());
                    }

                    return WioCellularResult::Ok;
                }

                /**
                 * @~Japanese
                 * @brief 問い合わせコマンドを実行
                 *
                 * @param [in] command コマンド。
                 * @param [in] informationTextHandler information textのハンドラ。
                 * @param [in] timeout タイムアウト時間[ミリ秒]。
                 * @return 実行結果。
                 *
                 * 問い合わせコマンドを実行します。
                 * 永久に待機したいときはtimeoutに-1を指定します。
                 * informaton textを読み込んだときはinformationTextHandlerを呼び出します。
                 */
                WioCellularResult queryCommand(const std::string &command, const std::function<bool(const std::string &response)> &informationTextHandler, int timeout)
                {
                    printf(COLOR_CMD "CMD> %s" COLOR_RESET "\n", command.c_str());
                    auto start = millis();
                    if (!at_client::AtClient<Bg770a<INTERFACE>>::writeAndWaitCommand(command, std::max(commandEchoTimeout, commandTimeoutMin)))
                    {
                        return WioCellularResult::WaitCommandTimeout;
                    }
                    printf(COLOR_ECO "ECO> %s ... %lu[ms]" COLOR_RESET "\n", command.c_str(), millis() - start);
                    start = millis();

                    std::string response;
                    while (true)
                    {
                        if ((response = at_client::AtClient<Bg770a<INTERFACE>>::readResponse(std::max(timeout, commandTimeoutMin))).empty())
                        {
                            return WioCellularResult::ReadResponseTimeout;
                        }

                        // Final Result Code
                        if (response == "OK")
                        {
                            printf(COLOR_FRC "FRC> %s ... %lu[ms]" COLOR_RESET "\n", response.c_str(), millis() - start);
                            break;
                        }
                        if (response == "ERROR" || internal::stringStartsWith(response, "+CME ERROR: ") || internal::stringStartsWith(response, "+CMS ERROR: "))
                        {
                            printf(COLOR_FRC "FRC> %s ... %lu[ms]" COLOR_RESET "\n", response.c_str(), millis() - start);
                            return WioCellularResult::CommandRejected;
                        }

                        // Information text
                        if (informationTextHandler && informationTextHandler(response))
                        {
                            printf(COLOR_INF "INF> %s" COLOR_RESET "\n", response.c_str());
                            continue;
                        }

                        // Unknown
                        printf(COLOR_UNK "unk> %s" COLOR_RESET "\n", response.c_str());
                    }

                    return WioCellularResult::Ok;
                }

                /**
                 * @~Japanese
                 * @brief 送信コマンドを実行
                 *
                 * @param [in] command コマンド。
                 * @param [in] informationTextHandler information textのハンドラ。
                 * @param [in] timeout タイムアウト時間[ミリ秒]。
                 * @return 実行結果。
                 *
                 * 問い合わせコマンドを実行します。
                 * 永久に待機したいときはtimeoutに-1を指定します。
                 * informaton textを読み込んだときはinformationTextHandlerを呼び出します。
                 */
                WioCellularResult sendCommand(const std::string &command, std::function<bool(const std::string &response)> informationTextHandler, int timeout)
                {
                    printf(COLOR_CMD "CMD> %s" COLOR_RESET "\n", command.c_str());
                    auto start = millis();
                    if (!at_client::AtClient<Bg770a<INTERFACE>>::writeAndWaitCommand(command, std::max(commandEchoTimeout, commandTimeoutMin)))
                    {
                        return WioCellularResult::WaitCommandTimeout;
                    }
                    printf(COLOR_ECO "ECO> %s ... %lu[ms]" COLOR_RESET "\n", command.c_str(), millis() - start);
                    start = millis();

                    std::string response;
                    while (true)
                    {
                        if ((response = at_client::AtClient<Bg770a<INTERFACE>>::readResponse(timeout, [](const std::string &response) -> bool
                                                                                             { return response == "> "; }))
                                .empty())
                        {
                            return WioCellularResult::ReadResponseTimeout;
                        }

                        // Final Result Code
                        if (response == "SEND OK")
                        {
                            printf(COLOR_FRC "FRC> %s ... %lu[ms]" COLOR_RESET "\n", response.c_str(), millis() - start);
                            break;
                        }
                        if (response == "ERROR" || response == "SEND FAIL")
                        {
                            printf(COLOR_FRC "FRC> %s ... %lu[ms]" COLOR_RESET "\n", response.c_str(), millis() - start);
                            return WioCellularResult::CommandRejected;
                        }

                        // Information text
                        if (informationTextHandler && informationTextHandler(response))
                        {
                            printf(COLOR_INF "INF> %s" COLOR_RESET "\n", response.c_str());
                            continue;
                        }

                        // Unknown
                        printf(COLOR_UNK "unk> %s" COLOR_RESET "\n", response.c_str());
                    }

                    return WioCellularResult::Ok;
                }

                /**
                 * @~Japanese
                 * @brief 電源をオン
                 *
                 * @param [in] timeout タイムアウト時間[ミリ秒]。
                 * @return 実行結果。
                 *
                 * 電源をオンします。
                 * 永久に待機したいときはtimeoutに-1を指定します。
                 * 処理完了までに10秒程度かかります．
                 */
                WioCellularResult powerOn(int timeout)
                {
                    bool appRdy = false;
                    {
                        const auto handler = at_client::AtClient<Bg770a<INTERFACE>>::registerUrcHandler2([&appRdy](const std::string &response) -> bool
                                                                                                         {
                                                                                                            if (response == "APP RDY")
                                                                                                            {
                                                                                                                appRdy = true;
                                                                                                                return true;
                                                                                                            }
                                                                                                            return false; });

                        if (!getInterface().isActive())
                        {
                            getInterface().powerOn();

                            wiocellular::internal::CountdownTimer timer{1000};
                            while (!getInterface().isActive() && !timer.isTimeout())
                            {
                                at_client::AtClient<Bg770a<INTERFACE>>::doWork(10); // Spin
                            }
                            if (!getInterface().isActive())
                            {
#if defined(BOARD_VERSION_ES2)
                                delay(2 + 2);
                                digitalWrite(PIN_VSYS_3V3_ENABLE, LOW);
                                delay(100 + 2);
                                digitalWrite(PIN_VSYS_3V3_ENABLE, HIGH);
                                delay(2 + 2);
                                getInterface().powerOn();
                                if (!getInterface().isActive())
                                {
                                    printf("---> Interface is not active when powerOn()\n");
                                    return WioCellularResult::NotActivate;
                                }
#elif defined(BOARD_VERSION_1_0)
                                printf("---> Interface is not active when powerOn()\n");
                                return WioCellularResult::NotActivate;
#else
#error "Unknown board version"
#endif
                            }
                        }
                        else
                        {
#if defined(BOARD_VERSION_ES2)
                            delay(2 + 2);
                            digitalWrite(PIN_VSYS_3V3_ENABLE, LOW);
                            delay(100 + 2);
                            digitalWrite(PIN_VSYS_3V3_ENABLE, HIGH);
                            delay(2 + 2);
                            getInterface().powerOn();
                            if (!getInterface().isActive())
                            {
                                printf("---> Interface is not active when powerOn()\n");
                                return WioCellularResult::NotActivate;
                            }
#elif defined(BOARD_VERSION_1_0)
                            getInterface().reset();
#else
#error "Unknown board version"
#endif
                        }

                        if (!at_client::AtClient<Bg770a<INTERFACE>>::doWork(timeout, [&appRdy]
                                                                            { return appRdy; }))
                        {
                            getInterface().reset();

                            if (!at_client::AtClient<Bg770a<INTERFACE>>::doWork(timeout, [&appRdy]
                                                                                { return appRdy; }))
                            {
                                return WioCellularResult::RdyTimeout;
                            }
                        }
                    }

                    WioCellularResult result;

                    // Enable Hardware Flow Control
                    int dte;
                    int dce;
                    if ((result = commands::Bg770aSerialInterfaceControlCommands<Bg770a<INTERFACE>>::getFlowControl(&dte, &dce)) != WioCellularResult::Ok)
                    {
                        return result;
                    }
                    if (dte != 2 || dce != 2)
                    {
                        if ((result = commands::Bg770aSerialInterfaceControlCommands<Bg770a<INTERFACE>>::setFlowControl(2, 2)) != WioCellularResult::Ok)
                        {
                            return result;
                        }
                        at_client::AtClient<Bg770a<INTERFACE>>::doWorkUntil(2200);
                    }

                    return WioCellularResult::Ok;
                }

                /**
                 * @~Japanese
                 * @brief 電源をオフ
                 *
                 * @return 実行結果。
                 *
                 * 電源をオフします。
                 */
                WioCellularResult powerOff()
                {
                    getInterface().powerOff();

                    return WioCellularResult::Ok;
                }
            };

        }
    }
}

#endif // BG770A_HPP
