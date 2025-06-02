/*
 * NonVolatileMemoryInterface.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef NONVOLATILEMEMORYINTERFACE_HPP
#define NONVOLATILEMEMORYINTERFACE_HPP

namespace wiocellular
{
    namespace component
    {
        namespace nonvolatilememory
        {

            /**
             * @~Japanese
             * @brief 不揮発性メモリのインターフェース
             *
             * 不揮発性メモリとやりとりするインターフェースのクラスです。
             */
            class NonVolatileMemoryInterface
            {
            public:
                /**
                 * @~Japanese
                 * @brief 不揮発性メモリのサイズを取得
                 *
                 * @return 不揮発性メモリのサイズ[バイト数]
                 *
                 * 不揮発性メモリのサイズを取得します。
                 */
                virtual size_t size() const = 0;

                /**
                 * @~Japanese
                 * @brief 不揮発性メモリから読み込み
                 *
                 * @param [in] address アドレス。
                 * @param [out] data データ。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * 不揮発性メモリからデータを読み込みます。
                 * エラーのときは負の値を返します。
                 */
                virtual long read(uintptr_t address, void *data, size_t dataSize) const = 0;

                /**
                 * @~Japanese
                 * @brief 不揮発性メモリへ書き込み
                 *
                 * @param [in] address アドレス。
                 * @param [in] data データ。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * 不揮発性メモリへデータを書き込みます。
                 * エラーのときは負の値を返します。
                 */
                virtual long write(uintptr_t address, const void *data, size_t dataSize) = 0;
            };

        }
    }
}

#endif // NONVOLATILEMEMORYINTERFACE_HPP
