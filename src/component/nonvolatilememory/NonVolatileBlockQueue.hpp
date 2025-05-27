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
                struct BlockInfo
                {
                    size_t index;
                    size_t size;
                };

            public:
                explicit NonVolatileBlockQueue(NonVolatileMemoryInterface &interface) : Interface_(interface)
                {
                }

                long clear()
                {

                    if (const auto result = setWriteIndex(0); result < 0)
                        return result;
                    if (const auto result = setReadIndex(0); result < 0)
                        return result;

                    return 0;
                }

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

                long read(void *data, size_t dataSize, size_t *readDataSize)
                {
                    return readInternal(data, dataSize, readDataSize, false);
                }

                long peek(void *data, size_t dataSize, size_t *readDataSize)
                {
                    return readInternal(data, dataSize, readDataSize, true);
                }

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
