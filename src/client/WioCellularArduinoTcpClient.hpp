/*
 * WioCellularArduinoTcpClient.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef WIOCELLULARARDUINOTCPCLIENT_HPP
#define WIOCELLULARARDUINOTCPCLIENT_HPP

#include "../WioCellular.hpp"
#include <Client.h>
#include <array>
#include <queue>
#include "WioCellularTcpClient2.hpp"

/**
 * @~Japanese
 * @brief Arduino用TCPクライアント
 *
 * @tparam MODULE モジュールのクラス
 *
 * Arduino用TCPクライアントのクラスです。
 */
template <typename MODULE>
class WioCellularArduinoTcpClient : public Client
{
private:
    MODULE &Module_;
    wiocellular::client::WioCellularTcpClient2<WioCellularModule> TcpClient_;
    int PdpContextId_;
    std::queue<uint8_t> ReceiveQueue_;
    std::array<uint8_t, MODULE::RECEIVE_SOCKET_SIZE_MAX> ReceiveBuffer_;
    int ConnectionTimeout_;

public:
    /**
     * @~Japanese
     * @brief 接続タイムアウトを設定
     *
     * @param [in] timeout タイムアウト時間[ミリ秒]。
     *
     * 接続タイムアウトを設定します。
     */
    void setConnectionTimeout(int timeout)
    {
        ConnectionTimeout_ = timeout;
    }

public:
    /**
     * @~Japanese
     * @brief コンストラクタ
     *
     * @param [in] module モジュールのインスタンス。
     * @param [in] pdpContextId PDPコンテキスト。
     *
     * コンストラクタ。
     */
    WioCellularArduinoTcpClient(MODULE &module, int pdpContextId) : Module_{module},
                                                                    TcpClient_{module},
                                                                    PdpContextId_{pdpContextId},
                                                                    ConnectionTimeout_{150000}
    {
    }

    /**
     * @~Japanese
     * @brief デストラクタ
     *
     * デストラクタ。
     */
    virtual ~WioCellularArduinoTcpClient()
    {
    }

    /**
     * @~Japanese
     * @brief TCPサーバーに接続
     *
     * @param [in] ip IPアドレス。
     * @param [in] port ポート番号。
     * @retval 1 成功
     * @retval 0 エラー
     *
     * TCPサーバーに接続します。
     */
    virtual int connect(IPAddress ip, uint16_t port)
    {
        String ipStr = String(ip[0]);
        ipStr += ".";
        ipStr += String(ip[1]);
        ipStr += ".";
        ipStr += String(ip[2]);
        ipStr += ".";
        ipStr += String(ip[3]);

        return connect(ipStr.c_str(), port);
    }

    /**
     * @~Japanese
     * @brief TCPサーバーに接続
     *
     * @param [in] host ホスト名。
     * @param [in] port ポート番号。
     * @retval 1 成功
     * @retval 0 エラー
     *
     * TCPサーバーに接続します。
     */
    virtual int connect(const char *host, uint16_t port)
    {
        if (!TcpClient_.open(PdpContextId_, host, port))
            return 0;

        if (!TcpClient_.waitForConnect(ConnectionTimeout_))
        {
            TcpClient_.close();
            return 0;
        }

        return 1;
    }

    /**
     * @~Japanese
     * @brief TCPサーバーへ送信
     *
     * @param [in] data データ。
     * @return 送信したデータサイズ。
     *
     * TCPサーバーへ送信します。
     */
    virtual size_t write(uint8_t data)
    {
        if (!TcpClient_.send(&data, 1))
            return 0;

        return 1;
    }

    /**
     * @~Japanese
     * @brief TCPサーバーへ送信
     *
     * @param [in] buf データ。
     * @param [in] size データサイズ。
     * @return 送信したデータサイズ。
     *
     * TCPサーバーへ送信します。
     */
    virtual size_t write(const uint8_t *buf, size_t size)
    {
        if (!TcpClient_.send(buf, size))
            return 0;

        return size;
    }

    /**
     * @~Japanese
     * @brief 未読のデータサイズを取得
     *
     * @retval >=0 未読のデータサイズ
     *
     * TCPサーバーから受信した、未読のデータサイズを取得します。
     */
    virtual int available()
    {
        size_t size;
        const auto result = TcpClient_.receive(ReceiveBuffer_.data(), ReceiveBuffer_.size(), &size);
        if (result)
        {
            for (size_t i = 0; i < size; ++i)
                ReceiveQueue_.push(ReceiveBuffer_[i]);
        }

        return ReceiveQueue_.size();
    }

    /**
     * @~Japanese
     * @brief TCPサーバーから受信
     *
     * @retval >=0 受信データ
     * @retval <0 受信データ無し
     *
     * TCPサーバーから受信します。
     * 受信データが無いときは負の値を返します。
     */
    virtual int read()
    {
        const int actualSize = available();
        if (actualSize <= 0)
            return -1;

        const uint8_t data = ReceiveQueue_.front();
        ReceiveQueue_.pop();

        return data;
    }

    /**
     * @~Japanese
     * @brief TCPサーバーから受信
     *
     * @param [in,out] buf データ。
     * @param [in] size データサイズ。
     * @retval >=0 受信したデータサイズ
     *
     * TCPサーバーから受信します。
     */
    virtual int read(uint8_t *buf, size_t size)
    {
        const int actualSize = available();
        if (actualSize <= 0)
            return 0;

        const int popSize = static_cast<size_t>(actualSize) <= size ? actualSize : size;
        for (int i = 0; i < popSize; ++i)
        {
            buf[i] = ReceiveQueue_.front();
            ReceiveQueue_.pop();
        }

        return popSize;
    }

    /**
     * @~Japanese
     * @brief TCPサーバーから先読み受信
     *
     * @retval >=0 受信データ
     * @retval <0 受信データ無し
     *
     * TCPサーバーから受信したデータを先読みします。
     * 受信データが無いときは負の値を返します。
     */
    virtual int peek()
    {
        const int actualSize = available();
        if (actualSize <= 0)
            return -1;

        return ReceiveQueue_.front();
    }

    /**
     * @~Japanese
     * @brief 受信データを破棄
     *
     * TCPサーバーから受信したデータを破棄します。
     */
    virtual void flush()
    {
        available();

        while (!ReceiveQueue_.empty())
            ReceiveQueue_.pop();
    }

    /**
     * @~Japanese
     * @brief TCPサーバーを切断
     *
     * TCPサーバーを切断します。
     */
    virtual void stop()
    {
        TcpClient_.close();

        while (!ReceiveQueue_.empty())
            ReceiveQueue_.pop();
    }

    /**
     * @~Japanese
     * @brief TCPサーバーの接続状態を取得
     *
     * @retval 1 接続
     * @retval 0 切断
     *
     * TCPサーバーの接続状態を取得します。
     */
    virtual uint8_t connected()
    {
        Module_.doWork(0);
        return TcpClient_.getState() == decltype(TcpClient_)::State::Connected ? 1 : 0;
    }

    /**
     * @~Japanese
     * @brief TCPサーバーの接続状態を取得
     *
     * @retval 1 接続
     * @retval 0 切断
     *
     * TCPサーバーの接続状態を取得します。
     */
    virtual operator bool()
    {
        return connected();
    }
};

/**
 * @~Japanese
 * @brief Arduino用TCPクライアント
 *
 * @tparam MODULE モジュールのクラス
 *
 * Arduino用TCPクライアントのクラスです。
 */
template <typename MODULE>
using WioCellularTcpClient [[deprecated("Use WioCellularArduinoTcpClient instead.")]] = WioCellularArduinoTcpClient<MODULE>;

#endif // WIOCELLULARARDUINOTCPCLIENT_HPP
