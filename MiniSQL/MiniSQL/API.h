#pragma once
#include "CatalogManager.h"
#include "RecordManager.h"
#include "Utils.h"
#include <string>
#include <memory>
#include <vector>

class InvalidComparisonType : public std::runtime_error
{
public:
    InvalidComparisonType(const char* str)
        : runtime_error(str)
    {
    }
};


class Value
{
public:
    virtual ~Value() {}
    virtual TableInfo::ValueProxy value(TableInfo& info, const BufferBlock& block) = 0;
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
    TableInfo::ValueProxy value(TableInfo& info, const BufferBlock& block) override
    {
        return info[block][_fieldName];
    }
};

class ImmediateValue : public Value
{
private:
    std::unique_ptr<byte, BufferArrayDeleter> _value;
    const TypeInfo* _type;
public:
    ImmediateValue(byte* value, const TypeInfo& type)
        : _value(value)
        , _type(&type)
    { }
    TableInfo::ValueProxy value(TableInfo& info, const BufferBlock& block) override
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
    virtual bool evaluate(TableInfo& info, const BufferBlock& block) const = 0;
    virtual ComparisonType type() const = 0;
};

class EqComparison : public Comparison
{
public:
    EqComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool evaluate(TableInfo& info, const BufferBlock& block) const override
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
    bool evaluate(TableInfo& info, const BufferBlock& block) const override
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
    bool evaluate(TableInfo& info, const BufferBlock& block) const override
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
    bool evaluate(TableInfo& info, const BufferBlock& block) const override
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
    bool evaluate(TableInfo& info, const BufferBlock& block) const override
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
    bool evaluate(TableInfo& info, const BufferBlock& block) const override
    {
        return _lhs->value(info, block) <= _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Le; }
};

class QueryList
{
private:
    std::vector<std::unique_ptr<Comparison>> _comparisonNodes;
    TableInfo* _info;

public:
    explicit QueryList(TableInfo* info)
        : _comparisonNodes{}
        , _info(info)
    {
    }
    void add_query(Comparison::ComparisonType type, const std::string& fieldName, byte* value)
    {
        std::unique_ptr<Comparison> comparison;
        auto lValue = new TokenFieldValue(fieldName);
        auto rValue = new ImmediateValue(value, _info->field(fieldName).type_info());
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
    std::vector<BlockPtr> execute_linearly()
    {
        auto& recMgr = RecordManager::instance();
        auto& recList = recMgr[_info->name()];
        auto listSize = recList.size();

        std::vector<BlockPtr> queryResult;

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
                queryResult.push_back(recList[i]);
            }
        }
        return queryResult;
    }
};

class API
{





};