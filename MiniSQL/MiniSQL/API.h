#pragma once
#include "CatalogManager.h"
#include "RecordManager.h"
#include "Utils.h"
#include <string>

class Value
{
public:
    virtual ~Value() {}
};

class TableEntryValue : public Value
{
private:
    std::string _entryName;
public:

};

class ImmediateValue : public Value
{
private:
    byte* _value;
    
};

class Condition
{
    
};

class API
{



};