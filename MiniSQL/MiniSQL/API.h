#pragma once
#include "CatalogManager.h"
#include "RecordManager.h"
#include "Utils.h"
#include <string>
#include <memory>
#include <vector>

class Value
{
public:
    virtual ~Value() {}
    virtual TableInfo::ValueProxy value(const TableInfo& info, const OffsetBlockProxy& block) = 0;
};

class TokenFieldValue : public Value
{
private:
    std::string _fieldName;
public:
    TokenFieldValue(const std::string& fieldName)
        : _fieldName(fieldName)
    {
    }
    TableInfo::ValueProxy value(const TableInfo& info, const OffsetBlockProxy& block) override
    {
        return info[block][_fieldName];
    }
};

class LiteralValue : public Value
{
private:
    std::unique_ptr<byte, ArrayDeleter> _value;
    const TypeInfo* _type;
public:
    LiteralValue(const std::string& value, const TypeInfo& type)
        : _value(new byte[type.size()])
        , _type(&type)
    {
        switch (_type->type())
        {
        case Int:
        {
            int iValue;
            std::istringstream(value) >> iValue;
            memcpy(_value.get(), &iValue, 4);
            break;
        }
        case Float:
        {
            float iValue;
            std::istringstream(value) >> iValue;
            memcpy(_value.get(), &iValue, 4);
            break;
        }
        case Chars:
        {
            memcpy(_value.get(), &value[0], std::min(_type->size(), value.size()));
            break;
        }
        default: break;
        }
    }

    TableInfo::ValueProxy value(const TableInfo& info, const OffsetBlockProxy& block) override
    {
        return TableInfo::ValueProxy(_value.get(), _type);
    }
};

class Comparison
{
protected:
    std::unique_ptr<Value> _lhs;
    std::unique_ptr<Value> _rhs;
public:
    enum class ComparisonType
    {
        Eq, Ne, Lt, Gt, Le, Ge
    };
    virtual ~Comparison() {}
    Comparison(Value* lhs, Value* rhs)
        : _lhs(lhs)
        , _rhs(rhs)
    {
    }
    virtual bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const = 0;
    virtual ComparisonType type() const = 0;
};

class EqComparison : public Comparison
{
public:
    EqComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const override
    {
        return _lhs->value(info, block) == _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Eq; }
};

class NeComparison : public Comparison
{
public:
    NeComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const override
    {
        return _lhs->value(info, block) != _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Ne; }
};

class GtComparison : public Comparison
{
public:
    GtComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const override
    {
        return _lhs->value(info, block) > _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Gt; }
};

class LtComparison : public Comparison
{
public:
    LtComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const override
    {
        return _lhs->value(info, block) < _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Lt; }
};

class GeComparison : public Comparison
{
public:
    GeComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const override
    {
        return _lhs->value(info, block) >= _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Gt; }
};

class LeComparison : public Comparison
{
public:
    LeComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const OffsetBlockProxy& block) const override
    {
        return _lhs->value(info, block) <= _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Le; }
};

class QueryListBase
{
protected:
    const TableInfo* _info;
    std::vector<std::unique_ptr<Comparison>> _comparisonNodes;
public:
    QueryListBase()
        : _info()
        , _comparisonNodes()
    {
    }
    virtual ~QueryListBase()
    {
    }
    void set_table(const std::string& table)
    {
        _info = &CatalogManager::instance().find_table(table);
    }
    void add_query(Comparison::ComparisonType type, const std::string& fieldName, const std::string& value)
    {
        std::unique_ptr<Comparison> comparison;
        auto lValue = new TokenFieldValue(fieldName);
        auto rValue = new LiteralValue(value, _info->field(fieldName).type_info());
        Comparison* expr;
        switch (type)
        {
        case Comparison::ComparisonType::Eq:
            expr = new EqComparison(lValue, rValue);
            break;
        case Comparison::ComparisonType::Ne:
            expr = new NeComparison(lValue, rValue);
            break;
        case Comparison::ComparisonType::Lt:
            expr = new LtComparison(lValue, rValue);
            break;
        case Comparison::ComparisonType::Gt:
            expr = new GtComparison(lValue, rValue);
            break;
        case Comparison::ComparisonType::Le:
            expr = new LeComparison(lValue, rValue);
            break;
        case Comparison::ComparisonType::Ge:
            expr = new GeComparison(lValue, rValue);
            break;
        default:
            throw InvalidComparisonType("invalid comparison type");
            break;
        }
        _comparisonNodes.emplace_back(expr);
    }
    virtual std::vector<BlockPtr> execute_linearly() const = 0;
};

class QueryList : public QueryListBase
{
public:
    QueryList()
        : QueryListBase()
    {
    }

    virtual std::vector<BlockPtr> execute_linearly() const override
    {
        auto& recMgr = RecordManager::instance();
        auto& recList = recMgr[_info->name()];
        auto listSize = recList.size();
        std::vector<BlockPtr> result;

        for (size_t i = 0; i != listSize; i++)
        {
            bool success = true;
            for (auto& cmpEntry : _comparisonNodes)
            {
                if (cmpEntry->evaluate(*_info, *recList[i]) == false)
                {
                    success = false;
                    break;
                }
            }
            if (success)
            {
                result.push_back(recList[i]);
            }
        }

        return result;
    }
};

class DeleteList : public QueryListBase
{
public:
    DeleteList()
        : QueryListBase()
    {
    }
    virtual std::vector<BlockPtr> execute_linearly() const override
    {
        auto& recMgr = RecordManager::instance();
        auto& recList = recMgr[_info->name()];

        for (size_t i = 0; i != recList.size();)
        {
            bool success = true;
            for (auto& cmpEntry : _comparisonNodes)
            {
                if (cmpEntry->evaluate(*_info, *recList[i]) == false)
                {
                    success = false;
                    break;
                }
            }
            if (success)
            {
                recList.erase(i);
            }
            else
            {
                i++;
            }
        }

        return{};
    }
};

class QueryResult : Uncopyable
{
public:
    virtual ~QueryResult()
    {
    }
    virtual const std::vector<std::string>& fields() const = 0;
    virtual const std::vector<size_t>& field_width() const = 0;
    virtual const boost::multi_array<std::string, 2>& results() const = 0;
    virtual size_t size() const = 0;

};

class SelectQueryResult : public QueryResult
{
private:
    std::vector<std::string> _fields;
    std::vector<size_t> _field_width;
    boost::multi_array<std::string, 2> _results;
    size_t _size;
public:
    SelectQueryResult(const TableInfo& info, std::vector<std::string> fields, std::vector<BlockPtr> results)
        : _fields(fields)
        , _field_width(3)
        , _results(boost::extents[results.size()][fields.size()])
        , _size(results.size())
    {
        for (size_t j = 0; j != fields.size(); j++)
        {
            _field_width[j] = fields[j].size();
        }
        for (size_t i = 0; i != results.size(); i++)
        {
            for (size_t j = 0; j != fields.size(); j++)
            {
                auto str = info[results[i]][fields[j]].to_string();
                _results[i][j] = str;
                if (_field_width[j] < str.size())
                {
                    _field_width[j] = str.size();
                }
            }
        }
    }
    virtual size_t size() const override { return _size; }
    virtual const std::vector<std::string>& fields() const override { return _fields; }
    virtual const std::vector<size_t>& field_width() const override { return _field_width; }
    virtual const boost::multi_array<std::string, 2>& results() const override { return _results; }
};

class TableCreater
{
private:
    std::string _tableName;
    size_t _primaryPos;
    size_t _indexPos;
    size_t _size;
    std::vector<TokenField> _fields;
public:
    TableCreater(const std::string& name)
        : _tableName(name)
        , _primaryPos(-1)
        , _indexPos(-1)
        , _size(0)
    {
    }

    uint16_t size() const { return (uint16_t)_size; }

    const std::string& table_name() const { return _tableName; }

    void add_field(const std::string& name, TypeInfo type, bool isUnique)
    {
        assert(locate_field(name) == -1);
        _fields.emplace_back(name, type, _size, isUnique);
        _size += type.size();
        if (_size > std::numeric_limits<uint16_t>::max())
        {
            throw InsuffcientSpace("too much fields");
        }
        if (isUnique)
        {
            _indexPos = _fields.size() - 1;
        }
    }

    void set_primary(const std::string& name)
    {
        auto pos = locate_field(name);
        assert(pos != -1);
        _primaryPos = pos;
        _indexPos = pos;
        _fields[pos]._isUnique = true;
    }

    size_t locate_field(const std::string& fieldName)
    {
        for (size_t i = 0; i != _fields.size(); i++)
        {
            if (_fields[i].name() == fieldName)
            {
                return i;
            }
        }
        return -1;
    }

    TableInfo create() const
    {
        return TableInfo(_tableName, _primaryPos, _indexPos, _size, _fields);
    }
};

class TupleBuilder
{
private:
    const TableInfo* _tableInfo;
    std::unique_ptr<byte, ArrayDeleter> _raw;
    size_t _iField;
public:
    TupleBuilder(const std::string& tableName)
        : _tableInfo(&CatalogManager::instance().find_table(tableName))
        , _raw(new byte[_tableInfo->entry_size()])
        , _iField(0)
    {
    }

    void set_value(const std::string& value, Type type)
    {
        if (_iField >= _tableInfo->fields().size())
        {
            throw SQLError("too many value");
        }

        if (!TypeInfo::is_convertible(type, _tableInfo->fields()[_iField].type_info().type()))
        {
            throw InvalidType("invalid type");
        }

        type = _tableInfo->fields()[_iField].type_info().type();

        switch (type)
        {
        case Int:
        {
            int iValue;
            std::istringstream(value) >> iValue;
            memcpy(_raw.get() + _tableInfo->fields()[_iField].offset(), &iValue, sizeof(iValue));
            break;
        }
        case Float:
        {
            float iValue;
            std::istringstream(value) >> iValue;
            memcpy(_raw.get() + _tableInfo->fields()[_iField].offset(), &iValue, sizeof(iValue));
            break;
        }
        case Chars:
        {
            memset(_raw.get() + _tableInfo->fields()[_iField].offset(), 0, _tableInfo->fields()[_iField].type_info().size());
            memcpy(_raw.get() + _tableInfo->fields()[_iField].offset(), &value[0], value.size());
            break;
        }
        default:
        {
            assert(("invalid type", 0));
            break;
        }
        }

        _iField++;
    }

    const std::string& table_name() const
    {
        return _tableInfo->name();
    }

    byte* get_field() const
    {
        return _raw.get();
    }
};

class StatementBuilder
{
protected:
    const TableInfo* _info;
    std::vector<std::string> _fields;
    std::string _tableName;
    std::unique_ptr<QueryListBase> _query;
public:
    StatementBuilder(QueryListBase* query)
        : _info(nullptr)
        , _fields()
        , _tableName()
        , _query(query)
    {
    }
    virtual ~StatementBuilder()
    {
    }
    void set_table(const std::string& tableName)
    {
        _info = &CatalogManager::instance().find_table(tableName);
        _tableName = tableName;
        _query->set_table(tableName);
        if (_fields.size() == 0)
        {
            for (const auto& elm : _info->fields())
            {
                _fields.push_back(elm.name());
            }
        }
        else
        {
            for (auto& fieldName : _fields)
            {
                if (std::find_if(_info->fields().begin(), _info->fields().end(), [&fieldName](const TokenField& field) {
                    return field.name() == fieldName;
                }) == _info->fields().end())
                {
                    throw InvalidField(fieldName.c_str());
                }
            }
        }
    }

    void add_condition(Comparison::ComparisonType type, const std::string& fieldName, const std::string& value)
    {
        _query->add_query(type, fieldName, value);
    }

    virtual std::shared_ptr<QueryResult> get_result() const = 0;

};

class SelectStatementBuilder : public StatementBuilder
{
public:
    SelectStatementBuilder()
        : StatementBuilder(new QueryList())
    {
    }

    void add_select_field(const std::string& fieldName)
    {
        _fields.push_back(fieldName);
    }

    virtual std::shared_ptr<QueryResult> get_result() const override
    {
        return std::make_shared<SelectQueryResult>(*_info, _fields, _query->execute_linearly());
    }
};

class DeleteStatementBuilder : public StatementBuilder
{
public:
    DeleteStatementBuilder()
        : StatementBuilder(new DeleteList())
    {
    }

    virtual std::shared_ptr<QueryResult> get_result() const override
    {
        _query->execute_linearly();
        return nullptr;
    }
};