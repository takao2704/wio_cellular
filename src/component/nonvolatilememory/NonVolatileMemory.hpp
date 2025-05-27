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

            class NonVolatileMemory : public NonVolatileMemoryInterface
            {
            private:
                NonVolatileMemoryInterface &Interface_;
                uintptr_t BaseAddress_;
                size_t Size_;

            public:
                NonVolatileMemory(NonVolatileMemoryInterface &interface, uintptr_t baseAddress, size_t size) : Interface_(interface),
                                                                                                               BaseAddress_(baseAddress),
                                                                                                               Size_(size)
                {
                }

                size_t size() const override
                {
                    return Size_;
                }

                long read(uintptr_t address, void *data, size_t dataSize) const override
                {
                    if (Size_ < address + dataSize)
                        return -1;

                    return Interface_.read(BaseAddress_ + address, data, dataSize);
                }

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
