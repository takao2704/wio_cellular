/*
 * NonVolatileBlockQueue.hpp
 * Copyright (C) Seeed K.K.
 * MIT License
 */

#ifndef NONVOLATILEBLOCKQUEUE_HPP
#define NONVOLATILEBLOCKQUEUE_HPP

#include <algorithm>
#include <vector>
#include "NonVolatileMemoryInterface.hpp"

namespace wiocellular
{
    namespace component
    {
        namespace nonvolatilememory
        {

            /**
             * @~Japanese
             * @brief 不揮発性メモリのブロックキュー
             *
             * 不揮発性メモリのブロックキューのクラスです。
             */
            class NonVolatileBlockQueue
            {
            private:
                NonVolatileMemoryInterface &Interface_;

            private:
                size_t indexSectionSize() const
                {
                    return sizeof(size_t) * 6;
                }

                size_t dataSectionSize() const
                {
                    return Interface_.size() - indexSectionSize();
                }

                size_t dataSectionFreeSize(size_t writeIndex, size_t readIndex) const
                {
                    if (writeIndex >= dataSectionSize() || readIndex >= dataSectionSize())
                        abort();

                    size_t dataFreeSize = writeIndex < readIndex ? readIndex - writeIndex : readIndex + dataSectionSize() - writeIndex;
                    if (dataFreeSize >= 1)
                        --dataFreeSize;

                    return dataFreeSize;
                }

                long getIndex(size_t *writeIndex, size_t *readIndex) const
                {
                    if (!writeIndex || !readIndex)
                        return -1;

                    size_t index[6];
                    if (const auto result = Interface_.read(0, index, sizeof(index)); result < 0)
                        return result;

                    if (index[1] == index[2] || index[0] == index[1])
                        *writeIndex = index[1];
                    else
                        *writeIndex = index[0];

                    if (index[4] == index[5] || index[3] == index[4])
                        *readIndex = index[4];
                    else
                        *readIndex = index[3];

                    return 0;
                }

                long setWriteIndex(size_t writeIndex)
                {
                    if (writeIndex >= dataSectionSize())
                        return -1;

                    const size_t index[3] = {writeIndex, writeIndex, writeIndex};
                    return Interface_.write(0, index, sizeof(index));
                }

                long setReadIndex(size_t readIndex)
                {
                    if (readIndex >= dataSectionSize())
                        return -1;

                    const size_t index[3] = {readIndex, readIndex, readIndex};
                    return Interface_.write(sizeof(index), index, sizeof(index));
                }

                long getData(size_t index, void *data, size_t dataSize) const
                {
                    if (index >= dataSectionSize())
                        return -1;
                    if (!data || dataSize <= 0)
                        return -1;

                    const auto firstDataSize = std::min(dataSize, dataSectionSize() - index);
                    if (const auto result = Interface_.read(indexSectionSize() + index, data, firstDataSize); result < 0)
                        return result;
                    if (dataSize > firstDataSize)
                    {
                        if (const auto result = Interface_.read(indexSectionSize(), reinterpret_cast<uint8_t *>(data) + firstDataSize, dataSize - firstDataSize); result < 0)
                            return result;
                    }

                    return 0;
                }

                long setData(size_t index, const void *data, size_t dataSize)
                {
                    if (index >= dataSectionSize())
                        return -1;
                    if (!data || dataSize <= 0)
                        return -1;

                    const auto firstDataSize = std::min(dataSize, dataSectionSize() - index);
                    if (const auto result = Interface_.write(indexSectionSize() + index, data, firstDataSize); result < 0)
                        return result;
                    if (dataSize > firstDataSize)
                    {
                        if (const auto result = Interface_.write(indexSectionSize(), reinterpret_cast<const uint8_t *>(data) + firstDataSize, dataSize - firstDataSize); result < 0)
                            return result;
                    }

                    return 0;
                }

                long readInternal(void *data, size_t dataSize, size_t *readDataSize, bool peek)
                {
                    if ((data && dataSize < 1) || (!data && dataSize > 0))
                        return -1;

                    size_t writeIndex;
                    size_t readIndex;
                    if (const auto result = getIndex(&writeIndex, &readIndex); result < 0)
                        return result;
                    if (readIndex == writeIndex)
                    {
                        if (readDataSize)
                            *readDataSize = 0;
                        return 0;
                    }

                    size_t readDataSizeInternal;
                    if (const auto result = getData(readIndex, &readDataSizeInternal, sizeof(readDataSizeInternal)); result < 0)
                        return result;
                    if (readDataSize)
                        *readDataSize = readDataSizeInternal;
                    if (readDataSizeInternal <= 0)
                        return -1;

                    if (data)
                    {
                        if (readDataSizeInternal > dataSize)
                            return -1; // TODO
                        if (const auto result = getData((readIndex + sizeof(readDataSizeInternal)) % dataSectionSize(), data, readDataSizeInternal); result < 0)
                            return result;
                    }

                    if (!peek)
                    {
                        if (const auto result = setReadIndex((readIndex + sizeof(readDataSizeInternal) + readDataSizeInternal) % dataSectionSize()); result < 0)
                            return result;
                    }

                    return 0;
                }

            public:
                /**
                 * @~Japanese
                 * @brief ブロック情報
                 */
                struct BlockInfo
                {
                    /**
                     * @~Japanese
                     * @brief ブロックの開始インデックス
                     */
                    size_t index;
                    /**
                     * @~Japanese
                     * @brief ブロックのサイズ[バイト数]
                     */
                    size_t size;
                };

            public:
                /**
                 * @~Japanese
                 * @brief コンストラクタ
                 *
                 * @param [in] interface 不揮発性メモリインターフェースのインスタンス
                 *
                 * コンストラクタ。
                 */
                explicit NonVolatileBlockQueue(NonVolatileMemoryInterface &interface) : Interface_(interface)
                {
                }

                /**
                 * @~Japanese
                 * @brief クリア
                 *
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * キューをクリアします。
                 * （書き込みインデックスと読み込みインデックスを0に設定します。）
                 */
                long clear()
                {

                    if (const auto result = setWriteIndex(0); result < 0)
                        return result;
                    if (const auto result = setReadIndex(0); result < 0)
                        return result;

                    return 0;
                }

                /**
                 * @~Japanese
                 * @brief 空きサイズを取得
                 *
                 * @param [out] size 空きサイズ[バイト]。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * キューの空きサイズを取得します。
                 * （ブロックサイズの保存領域を減算しないため、書き込みできる最大サイズとは一致しません。）
                 */
                long freeSize(size_t *size)
                {
                    if (!size)
                        return -1;

                    size_t writeIndex;
                    size_t readIndex;
                    if (const auto result = getIndex(&writeIndex, &readIndex); result < 0)
                        return result;

                    *size = dataSectionFreeSize(writeIndex, readIndex);

                    return 0;
                }

                /**
                 * @~Japanese
                 * @brief 書き込み（キューに追加）
                 *
                 * @param [in] data データ。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * ブロックキューにデータを書き込みます。（キューに追加します。）
                 * エラーのときは負の値を返します。
                 */
                long write(const void *data, size_t dataSize)
                {
                    if (!data || dataSize < 1)
                        return -1;

                    size_t writeIndex;
                    size_t readIndex;
                    if (const auto result = getIndex(&writeIndex, &readIndex); result < 0)
                        return result;

                    if (dataSectionFreeSize(writeIndex, readIndex) < sizeof(dataSize) + dataSize)
                        return -1;

                    if (const auto result = setData(writeIndex, &dataSize, sizeof(dataSize)); result < 0)
                        return result;

                    if (dataSize >= 1)
                    {
                        if (const auto result = setData((writeIndex + sizeof(dataSize)) % dataSectionSize(), data, dataSize); result < 0)
                            return result;
                    }

                    if (const auto result = setWriteIndex((writeIndex + sizeof(dataSize) + dataSize) % dataSectionSize()); result < 0)
                        return result;

                    return 0;
                }

                /**
                 * @~Japanese
                 * @brief 読み込み（キューから取出し）
                 *
                 * @param [out] data データ。nullptrを指定すると読み捨てます。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @param [out] readDataSize 取り出したデータサイズ[バイト数]。nullptrを指定すると値を代入しません。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * データを読み込みます。（キューから取出します。）
                 * dataにnullptr、dataSizeに0を指定すると、データを読み捨てます。
                 * エラーのときは負の値を返します。
                 */
                long read(void *data, size_t dataSize, size_t *readDataSize)
                {
                    return readInternal(data, dataSize, readDataSize, false);
                }

                /**
                 * @~Japanese
                 * @brief 読み込み（キューから取出さない）
                 *
                 * @param [out] data データ。nullptrを指定すると読み捨てます。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @param [out] readDataSize 取り出したデータサイズ[バイト数]。nullptrを指定すると値を代入しません。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * データを読み込みます。（キューから取出しません。）
                 * dataにnullptr、dataSizeに0を指定すると、データを読み捨てます。
                 * エラーのときは負の値を返します。
                 */
                long peek(void *data, size_t dataSize, size_t *readDataSize)
                {
                    return readInternal(data, dataSize, readDataSize, true);
                }

                /**
                 * @~Japanese
                 * @brief ブロック情報読み込み（キューから取出さない）
                 *
                 * @param [out] blockInfoList ブロック情報リスト。
                 * @param [in] maxSize 読み込むブロック情報数の最大。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * ブロック情報を読み込みます。（キューから取出しません。）
                 * エラーのときは負の値を返します。
                 */
                long peekBlockInfo(std::vector<BlockInfo> *blockInfoList, size_t maxSize = SIZE_MAX)
                {
                    if (!blockInfoList)
                        return -1;

                    size_t writeIndex;
                    size_t readIndex;
                    if (const auto result = getIndex(&writeIndex, &readIndex); result < 0)
                        return result;

                    blockInfoList->clear();
                    while (readIndex != writeIndex && maxSize--)
                    {
                        size_t readDataSize;
                        if (const auto result = getData(readIndex, &readDataSize, sizeof(readDataSize)); result < 0)
                            return result;
                        if (readDataSize <= 0)
                            return -1;

                        blockInfoList->push_back({readIndex, readDataSize});

                        readIndex = (readIndex + sizeof(readDataSize) + readDataSize) % dataSectionSize();
                    }

                    return 0;
                }

                /**
                 * @~Japanese
                 * @brief 読み込み（キューから取出さない）
                 *
                 * @param [out] blockInfo ブロック情報。
                 * @param [out] data データ。nullptrを指定すると読み捨てます。
                 * @param [in] dataSize データサイズ[バイト数]。
                 * @param [out] readDataSize 取り出したデータサイズ[バイト数]。nullptrを指定すると値を代入しません。
                 * @retval >=0 成功
                 * @retval <0 エラー
                 *
                 * ブロック情報で指定されたデータを読み込みます。（キューから取出しません。）
                 * dataにnullptr、dataSizeに0を指定すると、データを読み捨てます。
                 * エラーのときは負の値を返します。
                 */
                long peek(const BlockInfo &blockInfo, void *data, size_t dataSize, size_t *readDataSize)
                {
                    if ((data && dataSize < 1) || (!data && dataSize > 0))
                        return -1;

                    if (readDataSize)
                        *readDataSize = blockInfo.size;
                    if (blockInfo.size <= 0)
                        return -1;

                    if (data)
                    {
                        if (blockInfo.size > dataSize)
                            return -1; // TODO
                        if (const auto result = getData((blockInfo.index + sizeof(blockInfo.size)) % dataSectionSize(), data, blockInfo.size); result < 0)
                            return result;
                    }

                    return 0;
                }
            };

        }
    }
}

#endif // NONVOLATILEBLOCKQUEUE_HPP
