#include "stdafx.h"
#include "CatalogManager.h"

std::shared_ptr<Comparer> Comparer::from_type(Type type, size_t length)
{
    if (type == Chars)
    {
        return std::shared_ptr<Comparer>(new CharsComparer(length));
    }
    assert(("accept only char", 0));
    return nullptr;
}

std::shared_ptr<Comparer> Comparer::from_type(Type type)
{
    assert(type != Chars);
    if(type == Int)
    {
        return std::shared_ptr<IntComparer>(new IntComparer());
    }
    else if(type == Float)
    {
        return std::shared_ptr<FloatComparer>(new FloatComparer());
    }
    assert(("accept only int and float", 0));
    return nullptr;
}
