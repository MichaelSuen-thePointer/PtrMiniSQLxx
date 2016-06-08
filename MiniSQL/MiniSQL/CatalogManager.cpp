#include "stdafx.h"
#include "CatalogManager.h"

const char* const CatalogManager::FileName = "CatalogManagerConfig";

Comparator* Comparator::from_type(Type type, size_t length)
{
    static std::array<std::unique_ptr<Comparator>, 255> comparers;
    assert(type == Chars);
    
    if(comparers[length - 1] == nullptr)
    {
        comparers[length - 1].reset(new CharsComparator(length));
    }
    return comparers[length - 1].get();
}

Comparator* Comparator::from_type(Type type)
{
    static std::unique_ptr<IntComparator> intComparer(new IntComparator());
    static std::unique_ptr<FloatComparator> floatComparer(new FloatComparator());
    assert(type != Chars);
    if(type == Int)
    {
        return intComparer.get();
    }
    if(type == Float)
    {
        return floatComparer.get();
    }
    assert(("accept only int and float", 0));
    return nullptr;
}

CatalogManager::~CatalogManager()
{
    auto& block0 = BufferManager::instance().find_or_alloc(FileName, 0, 0);
    MemoryWriteStream stream(block0.raw_ptr(), BufferBlock::BlockSize);
    stream << static_cast<uint16_t>(_tables.size());
    block0.notify_modification();
    uint16_t i = 1;
    for (auto& info : _tables)
    {
        auto& tableBlock = BufferManager::instance().find_or_alloc(FileName, 0, i);
        MemoryWriteStream mws(tableBlock.raw_ptr(), BufferBlock::BlockSize);
        Serializer<TableInfo>::serialize(mws, info);
        tableBlock.notify_modification();
        i++;
    }
}

CatalogManager::CatalogManager()
{
    if (!BufferManager::instance().has_block(FileName, 0, 0))
    {
        return;
    }
    auto& block0 = BufferManager::instance().find_or_alloc(FileName, 0, 0);
    MemoryReadStream stream(block0.raw_ptr(), BufferBlock::BlockSize);
    uint16_t tableSize;
    stream >> tableSize;
    _tables.reserve(tableSize);

    for (uint16_t i = 0; i != tableSize; i++)
    {
        auto& blocki = BufferManager::instance().find_or_alloc(FileName, 0, i + 1);
        MemoryReadStream mrs(blocki.raw_ptr(), BufferBlock::BlockSize);
        _tables.push_back(Serializer<TableInfo>::deserialize(mrs));
    }
}
