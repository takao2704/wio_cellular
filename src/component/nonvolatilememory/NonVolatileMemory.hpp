/*
 * NonVolatileMemory.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef NONVOLATILEMEMORY_HPP
#define NONVOLATILEMEMORY_HPP

#include "NonVolatileMemoryInterface.hpp"

namespace wiocellular
{
    namespace component
    {
        namespace nonvolatilememory
        {

            /**
             * @~Japanese
             * @brief 不揮発性メモリ
             *
             * 不揮発性メモリのクラスです。
             */
            class NonVolatileMemory : public NonVolatileMemoryInterface
            {
            private:
                NonVolatileMemoryInterface &Interface_;
                uintptr_t BaseAddress_;
                size_t Size_;

            public:
                /**
                 * @~Japanese
                 * @brief コンストラクタ
                 *
                 * @param [in] interface 不揮発性メモリインターフェースのインスタンス
                 * @param [in] baseAddress インターフェースへアクセスするベースアドレス
                 * @param [in] size インターフェースへアクセスするサイズ
                 *
                 * コンストラクタ。
                 */
                NonVolatileMemory(NonVolatileMemoryInterface &interface, uintptr_t baseAddress, size_t size) : Interface_(interface),
                                                                                                               BaseAddress_(baseAddress),
                                                                                                               Size_(size)
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
                    return Size_;
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
                    if (Size_ < address + dataSize)
                        return -1;

                    return Interface_.read(BaseAddress_ + address, data, dataSize);
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
                    if (Size_ < address + dataSize)
                        return -1;

                    return Interface_.write(BaseAddress_ + address, data, dataSize);
                }

                template <typename T>
                class ValueRef
                {
                private:
                    NonVolatileMemoryInterface &Interface_;

                public:
                    explicit ValueRef(NonVolatileMemoryInterface &interface) : Interface_(interface)
                    {
                    }

                    operator T() const
                    {
                        T value;

                        if (Interface_.read(0, &value, sizeof(T)) < 0)
                            abort();

                        return value;
                    }

                    ValueRef &operator=(T value)
                    {
                        if (Interface_.write(0, &value, sizeof(T)) < 0)
                            abort();

                        return *this;
                    }
                };

                /**
                 * @~Japanese
                 * @brief 値の参照を取得
                 *
                 * @return 値の参照
                 *
                 * 特定の型でアクセスできる参照を取得します。
                 */
                template <typename T>
                ValueRef<T> value()
                {
                    return ValueRef<T>(*this);
                }
            };

        }
    }
}

#endif // NONVOLATILEMEMORY_HPP
