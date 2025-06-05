/*
 * WioCellularTcpClient2.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef WIOCELLULARTCPCLIENT2_HPP
#define WIOCELLULARTCPCLIENT2_HPP

#include <bitset>
#include <memory>
#include "internal/CountdownTimer.hpp"
#include "internal/Misc.hpp"
#include "WioCellularResult.hpp"

namespace wiocellular::client
{

    /**
     * @~Japanese
     * @brief TCPクライアント
     *
     * @tparam MODULE モジュールのクラス
     *
     * TCPクライアントのクラスです。
     */
    template <typename MODULE>
    class WioCellularTcpClient2
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
            /**
             * @~Japanese
             * @brief クローズ中
             */
            Closing, // CLOSE_WAIT
        };

    private:
        MODULE &Module_;
        State State_;
        WioCellularResult LastResult_;
        std::unique_ptr<typename MODULE::UrcHandler> UrcHandler_;
        inline static std::bitset<12> UsedConnectIds_;
        int ConnectId_;
        bool ReceivedNofity_;
        uint32_t openedTime_;

    private:
        void setState(State state)
        {
            printf("---> WioCellularTcpClient2<%d> state changed %d to %d\n", ConnectId_, static_cast<int>(State_), static_cast<int>(state));
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
                const std::string urcQiurcClosed = wiocellular::internal::stringFormat("+QIURC: \"closed\",%d", ConnectId_);
                if (response == urcQiurcClosed)
                {
                    setState(State::Closing);
                    return true;
                }
            }
            if (getState() == State::Connected || getState() == State::Closing)
            {
                const std::string urcQiurcRecv = wiocellular::internal::stringFormat("+QIURC: \"recv\",%d", ConnectId_);
                const std::string urcQiurcRecvPrefix = wiocellular::internal::stringFormat("+QIURC: \"recv\",%d,", ConnectId_);
                if (response == urcQiurcRecv || response.starts_with(urcQiurcRecvPrefix))
                {
                    printf("---> WioCellularTcpClient2<%d> received\n", ConnectId_);
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
        WioCellularTcpClient2(MODULE &module) : Module_{module},
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
        ~WioCellularTcpClient2()
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
         * TCPクライアントをオープンします。
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
                for (size_t i = 0; i < UsedConnectIds_.size(); ++i)
                {
                    if (!UsedConnectIds_.test(i))
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
                UsedConnectIds_.set(connectId);
                ConnectId_ = connectId;
            }

            ReceivedNofity_ = false;
            UrcHandler_ = Module_.registerUrcHandler2(std::bind(&WioCellularTcpClient2::urcHandler, this, std::placeholders::_1));

            if (const auto result = WioCellular.openSocket2(cid, ConnectId_, "TCP", ipAddress, remotePort, 0); result != WioCellularResult::Ok)
            {
                UrcHandler_ = nullptr;
                UsedConnectIds_.reset(ConnectId_);
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
         * TCPクライアントの接続完了を待機します。
         * 永久に待機したいときはtimeoutに-1を指定します。
         * エラーの詳細をgetLastResult()で取得できます。
         */
        bool waitforConnect(int timeout = 150000)
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

        /**
         * @~Japanese
         * @brief クローズ
         *
         * @retval true 成功
         * @retval false エラー
         *
         * TCPクライアントをクローズします。
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
                    printf("---> WioCellularTcpClient2<%d> forced to wait %lu\n", ConnectId_, wait);
                    WioCellular.doWork(wait, [this]
                                       { return getState() != State::Opened; });
                }

                if (const auto result = Module_.closeSocket2(ConnectId_); result != WioCellularResult::Ok)
                {
                    LastResult_ = result;
                    return false;
                }

                UrcHandler_ = nullptr;
                UsedConnectIds_.reset(ConnectId_);
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
            if (getState() != State::Connected && getState() != State::Closing)
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

            if (*readDataSize == 0 && getState() != State::Connected)
            {
                LastResult_ = WioCellularResult::Closing;
                return false;
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
            if (getState() != State::Connected && getState() != State::Closing)
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

#endif // WIOCELLULARTCPCLIENT2_HPP
