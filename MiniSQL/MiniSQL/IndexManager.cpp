#include "stdafx.h"
#include "IndexManager.h"

const char* const IndexManager::FileName = "IndexManagerMeta";

IndexInfo Serializer<IndexInfo>::deserialize(MemoryReadStream& mrs)
{

}

void Serializer<IndexInfo>::serialize(MemoryWriteStream& mws, const IndexInfo& value)
{
}
