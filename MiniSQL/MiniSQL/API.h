#pragma once
#include "CatalogManager.h"
#include "RecordManager.h"
#include "Utils.h"
#include <string>
#include <memory>

class Value
{
public:
    virtual ~Value() {}
    virtual TableInfo::ValueProxy value(TableInfo& info, const BufferBlock& block) = 0;
};

class TableEntryValue : public Value
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
    virtual ~Comparison() {}
    Comparison(Value* lhs, Value* rhs)
        : _lhs(lhs)
        , _rhs(rhs)
    {
    }
    virtual bool compare(TableInfo& info, const BufferBlock& block) const = 0;
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
};

class QueryExpr
{
private:

    enum class ExprConcatType
    {
        None, And, Or
    };
    struct QueryExprNode
    {
        ExprConcatType operation;
        std::unique_ptr<QueryExprNode> lhs;
        std::unique_ptr<Comparison> rhs;
    };
};

class API
{





};