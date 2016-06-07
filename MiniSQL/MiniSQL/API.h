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
    virtual TableInfo::ValueProxy value(TableInfo& info, const BufferBlock& block) = 0;
};

class TokenFieldValue : public Value
{
private:
    std::string _entryName;
public:
    TableInfo::ValueProxy value(TableInfo& info, const BufferBlock& block) override
    {
        return info[block][_entryName];
    }
};

class ImmediateValue : public Value
{
private:
    std::unique_ptr<byte, BufferArrayDeleter> _value;
    TypeInfo* _type;
public:
    ImmediateValue(byte* value, TypeInfo* type)
        : _value(value)
        , _type(type)
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
    virtual bool compare(TableInfo& info, const BufferBlock& block) const = 0;
    virtual ComparisonType type() const = 0;
};

class EqComparison : public Comparison
{
    EqComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool compare(TableInfo& info, const BufferBlock& block) const override
    {
        return _lhs->value(info, block) == _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Eq; }
};

class NeComparison : public Comparison
{
    NeComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool compare(TableInfo& info, const BufferBlock& block) const override
    {
        return _lhs->value(info, block) != _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Ne; }
};

class GtComparison : public Comparison
{
    GtComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool compare(TableInfo& info, const BufferBlock& block) const override
    {
        return _lhs->value(info, block) > _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Gt; }
};

class LtComparison : public Comparison
{
    LtComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool compare(TableInfo& info, const BufferBlock& block) const override
    {
        return _lhs->value(info, block) < _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Lt; }
};

class GeComparison : public Comparison
{
    GeComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool compare(TableInfo& info, const BufferBlock& block) const override
    {
        return _lhs->value(info, block) >= _rhs->value(info, block);
    }
    ComparisonType type() const override { return ComparisonType::Gt; }
};

class LeComparison : public Comparison
{
    LeComparison(Value* lhs, Value* rhs)
        : Comparison(lhs, rhs)
    {
    }
    bool compare(TableInfo& info, const BufferBlock& block) const override
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
        switch (type)
        {
        case Comparison::ComparisonType::Eq: 
            comparison.reset(new EqComparison(new TokenFieldValue(fieldName), new ImmediateValue(value, _info->field(fieldName).type_info())));
            break;
        case Comparison::ComparisonType::Ne: break;
        case Comparison::ComparisonType::Lt: break;
        case Comparison::ComparisonType::Gt: break;
        case Comparison::ComparisonType::Le: break;
        case Comparison::ComparisonType::Ge: break;
        default: break;
        }
    }

};

class API
{





};