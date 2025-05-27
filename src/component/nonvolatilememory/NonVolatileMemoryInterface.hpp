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

            class NonVolatileMemoryInterface
            {
            public:
                virtual size_t size() const = 0;
                virtual long read(uintptr_t address, void *data, size_t dataSize) const = 0;
                virtual long write(uintptr_t address, const void *data, size_t dataSize) = 0;
            };

        }
    }
}

#endif // NONVOLATILEMEMORYINTERFACE_HPP
