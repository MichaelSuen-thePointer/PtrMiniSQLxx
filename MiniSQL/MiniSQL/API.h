#pragma once
#include "CatalogManager.h"
#include "RecordManager.h"
#include "IndexManager.h"

class Value
{
public:
    virtual ~Value() {}
    virtual TableInfo::ValueProxy value(const TableInfo& info, const BlockPtr& ptr) = 0;
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
    TableInfo::ValueProxy value(const TableInfo& info, const BlockPtr& ptr) override
    {
        return info[ptr][_fieldName];
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
        memset(_value.get(), 0, type.size());
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

    TableInfo::ValueProxy value(const TableInfo& info, const BlockPtr& ptr) override
    {
        return get_value();
    }

    TableInfo::ValueProxy get_value() const
    {
        return TableInfo::ValueProxy(_value.get(), _type);
    }

    byte* raw() const
    {
        return _value.get();
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
    virtual bool evaluate(const TableInfo& info, const BlockPtr& ptr) const = 0;
    virtual ComparisonType type() const = 0;
};

class EqComparison : public Comparison
{
public:
    EqComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(const TableInfo& info, const BlockPtr& ptr) const override
    {
        return _lhs->value(info, ptr) == _rhs->value(info, ptr);
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
    bool evaluate(const TableInfo& info, const BlockPtr& ptr) const override
    {
        return _lhs->value(info, ptr) != _rhs->value(info, ptr);
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
    bool evaluate(const TableInfo& info, const BlockPtr& ptr) const override
    {
        return _lhs->value(info, ptr) > _rhs->value(info, ptr);
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
    bool evaluate(const TableInfo& info, const BlockPtr& ptr) const override
    {
        return _lhs->value(info, ptr) < _rhs->value(info, ptr);
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
    bool evaluate(const TableInfo& info, const BlockPtr& ptr) const override
    {
        return _lhs->value(info, ptr) >= _rhs->value(info, ptr);
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
    bool evaluate(const TableInfo& info, const BlockPtr& ptr) const override
    {
        return _lhs->value(info, ptr) <= _rhs->value(info, ptr);
    }
    ComparisonType type() const override { return ComparisonType::Le; }
};

class QueryListBase
{
protected:
    const TableInfo* _info;
    std::vector<std::unique_ptr<Comparison>> _comparisonNodes;
    std::map<std::string,
        std::pair<std::shared_ptr<LiteralValue>, std::shared_ptr<LiteralValue>>> _indexQueries;
    bool _isValid;
public:
    QueryListBase()
        : _info()
        , _comparisonNodes()
        , _indexQueries()
        , _isValid(true)
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
        if (!_isValid)
        {
            return;
        }
        auto& indexMgr = IndexManager::instance();
        if (indexMgr.has_index(_info->name(), fieldName))
        {
            add_index_query(type, fieldName, value);
        }
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

    void add_index_query(Comparison::ComparisonType type, const std::string& fieldName, const std::string& value)
    {
        auto newValue = std::make_shared<LiteralValue>(value, _info->field(fieldName).type_info());
        switch (type)
        {
        case Comparison::ComparisonType::Eq:
            if (_indexQueries[fieldName].first != nullptr)
            {
                if (newValue->get_value() < _indexQueries[fieldName].first->get_value())
                {
                    _isValid = false;
                    break;
                }
            }
            if (_indexQueries[fieldName].second != nullptr)
            {
                if (newValue->get_value() > _indexQueries[fieldName].first->get_value())
                {
                    _isValid = false;
                    break;
                }
            }
            _indexQueries[fieldName].first = newValue;
            _indexQueries[fieldName].second = newValue;
            break;
        case Comparison::ComparisonType::Lt:
        case Comparison::ComparisonType::Le:
            if (_indexQueries[fieldName].first != nullptr)
            {
                if (newValue->get_value() < _indexQueries[fieldName].first->get_value())
                {
                    _isValid = false;
                    break;
                }
            }
            if (_indexQueries[fieldName].second != nullptr)
            {
                if (newValue->get_value() < _indexQueries[fieldName].second->get_value())
                {
                    _indexQueries[fieldName].second = newValue;
                }
            }
            else
            {
                _indexQueries[fieldName].second = newValue;
            }
            break;
        case Comparison::ComparisonType::Gt:
        case Comparison::ComparisonType::Ge:
            if (_indexQueries[fieldName].first != nullptr)
            {
                if (newValue->get_value() > _indexQueries[fieldName].first->get_value())
                {
                    _indexQueries[fieldName].first = newValue;
                    break;
                }
            }
            else
            {
                _indexQueries[fieldName].first = newValue;
            }
            if (_indexQueries[fieldName].second != nullptr)
            {
                if (newValue->get_value() > _indexQueries[fieldName].second->get_value())
                {
                    _isValid = false;
                    break;
                }
            }
            break;
        case Comparison::ComparisonType::Ne: break;
        default: break;
        }
    }

    virtual std::vector<BlockPtr> execute_linearly() const = 0;
    virtual std::vector<BlockPtr> execute() const = 0;
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
        if (!_isValid)
        {
            return{};
        }
        auto& recMgr = RecordManager::instance();
        auto& recList = recMgr[_info->name()];
        auto listSize = recList.size();
        std::vector<BlockPtr> result;

        for (size_t i = 0; i != listSize; i++)
        {
            bool success = true;
            for (auto& cmpEntry : _comparisonNodes)
            {
                if (cmpEntry->evaluate(*_info, recList[i]) == false)
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

    std::vector<BlockPtr> execute() const override
    {
        if (!IndexManager::instance().has_index(_info->name()))
        {
            return execute_linearly();
        }
        if (!_isValid)
        {
            return{};
        }

        std::vector<BlockPtr> entries;

        if (_indexQueries.size() == 0)
        {
            entries = IndexManager::instance().search(_info->name(), _info->fields()[_info->primary_pos()].name(), nullptr, nullptr);
        }
        bool first = true;
        for (auto& idxQuery : _indexQueries)
        {
            auto temp = IndexManager::instance().search(_info->name(), idxQuery.first, idxQuery.second.first ? idxQuery.second.first->raw() : nullptr,
                                                        idxQuery.second.second ? idxQuery.second.second->raw() : nullptr);
            if (first)
            {
                entries = temp;
                first = false;
            }
            else
            {
                for (const auto& pEntry : temp)
                {
                    if (std::find(entries.begin(), entries.end(), pEntry) == entries.end())
                    {
                        entries.push_back(pEntry);
                    }
                }
            }
            if (entries.size() == RecordManager::instance().find_table(_info->name()).size())
            {
                break;
            }
        }
        for (size_t i = 0; i != entries.size(); )
        {
            bool success = true;
            for (auto& cmpEntry : _comparisonNodes)
            {
                if (cmpEntry->evaluate(*_info, entries[i]) == false)
                {
                    success = false;
                    break;
                }
            }
            if (!success)
            {
                entries.erase(entries.begin() + i);
            }
            else
            {
                i++;
            }
        }
        return entries;
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

        int debugCount = 0;

        for (size_t i = 0; i != recList.size();)
        {
            bool success = true;
            for (auto& cmpEntry : _comparisonNodes)
            {
                if (cmpEntry->evaluate(*_info, recList[i]) == false)
                {
                    success = false;
                    break;
                }
            }
            if (success)
            {
                debugCount++;
                auto indexedFields = IndexManager::instance().indexed_fields(_info->name());
                for (const auto& fieldName : indexedFields)
                {
                    IndexManager::instance().remove(_info->name(), fieldName, recList[i]->raw_ptr() + _info->field(fieldName).offset());
                }
                recList.erase(i);
            }
            else
            {
                i++;
            }
        }

        return{};
    }

    std::vector<BlockPtr> execute() const override
    {
        return execute_linearly();
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
    size_t _size;
    std::vector<FieldInfo> _fields;
public:
    TableCreater(const std::string& name)
        : _tableName(name)
        , _primaryPos(-1)
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
        if (_size > RecordManager::MaxRecordSize)
        {
            throw InsuffcientSpace("too much fields");
        }
    }

    void set_primary(const std::string& name)
    {
        auto pos = locate_field(name);
        assert(pos != -1);
        _primaryPos = pos;
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

    void execute() const
    {
        CatalogManager::instance().add_table({_tableName, _primaryPos, (uint16_t)_size, _fields});
        RecordManager::instance().create_table(_tableName, (uint16_t)_size);
        if (_primaryPos != -1)
        {
            IndexManager::instance().create_index(_tableName, _fields[_primaryPos].name(), _fields[_primaryPos].name(), _fields[_primaryPos].type_info());
        }
    }
};

class TableInserter
{
private:
    const TableInfo* _tableInfo;
    std::unique_ptr<byte, ArrayDeleter> _raw;
    size_t _iField;
public:
    TableInserter(const std::string& tableName)
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

    void linearly_check_unique_field(const std::vector<std::string>& nonindexedUniqueFieldNames) const
    {
        auto& rec = RecordManager::instance().find_table(_tableInfo->name());
        for (size_t iRec = 0; iRec != rec.size(); iRec++)
        {
            for (const auto& fieldName : nonindexedUniqueFieldNames)
            {
                if ((*_tableInfo)[rec[iRec]][fieldName] == (*_tableInfo)[_raw.get()][fieldName])
                {
                    throw SQLError(("not unique: " + fieldName).c_str());
                }
            }
        }
    }

    void execute()
    {
        auto& im = IndexManager::instance();
        std::vector<std::string> nonindexedUniqueFields;
        nonindexedUniqueFields.reserve(_tableInfo->fields().size());
        for (const auto& field : _tableInfo->fields())
        {
            if (im.has_index(_tableInfo->name(), field.name()))
            {
                if (!im.check_index_unique(_tableInfo->name(), field.name(), _raw.get() + field.offset()))
                {
                    throw SQLError("not unique: primary");
                }
            }
            else if (field.is_unique())
            {
                nonindexedUniqueFields.push_back(field.name());
            }
        }
        linearly_check_unique_field(nonindexedUniqueFields);
        auto entryPtr = RecordManager::instance().insert_entry(_tableInfo->name(), _raw.get());
        auto fields = im.indexed_fields(_tableInfo->name());
        for (const auto& fieldName : fields)
        {
            im.insert(_tableInfo->name(), fieldName, _raw.get() + _tableInfo->field(fieldName).offset(), entryPtr);
        }
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
                if (std::find_if(_info->fields().begin(), _info->fields().end(), [&fieldName](const FieldInfo& field) {
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
        return std::make_shared<SelectQueryResult>(*_info, _fields, _query->execute());
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
        _query->execute();
        return nullptr;
    }
};

class TableDroper
{
    const TableInfo* _info;
public:
    TableDroper()
        : _info(nullptr)
    {
    }
    void set_table(const std::string& table)
    {
        _info = &CatalogManager::instance().find_table(table);
    }
    void execute()
    {
        RecordManager::instance().drop_record(_info->name());
        if (IndexManager::instance().has_index(_info->name()))
        {
            IndexManager::instance().drop_index(_info->name());
        }
        CatalogManager::instance().drop_info(_info->name());
    }
};

class IndexDroper
{
    TableInfo* _info;
    std::string _indexName;
public:
    IndexDroper()
        : _info(nullptr)
    {
    }
    void set_table(const std::string& table)
    {
        _info = &CatalogManager::instance().find_table(table);
    }
    void set_name(const std::string& indexName)
    {
        _indexName = indexName;
    }
    void execute()
    {
        IndexManager::instance().drop_index(_info->name(), _indexName);
    }
};

class IndexCreater
{
    TableInfo* _info;
    const FieldInfo* _field;
    std::string _indexName;
public:
    IndexCreater()
        : _info(nullptr)
        , _field(nullptr)
    {
    }
    void set_table(const std::string& table)
    {
        _info = &CatalogManager::instance().find_table(table);
    }
    void set_name(const std::string& indexName)
    {
        _indexName = indexName;
    }
    void set_index(const std::string& fieldName)
    {
        _field = &_info->field(fieldName);
        if (!_field->is_unique())
        {
            throw SQLError(("field not unique: " + _field->name()).c_str());
        }
    }
    void execute()
    {
        auto& index = IndexManager::instance().create_index(_info->name(), _indexName, _field->name(), _field->type_info());

        auto& records = RecordManager::instance().find_table(_info->name());
        for (size_t i = 0; i != records.size(); i++)
        {
            index.tree()->insert(records[i]->raw_ptr() + _field->offset(), records[i]);
        }
    }
};