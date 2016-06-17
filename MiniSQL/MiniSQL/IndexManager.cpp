#include "stdafx.h"
#include "IndexManager.h"

const char* const IndexManager::FileName = "IndexManagerMeta";

IndexInfo Serializer<IndexInfo>::deserialize(MemoryReadStream& mrs)
{
    std::string fieldName;
    std::string tableName;
    mrs >> fieldName;
    mrs >> tableName;
    BlockPtr ptr = Serializer<BlockPtr>::deserialize(mrs);
    Type type;
    size_t size;
    mrs >> type >> size;
    return IndexInfo{fieldName, TreeCreater::create(type, size, ptr, tableName, false), type, size};
}

void Serializer<IndexInfo>::serialize(MemoryWriteStream& mws, const IndexInfo& value)
{
    mws << value.field_name() << value.tree()->file_name();
    Serializer<BlockPtr>::serialize(mws, value.tree()->root());
    mws << value._type << value._size;
}

size_t Serializer<IndexInfo>::size(const IndexInfo& value)
{
    return value.field_name().size() + 1 + value.tree()->file_name().size() + 1
        + Serializer<BlockPtr>::size(value.tree()->root())
        + sizeof(value._type) + sizeof(value._size);
}
