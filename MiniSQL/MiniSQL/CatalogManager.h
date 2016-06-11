#pragma once

#include "BufferManager.h"
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
    Comparator* _comparer;
public:
    static bool is_convertible(Type from, Type to)
    {
        return (from == to) || (from == Int && to == Float);
    }
    TypeInfo(Type type, size_t length)
        : _type(type)
        , _size(length)
        , _comparer(Comparator::from_type(type, length))
    {
    }
    explicit TypeInfo(Type type)
        : _type(type)
        , _size(sizeof(int))
        , _comparer(Comparator::from_type(type))
    {
    }

    Type type() const { return _type; }

    size_t size() const { return _size; }

    std::string name() const
    {
        switch(_type)
        {
        case Int:
            return "int";
            break;
        case Float: 
            return "float";
            break;
        case Chars: 
            return "char(" + std::to_string(_size)+ ")";
            break;
        default: 
            return "invalid";
            break;
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

class TokenField
{
    friend class TableCreater;
    friend class TableInfo;
    friend class Serializer<TokenField>;
private:
    std::string _name;
    TypeInfo _info;
    size_t _offset;
    bool _isUnique;
public:
    TokenField(const std::string& name, const TypeInfo& info, size_t offset, bool isUnique)
        : _name(name)
        , _info(info)
        , _offset(offset)
        , _isUnique(isUnique)
    {
    }
    const std::string& name() const { return _name; }
    const TypeInfo& type_info() const { return _info; }
    size_t offset() const { return _offset; }
    bool is_unique() const { return _isUnique; }
};

template<>
class Serializer<TokenField>
{
public:
    static TokenField deserialize(MemoryReadStream& mrs)
    {
        std::string name;
        mrs >> name;
        TypeInfo info = Serializer<TypeInfo>::deserialize(mrs);
        size_t offset;
        mrs >> offset;
        bool isUnique;
        mrs >> isUnique;
        return TokenField(name, info, offset, isUnique);
    }
    static void serialize(MemoryWriteStream& mws, const TokenField& value)
    {
        mws << value._name;
        Serializer<TypeInfo>::serialize(mws, value._info);
        mws << value._offset;
        mws << value._isUnique;
    }
};

class TableInfo
{
    friend class TableCreater;
    friend class Serializer<TableInfo>;
private:
    std::string _name;
    size_t _primaryPos;
    size_t _indexPos;
    size_t _size;
    std::vector<TokenField> _fields;

    TableInfo(const std::string& name, size_t primaryPos, size_t indexPos, size_t size, const std::vector<TokenField> _fields)
        : _name(name)
        , _primaryPos(primaryPos)
        , _indexPos(indexPos)
        , _size(size)
        , _fields(_fields)
    {
    }
public:
    class TupleProxy;
    class ValueProxy : Uncopyable
    {
        friend class TupleProxy;
    private:
        byte* _raw;
        const TypeInfo* _info;
    public:
        ValueProxy(byte* raw, const TypeInfo* info)
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

        std::string to_string() const
        {
            switch (_info->type())
            {
            case Int:
                return std::to_string(*reinterpret_cast<int*>(_raw));
                break;
            case Float:
                return std::to_string(*reinterpret_cast<float*>(_raw));
                break;
            case Chars:
                return{_raw, _raw + _info->size()};
                break;
            default:
                return "invalid type";
                break;
            }
        }

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

        const TypeInfo& type_info() const { return *_info; }

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
        BlockPtr _block;
        const TableInfo* _info;
        TupleProxy(BlockPtr block, const TableInfo* info)
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
        ValueProxy operator[](const std::string& keyName)
        {
            auto place = std::find_if(_info->_fields.begin(), _info->_fields.end(), [&keyName](const TokenField& item) {
                return item._name == keyName;
            });
            if (place == _info->_fields.end())
            {
                throw InvalidKey(("invalid key name: " + keyName).c_str());
            }
            return{(*_block).raw_ptr() + place->_offset, &place->_info};
        }
    };

    class RawTupleProxy : Uncopyable
    {
        friend class TableInfo;
    private:
        byte* _raw;
        const TableInfo* _info;
        RawTupleProxy(byte* block, const TableInfo* info)
            : _raw(block)
            , _info(info)
        {
        }
    public:
        RawTupleProxy(RawTupleProxy&& other)
            : _raw(other._raw)
            , _info(other._info)
        {
            other._raw = nullptr;
            other._info = nullptr;
        }

        const TableInfo& table_info() const { return *_info; }
        
        ValueProxy operator[](const std::string& keyName) const
        {
            auto place = std::find_if(_info->_fields.begin(), _info->_fields.end(), [&keyName](const TokenField& item) {
                return item._name == keyName;
            });
            if (place == _info->_fields.end())
            {
                throw InvalidKey(("invalid key name: " + keyName).c_str());
            }
            return{_raw + place->_offset, &place->_info};
        }
    };

    const std::string& name() const { return _name; }

    size_t primary_pos() const { return _primaryPos; }

    size_t index_pos() const { return _indexPos; }

    size_t entry_size() const { return _size; }

    const std::vector<TokenField>& fields() const { return _fields; }

    TupleProxy operator[](const BlockPtr& ptr) const
    {
        return{ptr, this};
    }

    TupleProxy operator[](const BufferBlock& block) const
    {
        return{block.ptr(), this};
    }

    RawTupleProxy operator[](byte* raw) const
    {
        return RawTupleProxy{raw, this};
    }

    const TokenField& field(const std::string& fieldName) const
    {
        for (auto& entry : _fields)
        {
            if (entry._name == fieldName)
            {
                return entry;
            }
        }
        throw InvalidKey(fieldName.c_str());
    }
};

template<>
class Serializer<TableInfo>
{
public:
    static TableInfo deserialize(MemoryReadStream& mrs)
    {
        std::string tableName;
        size_t primaryPos;
        size_t indexPos;
        size_t size;
        size_t fieldCount;

        mrs >> tableName;
        mrs >> primaryPos;
        mrs >> indexPos;
        mrs >> size;
        mrs >> fieldCount;

        std::vector<TokenField> fields;
        for (size_t i = 0; i != fieldCount; i++)
        {
            fields.push_back(Serializer<TokenField>::deserialize(mrs));
        }
        return TableInfo(tableName, primaryPos, indexPos, size, fields);
    }

    static void serialize(MemoryWriteStream& mws, const TableInfo& value)
    {
        mws << value._name << value._primaryPos << value._indexPos << value._size << value._fields.size();
        for (auto& entry : value._fields)
        {
            Serializer<TokenField>::serialize(mws, entry);
        }
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

    const static char* const FileName; /*= "CatalogManagerMeta"*/

    ~CatalogManager();

    void add_table(const TableInfo& tableInfo)
    {
        if (locate_table(tableInfo.name()) != -1)
        {
            throw SQLError(("Error: Duplicated table name '" + tableInfo.name() + "'").c_str());
        }
        _tables.push_back(tableInfo);
    }

    const TableInfo& find_table(const std::string& tableName) const
    {
        size_t i = locate_table(tableName);
        if (i == -1)
        {
            throw TableNotExist(tableName.c_str());
        }
        return _tables[i];
    }

    size_t locate_table(const std::string& tableName) const
    {
        for (size_t i = 0; i != _tables.size(); i++)
        {
            if (_tables[i].name() == tableName)
            {
                return i;
            }
        }
        return -1;
    }

    void remove_table(const std::string& tableName)
    {
        size_t i = locate_table(tableName);
        _tables.erase(_tables.begin() + i);
    }

    const std::vector<TableInfo>& tables() const { return _tables; }

private:
    std::vector<TableInfo> _tables;

    CatalogManager();
};

