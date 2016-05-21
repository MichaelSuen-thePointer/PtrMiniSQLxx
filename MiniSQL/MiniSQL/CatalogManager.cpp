#include "stdafx.h"
#include "CatalogManager.h"

std::shared_ptr<Comparer> Comparer::from_type(Type type, size_t length)
{
    if (type == Chars)
    {
        return std::make_shared<CharsComparer>(length);
    }
    assert(("accept only char", 0));
}

std::shared_ptr<Comparer> Comparer::from_type(Type type)
{
    assert(type != Chars);
    if(type == Int)
    {
        return std::make_shared<IntComparer>();
    }
    else if(type == Float)
    {
        return std::make_shared<FloatComparer>();
    }
    assert(("accept only int and float", 0));
    return nullptr;
}
