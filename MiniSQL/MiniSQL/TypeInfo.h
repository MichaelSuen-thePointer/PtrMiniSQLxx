#pragma once

#include "MemoryWriteStream.h"
#include "MemoryReadStream.h"
#include "Serialization.h"

enum Type
{
    Int,
    Float,
    Chars
};

struct Comparator
{
    virtual int operator()(const byte* a, const byte* b) = 0;
    virtual ~Comparator() = default;
    static Comparator* from_type(Type type, size_t length);
    static Comparator* from_type(Type type);
};

struct IntComparator : Comparator
{
    friend struct Comparator;
    int operator()(const byte* a, const byte* b) override
    {
        return *reinterpret_cast<const int*>(a) - *reinterpret_cast<const int*>(b);
    }
protected:
    IntComparator() = default;
};

struct FloatComparator : Comparator
{
    friend struct Comparator;

    int operator()(const byte* a, const byte* b) override
    {
        auto v1 = *reinterpret_cast<const float*>(a);
        auto v2 = *reinterpret_cast<const float*>(b);
        if (v1 > v2)
        {
            return 1;
        }
        if (v1 < v2)
        {
            return -1;
        }
        return 0;
    }
protected:
    FloatComparator() = default;
};

struct CharsComparator : Comparator
{
    friend struct Comparator;
    size_t size;
    virtual int operator()(const byte* a, const byte* b) override
    {
        return strncmp(reinterpret_cast<const char*>(a), reinterpret_cast<const char*>(b), size);
    }
protected:
    explicit CharsComparator(size_t _size)
        : size(_size)
    {
    }
};

class TypeInfo
{
    friend class TableInfo;
    friend class Serializer<TypeInfo>;
private:
    Type _type;
    size_t _size;
    Comparator* _comparator;
public:
    static bool is_convertible(Type from, Type to)
    {
        return (from == to) || (from == Int && to == Float);
    }
    TypeInfo(Type type, size_t length)
        : _type(type)
        , _size(length)
        , _comparator(Comparator::from_type(type, length))
    {
    }
    explicit TypeInfo(Type type)
        : _type(type)
        , _size(sizeof(int))
        , _comparator(Comparator::from_type(type))
    {
    }

    Type type() const { return _type; }

    size_t size() const { return _size; }

    Comparator* comparator() const { return _comparator; }

    std::string name() const
    {
        switch (_type)
        {
        case Int:
            return "int";
        case Float:
            return "float";
        case Chars:
            return "char(" + std::to_string(_size) + ")";
        default:
            return "invalid";
        }
    }

    bool operator==(const TypeInfo& other) const
    {
        return _type == other._type && _size == other._size;
    }

    bool operator!=(const TypeInfo& other) const
    {
        return !(*this == other);
    }
};

template<>
class Serializer<TypeInfo>
{
public:
    static TypeInfo deserialize(MemoryReadStream& mrs)
    {
        Type type;
        mrs >> type;
        if (type == Chars)
        {
            size_t size;
            mrs >> size;
            return TypeInfo(type, size);
        }
        return TypeInfo(type);
    }

    static void serialize(MemoryWriteStream& mws, const TypeInfo& value)
    {
        mws << value._type;
        if (value._type == Chars)
        {
            mws << value._size;
        }
    }
};
