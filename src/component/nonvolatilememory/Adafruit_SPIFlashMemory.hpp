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

            class Adafruit_SPIFlashMemory : public NonVolatileMemoryInterface
            {
            private:
                Adafruit_SPIFlashBase &Flash_;

            public:
                explicit Adafruit_SPIFlashMemory(Adafruit_SPIFlashBase &flash) : Flash_(flash)
                {
                }

                size_t size() const override
                {
                    return Flash_.size();
                }

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

                    // // for debug
                    // Serial.printf("R [%04X] ", address);
                    // for (size_t i = 0; i < dataSize; ++i)
                    // {
                    //     Serial.printf("%02X ", reinterpret_cast<uint8_t *>(data)[i]);
                    // }
                    // Serial.println();

                    return 0;
                }

                long write(uintptr_t address, const void *data, size_t dataSize) override
                {
                    if (address + dataSize > Flash_.size())
                        return -1;
                    if (!data)
                        return -1;
                    if (dataSize < 1)
                        return -1;

                    // // for debug
                    // Serial.printf("W [%04X] ", address);
                    // for (size_t i = 0; i < dataSize; ++i)
                    // {
                    //     Serial.printf("%02X ", reinterpret_cast<const uint8_t *>(data)[i]);
                    // }
                    // Serial.println();

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
