/*
 * Bg770aNetwork.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef BG770ANETWORK_HPP
#define BG770ANETWORK_HPP

namespace wiocellular
{
    namespace network
    {

        /**
         * @~Japanese
         * @brief [Experimental] ネットワーク支援クラス
         *
         * ネットワークの操作を支援するクラスです。
         */
        class Bg770aNetwork
        {
        public:
            /**
             * @~Japanese
             * @brief ネットワーク探索のアクセステクノロジーと順序
             */
            enum class SearchAccessTechnology
            {
                /**
                 * @~Japanese
                 * @brief 未設定
                 */
                NOSET,
                /**
                 * @~Japanese
                 * @brief LTE-M
                 */
                LTEM,
                /**
                 * @~Japanese
                 * @brief NB-IoT
                 */
                NBIOT,
                /**
                 * @~Japanese
                 * @brief LTE-M->NB-IoT
                 */
                LTEM_NBIOT,
                /**
                 * @~Japanese
                 * @brief NB-IoT->LTE-M
                 */
                NBIOT_LTEM,
            };

            /**
             * @~Japanese
             * @brief ネットワーク状態
             */
            enum class NetworkState
            {
                /**
                 * @~Japanese
                 * @brief 未検索
                 */
                NotSearching,
                /**
                 * @~Japanese
                 * @brief 検索中
                 */
                Searching,
                /**
                 * @~Japanese
                 * @brief 接続済み
                 */
                Connected,
                /**
                 * @~Japanese
                 * @brief 拒否
                 */
                Denied,
                /**
                 * @~Japanese
                 * @brief 不明
                 */
                Unknown,
            };

        public:
            /**
             * @~Japanese
             * @brief NTTドコモのLTE-M周波数バンド
             */
            static constexpr char ALL_LTEM_BAND[] = "0x2000000000f0e189f";
            /**
             * @~Japanese
             * @brief NTTドコモのLTE-M周波数バンド
             */
            static constexpr char NTTDOCOMO_LTEM_BAND[] = "0xa040005";
            /**
             * @~Japanese
             * @brief KDDIのLTE-M周波数バンド
             */
            static constexpr char KDDI_LTEM_BAND[] = "0xa020005";

        public:
            /**
             * @~Japanese
             * @brief 異常終了ハンドラ
             */
            std::function<void(const char *file, int line)> abortHandler;

            /**
             * @~Japanese
             * @brief ネットワークの設定
             */
            struct
            {
                /**
                 * @~Japanese
                 * @brief ネットワーク探索のアクセステクノロジーと順序
                 */
                SearchAccessTechnology searchAccessTechnology;
                /**
                 * @~Japanese
                 * @brief LTE-M周波数バンド
                 *
                 * * "": 未設定
                 * * "0x1": LTE B1
                 * * "0x2": LTE B2
                 * * "0x4": LTE B3
                 * * "0x8": LTE B4
                 * * "0x10": LTE B5
                 * * "0x80": LTE B8
                 * * "0x800": LTE B12
                 * * "0x1000": LTE B13
                 * * "0x20000": LTE B18
                 * * "0x40000": LTE B19
                 * * "0x80000": LTE B20
                 * * "0x1000000": LTE B25
                 * * "0x2000000": LTE B26
                 * * "0x4000000": LTE B27
                 * * "0x8000000": LTE B28
                 * * "0x20000000000000000": LTE B66
                 */
                std::string ltemBand;
                /**
                 * @~Japanese
                 * @brief NB-IoT周波数バンド
                 *
                 * * "": 未設定
                 * * "0x1": LTE B1
                 * * "0x2": LTE B2
                 * * "0x4": LTE B3
                 * * "0x8": LTE B4
                 * * "0x10": LTE B5
                 * * "0x80": LTE B8
                 * * "0x800": LTE B12
                 * * "0x1000": LTE B13
                 * * "0x10000": LTE B17
                 * * "0x20000": LTE B18
                 * * "0x40000": LTE B19
                 * * "0x80000": LTE B20
                 * * "0x1000000": LTE B25
                 * * "0x8000000": LTE B28
                 * * "0x20000000000000000": LTE B66
                 */
                std::string nbiotBand;
                /**
                 * @~Japanese
                 * @brief PDPコンテキストID
                 */
                int pdpContextId;
                /**
                 * @~Japanese
                 * @brief APN(access point name)
                 */
                std::string apn;
            } config;

        private:
            int EpsRegistrationStatus_;
            std::unique_ptr<WioCellularModule::UrcHandler> UrcHandler_;
            bool BeginFirstCall_;

        private:
            void defaultAbortHandler(const char *file, int line)
            {
                Serial.print("ERROR: ");
                Serial.print(file);
                Serial.print(":");
                Serial.println(line);

                Serial.flush();
                abort();
            }

        public:
            /**
             * @~Japanese
             * @brief コンストラクタ
             *
             * コンストラクタ。
             */
            Bg770aNetwork(void)
                : abortHandler{nullptr},
                  config{SearchAccessTechnology::NOSET, "", "", 1, ""},
                  EpsRegistrationStatus_{-1},
                  UrcHandler_{nullptr},
                  BeginFirstCall_{true}
            {
            }

            /**
             * @~Japanese
             * @brief ネットワーク支援を開始
             *
             * ネットワーク支援を開始します。
             */
            void begin(void)
            {
                WioCellularResult result;

                // Set default abort handler
                if (!abortHandler)
                {
                    abortHandler = std::bind(&Bg770aNetwork::defaultAbortHandler, this, std::placeholders::_1, std::placeholders::_2);
                }

                // Check PDP context
                bool setPdpContext = false;
                if (BeginFirstCall_)
                {
                    if (!config.apn.empty())
                    {
                        std::vector<WioCellularModule::PdpContext> pdpContexts;
                        if ((result = WioCellular.getPdpContext(&pdpContexts)) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                        if (std::find_if(std::cbegin(pdpContexts), std::cend(pdpContexts), [&apn = config.apn](const WioCellularModule::PdpContext &pdpContext)
                                         { return pdpContext.apn == apn; }) == std::cend(pdpContexts))
                        {
                            setPdpContext = true;
                        }
                    }
                }
                // Check search access technology
                bool setSearchAccessTechnology = false;
                if (config.searchAccessTechnology != SearchAccessTechnology::NOSET)
                {
                    int actMode;
                    if ((result = WioCellular.getSearchAccessTechnology(&actMode)) != WioCellularResult::Ok)
                    {
                        abortHandler(__FILE__, __LINE__);
                    }
                    std::string actSequence;
                    if ((result = WioCellular.getSearchAccessTechnologySequence(&actSequence)) != WioCellularResult::Ok)
                    {
                        abortHandler(__FILE__, __LINE__);
                    }
                    switch (config.searchAccessTechnology)
                    {
                    case SearchAccessTechnology::LTEM:
                        if (actMode != 0)
                            setSearchAccessTechnology = true;
                        break;
                    case SearchAccessTechnology::NBIOT:
                        if (actMode != 1)
                            setSearchAccessTechnology = true;
                        break;
                    case SearchAccessTechnology::LTEM_NBIOT:
                        if (actMode != 2 || actSequence != "0203")
                            setSearchAccessTechnology = true;
                        break;
                    case SearchAccessTechnology::NBIOT_LTEM:
                        if (actMode != 2 || actSequence != "0302")
                            setSearchAccessTechnology = true;
                        break;
                    default:
                        abort();
                    }
                }

                // Check search frequency band
                bool setSearchFrequencyBand = false;
                if (!config.ltemBand.empty() || !config.nbiotBand.empty())
                {
                    std::string ltemBand;
                    std::string nbiotBand;
                    if ((result = WioCellular.getSearchFrequencyBand(nullptr, &ltemBand, &nbiotBand)) != WioCellularResult::Ok)
                    {
                        abortHandler(__FILE__, __LINE__);
                    }
                    if (!config.ltemBand.empty() && ltemBand != config.ltemBand)
                    {
                        setSearchFrequencyBand = true;
                    }
                    if (!config.nbiotBand.empty() && nbiotBand != config.nbiotBand)
                    {
                        setSearchFrequencyBand = true;
                    }
                }

                // Register EPS network registration state notification
                UrcHandler_ = WioCellular.registerUrcHandler2([this](const std::string &response) -> bool
                                                              {
                                                                if (response.compare(0, 8, "+CEREG: ") == 0) {
                                                                    wiocellular::module::at_client::AtParameterParser parser{response.substr(8)};
                                                                    if (parser.size() < 1) return false;
                                                                    EpsRegistrationStatus_ = std::stoi(parser[0]);
                                                                    return true;
                                                                }
                                                                return false; });
                if ((result = WioCellular.setEpsNetworkRegistrationStatusUrc(1)) != WioCellularResult::Ok)
                {
                    abortHandler(__FILE__, __LINE__);
                }
                if ((result = WioCellular.getEpsNetworkRegistrationState(&EpsRegistrationStatus_)) != WioCellularResult::Ok)
                {
                    abortHandler(__FILE__, __LINE__);
                }

                if (setPdpContext || setSearchAccessTechnology || setSearchFrequencyBand)
                {
                    // Get phone functionality and off it
                    int fun;
                    if ((result = WioCellular.getPhoneFunctionality(&fun)) != WioCellularResult::Ok)
                    {
                        abortHandler(__FILE__, __LINE__);
                    }
                    if (fun != 0)
                    {
                        if ((result = WioCellular.setPhoneFunctionality(0)) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                    }
                    while (getNetworkState() != NetworkState::NotSearching)
                    {
                        WioCellular.doWork(10); // Spin
                    }

                    // Set PDP context
                    if (setPdpContext)
                    {
                        if ((result = WioCellular.setPdpContext({config.pdpContextId, "IP", config.apn, "0.0.0.0", 0, 0, 0})) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                    }

                    // Set search access technology
                    if (setSearchAccessTechnology)
                    {
                        int actMode = -1;
                        std::string actSequence;
                        switch (config.searchAccessTechnology)
                        {
                        case SearchAccessTechnology::LTEM:
                            actMode = 0;
                            actSequence = "0203";
                            break;
                        case SearchAccessTechnology::NBIOT:
                            actMode = 1;
                            actSequence = "0302";
                            break;
                        case SearchAccessTechnology::LTEM_NBIOT:
                            actMode = 2;
                            actSequence = "0203";
                            break;
                        case SearchAccessTechnology::NBIOT_LTEM:
                            actMode = 2;
                            actSequence = "0302";
                            break;
                        default:
                            abort();
                        }
                        if ((result = WioCellular.setSearchAccessTechnology(actMode)) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                        if ((result = WioCellular.setSearchAccessTechnologySequence(actSequence)) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                    }

                    // Set search frequency band
                    if (setSearchFrequencyBand)
                    {
                        if ((result = WioCellular.setSearchFrequencyBand("0x0", !config.ltemBand.empty() ? config.ltemBand : "0x0", !config.nbiotBand.empty() ? config.nbiotBand : "0x0")) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                    }

                    // Set phone functionality back
                    if (fun != 0)
                    {
                        if ((result = WioCellular.setPhoneFunctionality(fun)) != WioCellularResult::Ok)
                        {
                            abortHandler(__FILE__, __LINE__);
                        }
                    }
                }

                BeginFirstCall_ = false;
            }

            /**
             * @~Japanese
             * @brief ネットワーク支援を終了
             *
             * @param [in] disconnect ネットワークを切断する。
             *
             * ネットワーク支援を終了します。
             */
            void end(bool disconnect = true)
            {
                if (disconnect)
                {
                    if (WioCellular.setPhoneFunctionality(0) != WioCellularResult::Ok)
                    {
                        abortHandler(__FILE__, __LINE__);
                    }
                }

                UrcHandler_ = nullptr;
                EpsRegistrationStatus_ = -1;
            }

            /**
             * @~Japanese
             * @brief ネットワーク状態を取得
             *
             * @return ネットワーク状態。
             *
             * ネットワーク状態を取得します。
             */
            NetworkState getNetworkState(void)
            {
                switch (EpsRegistrationStatus_)
                {
                case 1:
                case 5:
                    return NetworkState::Connected;
                case 3:
                    return NetworkState::Denied;
                case 2:
                    return NetworkState::Searching;
                case 0:
                    return NetworkState::NotSearching;
                default:
                    return NetworkState::Unknown;
                }
            }

            /**
             * @~Japanese
             * @brief ネットワーク状態を文字列に変換
             *
             * @param [in] state ネットワーク状態。
             * @return ネットワーク状態の文字列。
             *
             * ネットワーク状態を文字列に変換します。
             */
            static constexpr const char *networkStateToString(NetworkState state)
            {
                return state == NetworkState::NotSearching ? "NotSearching" : state == NetworkState::Searching ? "Searching"
                                                                          : state == NetworkState::Connected   ? "Connected"
                                                                          : state == NetworkState::Denied      ? "Denied"
                                                                                                               : "Unknown";
            }

            /**
             * @~Japanese
             * @brief 通信可否を取得
             *
             * @retval true 通信可能
             * @retval false 通信不可能
             *
             * 通信可否を取得します。
             */
            bool canCommunicate(void)
            {
                if (getNetworkState() != Bg770aNetwork::NetworkState::Connected)
                    return false;

                WioCellularResult result;

                // Get PDP address
                std::string address;
                if (WioCellular.getPdpAddress(config.pdpContextId, &address) != WioCellularResult::Ok)
                {
                    abortHandler(__FILE__, __LINE__);
                }

                return !address.empty();
            }

            /**
             * @~Japanese
             * @brief 通信可能を待機
             *
             * @param [in] timeout タイムアウト時間[ミリ秒]。
             * @retval true 接続完了
             * @retval false タイムアウト
             *
             * 通信可能になるまで待機します。
             * 永久に待機したいときはtimeoutに-1を指定します。
             */
            bool waitUntilCommunicationAvailable(int timeout)
            {
                return WioCellular.doWork(timeout, [this]
                                          { return canCommunicate(); });
            }
        };
    }
}

#endif // BG770ANETWORK_HPP
