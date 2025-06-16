/*
 * Adafruit_SPIFlashMemory.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef ADAFRUIT_SPIFLASHMEMORY_HPP
#define ADAFRUIT_SPIFLASHMEMORY_HPP

#include <memory>
#include "NonVolatileMemoryInterface.hpp"

namespace wiocellular
{
    namespace component
    {
        namespace nonvolatilememory
        {

            /**
             * @~Japanese
             * @brief Adafruit_SPIFlashライブラリの不揮発性メモリインターフェース
             *
             * Adafruit_SPIFlashライブラリと不揮発性メモリインターフェースのブリッジクラスです。
             */
            class Adafruit_SPIFlashMemory : public NonVolatileMemoryInterface
            {
            private:
                Adafruit_SPIFlashBase &Flash_;

            public:
                /**
                 * @~Japanese
                 * @brief コンストラクタ
                 *
                 * @param [in] flash Adafruit_SPIFlashライブラリのフラッシュメモリのインスタンス。
                 *
                 * コンストラクタ。
                 */
                explicit Adafruit_SPIFlashMemory(Adafruit_SPIFlashBase &flash) : Flash_(flash)
                {
                }

                /**
                 * @~Japanese
                 * @brief サイズを取得
                 *
                 * @return サイズ[バイト数]
                 *
                 * サイズを取得します。
                 */
                size_t size() const override
                {
                    return Flash_.size();
                }

                /**
                 * @~Japanese
                 * @brief 読み込み
                 *
                 * @param [in] address アドレス。
                 * @param [out] data データ。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * データを読み込みます。
                 * エラーのときは負の値を返します。
                 */
                long read(uintptr_t address, void *data, size_t dataSize) const override
                {
                    if (address + dataSize > Flash_.size())
                        return -1;
                    if (!data)
                        return -1;
                    if (dataSize < 1)
                        return -1;

                    if (Flash_.readBuffer(address, reinterpret_cast<uint8_t *>(data), dataSize) != dataSize)
                        return -1;

                    return 0;
                }

                /**
                 * @~Japanese
                 * @brief 書き込み
                 *
                 * @param [in] address アドレス。
                 * @param [in] data データ。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * データを書き込みます。
                 * エラーのときは負の値を返します。
                 */
                long write(uintptr_t address, const void *data, size_t dataSize) override
                {
                    if (address + dataSize > Flash_.size())
                        return -1;
                    if (!data)
                        return -1;
                    if (dataSize < 1)
                        return -1;

                    if (nrf_dma_accessible_check(nullptr, data))
                    {
                        if (Flash_.writeBuffer(address, reinterpret_cast<const uint8_t *>(data), dataSize) != dataSize)
                            return -1;
                    }
                    else
                    {
                        const auto dataInRam = std::make_unique<uint8_t[]>(dataSize);
                        memcpy(dataInRam.get(), data, dataSize);
                        if (Flash_.writeBuffer(address, dataInRam.get(), dataSize) != dataSize)
                            return -1;
                    }

                    // // Verify
                    // const auto readData = std::make_unique<uint8_t[]>(dataSize);
                    // if (Flash_.readBuffer(address, readData.get(), dataSize) != dataSize)
                    //     return -1;
                    // if (memcmp(data, readData.get(), dataSize) != 0)
                    //     return -1;

                    return 0;
                }
            };

        }
    }
}

#endif // ADAFRUIT_SPIFLASHMEMORY_HPP
