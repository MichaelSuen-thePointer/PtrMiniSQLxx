#pragma once

#include "BufferManager.h"

class InvalidKey : public std::runtime_error
{
public:
    explicit InvalidKey(const char* msg)
        : std::runtime_error(msg)
    {
    }
};

class InvalidType : public std::runtime_error
{
public:
    explicit InvalidType(const char* msg)
        : std::runtime_error(msg)
    {
    }
};

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
        auto v1 = *reinterpret_cast<const float*>(a);
        auto v2 = *reinterpret_cast<const float*>(b);
        if(v1 > v2)
        {
            return 1;
        }
        if(v1 < v2)
        {
            return -1;
        }
        return 0;
    }
protected:
    FloatComparer() = default;
};

struct CharsComparer : Comparer
{
    friend struct Comparer;
    size_t size;
    virtual int operator()(const byte* a, const byte* b) override
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
    friend class TableInfo;
private:
    Type _type;
    size_t _size;
    std::shared_ptr<Comparer> _comparer;
public:
    TypeInfo(Type type, size_t length)
        : _type(type)
        , _size(length)
        , _comparer(Comparer::from_type(type, length))
    {
    }
    explicit TypeInfo(Type type)
        : _type(type)
        , _size(sizeof(int))
        , _comparer(Comparer::from_type(type))
    {
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

class KeyItem
{
    friend class TableInfo;
private:
    std::string _name;
    TypeInfo _info;
    size_t _offset;
    bool _isUnique;
public:
    KeyItem(const std::string& name, const TypeInfo& info, size_t offset, bool isUnique)
        : _name(name)
        , _info(info)
        , _offset(offset)
        , _isUnique(isUnique)
    {
    }
};

class TableInfo
{
private:
    std::string _name;
    size_t _primaryPos;
    size_t _indexPos;
    std::vector<KeyItem> _keys;
    size_t _totalSize;
public:
    class TupleProxy;
    class ValueProxy : Uncopyable
    {
        friend class TupleProxy;
    private:
        byte* _raw;
        TypeInfo* _info;
        ValueProxy(byte* raw, TypeInfo* info)
            : _raw(raw)
            , _info(info)
        {
        }
        ValueProxy(ValueProxy&& other)
            : _raw(other._raw)
            , _info(other._info)
        {
            other._raw = nullptr;
            other._info = nullptr;
        }
    public:
        int compare(const ValueProxy& other) const
        {
            if (*_info == *other._info)
            {
                return (*_info->_comparer)(_raw, other._raw);
            }
            throw InvalidType("compare type doesn't match");
        }
        bool operator==(const ValueProxy& other) const { return compare(other) == 0; }
        bool operator!=(const ValueProxy& other) const { return compare(other) != 0; }
        bool operator>(const ValueProxy& other) const { return compare(other) > 0; }
        bool operator<(const ValueProxy& other) const { return compare(other) < 0; }
        bool operator>=(const ValueProxy& other) const { return compare(other) >= 0; }
        bool operator<=(const ValueProxy& other) const { return compare(other) <= 0; }
        Type type() const { return _info->_type; }
        size_t size() const { return _info->_size; }
        int as_int() const { return *reinterpret_cast<int*>(_raw); }
        float as_float() const { return *reinterpret_cast<float*>(_raw); }
        std::string as_str() const { return std::string(reinterpret_cast<char*>(_raw), reinterpret_cast<char*>(_raw) + _info->_size); }
    };

    class TupleProxy : Uncopyable
    {
        friend class TableInfo;
    private:
        mutable BlockPtr _block;
        TableInfo* _info;
        TupleProxy(BlockPtr block, TableInfo* info)
            : _block(block)
            , _info(info)
        {
        }
    public:
        TupleProxy(TupleProxy&& other)
            : _block(other._block)
            , _info(other._info)
        {
            other._block = nullptr;
            other._info = nullptr;
        }
        ValueProxy operator[](const std::string& keyName) const
        {
            auto place = std::find_if(_info->_keys.begin(), _info->_keys.end(), [&keyName](const KeyItem& item) {
                return item._name == keyName;
            });
            if (place == _info->_keys.end())
            {
                throw InvalidKey(("invalid key name: " + keyName).c_str());
            }
            return{ _block->raw_ptr() + place->_offset, &place->_info };
        }
    };

    TableInfo(const std::string& name, const std::vector<std::tuple<std::string, TypeInfo, bool>>& keys, size_t primaryPos = -1)
        : _name(name)
        , _primaryPos(primaryPos == -1 ? 0 : primaryPos)
        , _indexPos(primaryPos == -1 ? 0 : primaryPos)
        , _keys()
        , _totalSize()
    {
        const size_t blockSize = BufferBlock::BlockSize;
        size_t offset = 0;
        _keys.reserve(primaryPos == -1 ? keys.size() : keys.size() + 1);
        if (primaryPos == -1)
        {
            _keys.emplace_back("_auto_primary_key_" + name, TypeInfo(Int), offset, true);
            offset += 4;
        }

        for (auto& key : keys)
        {
            _keys.emplace_back(std::get<0>(key), std::get<1>(key), offset, std::get<2>(key));
            offset += std::get<1>(key)._size;
            if (offset >= blockSize)
            {
                throw InsuffcientSpace("total size of keys exceeded 4096 bytes.");
            }
        }
        _totalSize = offset;
    }

    const std::string& name() const { return _name; }

    TupleProxy operator[](const BlockPtr& ptr)
    {
        return{ ptr, this };
    }

    TupleProxy operator[](const BufferBlock& block)
    {
        return{ block.ptr(), this };
    }
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

