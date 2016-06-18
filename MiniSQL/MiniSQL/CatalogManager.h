#pragma once

#include "BufferManager.h"
#include "TypeInfo.h"

class FieldInfo
{
    friend class TableCreater;
    friend class TableInfo;
    friend class Serializer<FieldInfo>;
private:
    std::string _name;
    TypeInfo    _info;
    size_t      _offset;
    bool        _isUnique;
public:
    FieldInfo(const std::string& name, const TypeInfo& info, size_t offset, bool isUnique)
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
class Serializer<FieldInfo>
{
public:
    static FieldInfo deserialize(MemoryReadStream& mrs)
    {
        std::string name;
        mrs >> name;
        TypeInfo info = Serializer<TypeInfo>::deserialize(mrs);
        size_t offset;
        mrs >> offset;
        bool isUnique;
        mrs >> isUnique;
        return FieldInfo(name, info, offset, isUnique);
    }
    static void serialize(MemoryWriteStream& mws, const FieldInfo& value)
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
    size_t _size;
    std::vector<FieldInfo> _fields;

    TableInfo(const std::string& name, size_t primaryPos, size_t size, const std::vector<FieldInfo> _fields)
        : _name(name)
        , _primaryPos(primaryPos)
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
        //构造函数
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

        //输出为字符串
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

        //与另一个ValueProxy比较
        int compare(const ValueProxy& other) const
        {
            if (*_info == *other._info)
            {
                return (*_info->_comparator)(_raw, other._raw);
            }
            throw InvalidType("compare type doesn't match");
        }

        //比较函数
        bool operator==(const ValueProxy& other) const { return compare(other) == 0; }

        bool operator!=(const ValueProxy& other) const { return compare(other) != 0; }

        bool operator>(const ValueProxy& other) const { return compare(other) > 0; }

        bool operator<(const ValueProxy& other) const { return compare(other) < 0; }

        bool operator>=(const ValueProxy& other) const { return compare(other) >= 0; }

        bool operator<=(const ValueProxy& other) const { return compare(other) <= 0; }

        //获取相关属性
        const TypeInfo& type_info() const { return *_info; }

        Type type() const { return _info->_type; }

        size_t size() const { return _info->_size; }

        //将内容转换为对应类型
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
        TupleProxy(const BlockPtr& block, const TableInfo* info)
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
        //获取对应字段名的ValueProxy
        ValueProxy operator[](const std::string& keyName)
        {
            auto place = std::find_if(_info->_fields.begin(), _info->_fields.end(), [&keyName](const FieldInfo& item) {
                return item._name == keyName;
            });
            if (place == _info->_fields.end())
            {
                throw InvalidKey(("invalid key name: " + keyName).c_str());
            }
            return{_block->raw_ptr() + place->_offset, &place->_info};
        }
    };

    class RawTupleProxy : Uncopyable
    {
        friend class TableInfo;
    private:
        byte* _raw;
        const TableInfo* _info;
    public:
        RawTupleProxy(byte* block, const TableInfo* info)
            : _raw(block)
            , _info(info)
        {
        }
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
            auto place = std::find_if(_info->_fields.begin(), _info->_fields.end(), [&keyName](const FieldInfo& item) {
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

    size_t entry_size() const { return _size; }

    const std::vector<FieldInfo>& fields() const { return _fields; }

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

    const FieldInfo& field(const std::string& fieldName) const
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
        size_t size;
        size_t fieldCount;

        mrs >> tableName;
        mrs >> primaryPos;
        mrs >> size;
        mrs >> fieldCount;

        std::vector<FieldInfo> fields;
        for (size_t i = 0; i != fieldCount; i++)
        {
            fields.push_back(Serializer<FieldInfo>::deserialize(mrs));
        }
        return TableInfo(tableName, primaryPos, size, fields);
    }

    static void serialize(MemoryWriteStream& mws, const TableInfo& value)
    {
        mws << value._name << value._primaryPos << value._size << value._fields.size();
        for (auto& entry : value._fields)
        {
            Serializer<FieldInfo>::serialize(mws, entry);
        }
    }
};

class CatalogManager : Uncopyable
{
public:
    //获取管理器实例
    static CatalogManager& instance()
    {
        static CatalogManager instance;
        return instance;
    }

    const static char* const FileName; /*= "CatalogManagerMeta"*/

    ~CatalogManager();

    //丢弃表信息
    void drop_info(const std::string& tableName)
    {
        auto iter = std::find_if(_tables.begin(), _tables.end(), [&](const auto& elm) {
            return elm.name() == tableName;
        });
        _tables.erase(iter);
    }

    //增加表信息
    void add_table(const TableInfo& tableInfo)
    {
        if (locate_table(tableInfo.name()) != -1)
        {
            throw SQLError(("Error: Duplicated table name '" + tableInfo.name() + "'").c_str());
        }
        _tables.push_back(tableInfo);
    }

    //查找表信息
    TableInfo& find_table(const std::string& tableName)
    {
        size_t i = locate_table(tableName);
        if (i == -1)
        {
            throw TableNotExist(tableName.c_str());
        }
        return _tables[i];
    }

    //定位表信息在表信息数组中的位置
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

    //移除表信息
    void remove_table(const std::string& tableName)
    {
        drop_info(tableName);
    }

    //获取表信息数组
    const std::vector<TableInfo>& tables() const { return _tables; }

private:
    std::vector<TableInfo> _tables;

    CatalogManager();
};

