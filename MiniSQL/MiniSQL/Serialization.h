#pragma once

#include "MemoryWriteStream.h"
#include "MemoryReadStream.h"

template<class T>
class Serializer
{
public:
    static T deserialize(MemoryReadStream& mrs)
    {
        T t;
        mrs >> t;
        return t;
    }

    static void serialize(MemoryWriteStream& mws, const T& value)
    {
        mws << value;
    }
};

