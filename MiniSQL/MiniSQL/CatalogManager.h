#pragma once

#include "BufferManager.h"
enum Type
{
    Int,
    Float,
    Chars
};

struct Comparer
{
    virtual int operator()(const byte* a, const byte* b) = 0;
    virtual ~Comparer() = default;
    static std::shared_ptr<Comparer> from_type(Type type, size_t length);
    static std::shared_ptr<Comparer> from_type(Type type);
};

struct IntComparer : Comparer
{
    friend struct Comparer;
    int operator()(const byte* a, const byte* b) override
    {
        return *reinterpret_cast<const int*>(a) - *reinterpret_cast<const int*>(b);
    }
protected:
    IntComparer() = default;
};

struct FloatComparer : Comparer
{
    friend struct Comparer;

    int operator()(const byte* a, const byte* b) override
    {
        return std::less<float>()(*reinterpret_cast<const float*>(a), *reinterpret_cast<const float*>(b));
    }
protected:
    FloatComparer() = default;
};

struct CharsComparer : Comparer
{
    friend struct Comparer;
    size_t size;
    int operator()(const byte* a, const byte* b) override
    {
        return strncmp(reinterpret_cast<const char*>(a), reinterpret_cast<const char*>(b), size);
    }
protected:
    explicit CharsComparer(size_t _size)
        : size(_size)
    {
    }
};

class TypeInfo
{
private:
    Type _type;
    size_t _length;
    std::shared_ptr<Comparer> _comparer;
public:
    TypeInfo(Type type, size_t length)
        : _type(type)
        , _length(length)
        , _comparer(Comparer::from_type(type, length))
    {
    }
    explicit TypeInfo(Type type)
        : _type(type)
        , _length(sizeof(int))
        , _comparer(Comparer::from_type(type))
    {
    }
};

class RecordItem
{
private:
    std::string _name;
    TypeInfo _info;
    BlockPtr _base;
    size_t _offset;
public:

};

class TableInfo
{
private:
    std::string _name;
    std::size_t _primaryPos;
    std::size_t _indexPos;
    std::vector<RecordItem> _records;

};

class CatalogManager : Uncopyable
{
public:
    static CatalogManager& instance()
    {
        static CatalogManager instance;
        return instance;
    }

private:
    std::vector<TableInfo> _tables;
    
    CatalogManager();


};

