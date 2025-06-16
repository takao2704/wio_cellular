/*
 * WioCellularUdpClient2.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef WIOCELLULARUDPCLIENT2_HPP
#define WIOCELLULARUDPCLIENT2_HPP

#include <memory>
#include "internal/CountdownTimer.hpp"
#include "internal/Misc.hpp"
#include "internal/UsedConnectIds.hpp"
#include "internal/WioLog.hpp"
#include "WioCellularResult.hpp"

namespace wiocellular::client
{

    /**
     * @~Japanese
     * @brief UDPクライアント
     *
     * @tparam MODULE モジュールのクラス
     *
     * UDPクライアントのクラスです。
     */
    template <typename MODULE>
    class WioCellularUdpClient2
    {
    public:
        /**
         * @~Japanese
         * @brief クライアント状態
         */
        enum class State
        {
            /**
             * @~Japanese
             * @brief クローズ
             */
            Closed,
            /**
             * @~Japanese
             * @brief オープン完了
             */
            Opened,
            /**
             * @~Japanese
             * @brief 接続エラー
             */
            ConnectError,
            /**
             * @~Japanese
             * @brief 接続完了
             */
            Connected,
        };

    private:
        MODULE &Module_;
        State State_;
        WioCellularResult LastResult_;
        std::unique_ptr<typename MODULE::UrcHandler> UrcHandler_;
        int ConnectId_;
        bool ReceivedNofity_;
        uint32_t openedTime_;

    private:
        void setState(State state)
        {
            wiocellular::internal::WioLog(wiocellular::internal::WioLogType::INFO, "WioCellularUdpClient2<%d> state changed %d to %d", ConnectId_, static_cast<int>(State_), static_cast<int>(state));
            State_ = state;
        }

    public:
        /**
         * @~Japanese
         * @brief クライアント状態を取得
         *
         * @return クライアント状態。
         *
         * クライアント状態を取得します。
         * この関数は最後の実行結果に影響を与えません。
         */
        State getState() const
        {
            return State_;
        }

        /**
         * @~Japanese
         * @brief 最後の実行結果を取得
         *
         * @return 最後の実行結果。
         *
         * 最後の実行結果を取得します。
         */
        WioCellularResult getLastResult() const
        {
            return LastResult_;
        }

    private:
        bool urcHandler(const std::string &response)
        {
            if (getState() == State::Opened)
            {
                const std::string urcQiopenPrefix = wiocellular::internal::stringFormat("+QIOPEN: %d,", ConnectId_);
                if (response.starts_with(urcQiopenPrefix))
                {
                    const auto result = std::stoi(response.substr(urcQiopenPrefix.size()));
                    setState(result == 0 ? State::Connected : State::ConnectError);
                    return true;
                }
            }
            if (getState() == State::Connected)
            {
                const std::string urcQiurcRecv = wiocellular::internal::stringFormat("+QIURC: \"recv\",%d", ConnectId_);
                const std::string urcQiurcRecvPrefix = wiocellular::internal::stringFormat("+QIURC: \"recv\",%d,", ConnectId_);
                if (response == urcQiurcRecv || response.starts_with(urcQiurcRecvPrefix))
                {
                    wiocellular::internal::WioLog(wiocellular::internal::WioLogType::INFO, "---> WioCellularUdpClient2<%d> received", ConnectId_);
                    ReceivedNofity_ = true;
                    return true;
                }
            }
            return false;
        }

    public:
        /**
         * @~Japanese
         * @brief コンストラクタ
         *
         * @param [in] module モジュールのインスタンス。
         *
         * コンストラクタ。
         */
        WioCellularUdpClient2(MODULE &module) : Module_{module},
                                                State_{State::Closed},
                                                LastResult_{WioCellularResult::Ok},
                                                UrcHandler_{nullptr},
                                                ConnectId_{-1},
                                                ReceivedNofity_{false},
                                                openedTime_{0}
        {
        }

        /**
         * @~Japanese
         * @brief デストラクタ
         *
         * デストラクタ。
         */
        ~WioCellularUdpClient2()
        {
            close();
        }

        /**
         * @~Japanese
         * @brief オープン
         *
         * @param [in] cid PDPコンテキストID。
         * @param [in] ipAddress IPアドレス。
         * @param [in] remotePort リモートポート番号。
         * @retval true 成功
         * @retval false エラー
         *
         * UDPクライアントをオープンします。
         * エラーの詳細をgetLastResult()で取得できます。
         */
        bool open(int cid, const std::string &ipAddress, int remotePort)
        {
            if (getState() != State::Closed)
            {
                LastResult_ = WioCellularResult::InvalidOperation;
                return false;
            }

            {
                int connectId = -1;
                for (size_t i = 0; i < wiocellular::internal::UsedConnectIds.size(); ++i)
                {
                    if (!wiocellular::internal::UsedConnectIds.test(i))
                    {
                        connectId = i;
                        break;
                    }
                }
                if (connectId < 0)
                {
                    LastResult_ = WioCellularResult::InsufficientResources;
                    return false;
                }
                wiocellular::internal::UsedConnectIds.set(connectId);
                ConnectId_ = connectId;
            }

            ReceivedNofity_ = false;
            UrcHandler_ = Module_.registerUrcHandler2(std::bind(&WioCellularUdpClient2::urcHandler, this, std::placeholders::_1));

            if (const auto result = WioCellular.openSocket2(cid, ConnectId_, "UDP", ipAddress, remotePort, 0); result != WioCellularResult::Ok)
            {
                UrcHandler_ = nullptr;
                wiocellular::internal::UsedConnectIds.reset(ConnectId_);
                LastResult_ = result;
                return false;
            }

            setState(State::Opened);
            openedTime_ = millis();

            LastResult_ = WioCellularResult::Ok;
            return true;
        }

        /**
         * @~Japanese
         * @brief 接続完了を待機
         *
         * @param [in] timeout タイムアウト時間[ミリ秒]。
         * @retval true 接続完了
         * @retval false タイムアウトもしくは接続エラー
         *
         * UDPクライアントの接続完了を待機します。
         * 永久に待機したいときはtimeoutに-1を指定します。
         * エラーの詳細をgetLastResult()で取得できます。
         *
         * @note
         * UDPには接続という概念はありませんが、モジュールとのやりとりにはオープンと接続があります。
         * そのため、UDPであっても接続完了を待機しなければいけません。
         * なお、オープンのときにドメイン名の名前解決の送受信が発生しますが、接続処理として送受信は発生しません。
         */
        bool waitForConnect(int timeout = 150000)
        {
            if (getState() != State::Opened)
            {
                LastResult_ = WioCellularResult::InvalidOperation;
                return false;
            }

            if (!Module_.doWork(timeout, [this]
                                { return getState() != State::Opened; }))
            {
                LastResult_ = WioCellularResult::ConnectTimeout;
                return false;
            }
            if (getState() != State::Connected)
            {
                LastResult_ = WioCellularResult::ConnectError;
                return false;
            }

            LastResult_ = WioCellularResult::Ok;
            return true;
        }

        [[deprecated("Use waitForConnect() instead.")]]
        bool waitforConnect(int timeout = 150000)
        {
            return waitForConnect(timeout);
        }

        /**
         * @~Japanese
         * @brief クローズ
         *
         * @retval true 成功
         * @retval false エラー
         *
         * UDPクライアントをクローズします。
         * エラーの詳細をgetLastResult()で取得できます。
         */
        bool close()
        {
            if (getState() != State::Closed)
            {
                // The socket cannot be closed during a connect. (X_X)
                // Therefore, if the timeout period is short, wait for a certain period of time.
                if (getState() == State::Opened && millis() - openedTime_ < 150000)
                {
                    const auto wait = 150000 - (millis() - openedTime_);
                    wiocellular::internal::WioLog(wiocellular::internal::WioLogType::INFO, "WioCellularUdpClient2<%d> forced to wait %lu", ConnectId_, wait);
                    WioCellular.doWork(wait, [this]
                                       { return getState() != State::Opened; });
                }

                if (const auto result = Module_.closeSocket2(ConnectId_); result != WioCellularResult::Ok)
                {
                    LastResult_ = result;
                    return false;
                }

                UrcHandler_ = nullptr;
                wiocellular::internal::UsedConnectIds.reset(ConnectId_);
                setState(State::Closed);
            }

            LastResult_ = WioCellularResult::Ok;
            return true;
        }

        /**
         * @~Japanese
         * @brief 送信
         *
         * @param [in] data データ。
         * @param [in] dataSize データサイズ。
         * @retval true 成功
         * @retval false エラー
         *
         * データを送信します。
         * エラーの詳細をgetLastResult()で取得できます。
         */
        bool send(const void *data, size_t dataSize)
        {
            for (size_t offset = 0; offset < dataSize; offset += Module_.SEND_SOCKET_SIZE_MAX)
            {
                if (getState() != State::Connected)
                {
                    LastResult_ = WioCellularResult::InvalidOperation;
                    return false;
                }

                if (const auto result = Module_.sendSocket2(ConnectId_, static_cast<const uint8_t *>(data) + offset, std::min(dataSize - offset, Module_.SEND_SOCKET_SIZE_MAX)); result != WioCellularResult::Ok)
                {
                    LastResult_ = result;
                    return false;
                }
            }

            LastResult_ = WioCellularResult::Ok;
            return true;
        }

        /**
         * @~Japanese
         * @brief 受信
         *
         * @param [in,out] data データ。
         * @param [in] dataSize データサイズ。
         * @param [out] readDataSize 受信したデータサイズ。
         * @retval true 成功
         * @retval false エラー
         *
         * 受信したデータを取得します。
         * エラーの詳細をgetLastResult()で取得できます。
         */
        bool receive(void *data, size_t dataSize, size_t *readDataSize)
        {
            if (getState() != State::Connected)
            {
                LastResult_ = WioCellularResult::InvalidOperation;
                return false;
            }

            Module_.doWork(0);
            if (ReceivedNofity_)
            {
                ReceivedNofity_ = false;

                if (const auto result = Module_.receiveSocket2(ConnectId_, data, dataSize, readDataSize); result != WioCellularResult::Ok)
                {
                    LastResult_ = result;
                    return false;
                }

                if (*readDataSize >= 1)
                {
                    ReceivedNofity_ = true;
                }
            }
            else
            {
                *readDataSize = 0;
            }

            LastResult_ = WioCellularResult::Ok;
            return true;
        }

        /**
         * @~Japanese
         * @brief 受信
         *
         * @param [in,out] data データ。
         * @param [in] dataSize データサイズ。
         * @param [out] readDataSize 受信したデータサイズ。
         * @param [in] timeout タイムアウト時間[ミリ秒]。
         * @retval true 成功
         * @retval false タイムアウトもしくはエラー
         *
         * 受信したデータを取得します。
         * 永久に待機したいときはtimeoutに-1を指定します。
         * エラーの詳細をgetLastResult()で取得できます。
         */
        bool receive(void *data, size_t dataSize, size_t *readDataSize, int timeout)
        {
            if (getState() != State::Connected)
            {
                LastResult_ = WioCellularResult::InvalidOperation;
                return false;
            }

            wiocellular::internal::CountdownTimer timer{timeout};
            while (true)
            {
                if (!Module_.doWork(timer.remaining(), [this]
                                    { return ReceivedNofity_ || getState() != State::Connected; }))
                {
                    LastResult_ = WioCellularResult::ReceiveTimeout;
                    return false;
                }

                if (!receive(data, dataSize, readDataSize))
                {
                    return false;
                }
                if (*readDataSize >= 1)
                {
                    break;
                }
            }

            LastResult_ = WioCellularResult::Ok;
            return true;
        }
    };

}

#endif // WIOCELLULARUDPCLIENT2_HPP
