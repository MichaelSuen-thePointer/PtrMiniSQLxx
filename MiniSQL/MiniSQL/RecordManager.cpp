#include "stdafx.h"
#include "RecordManager.h"
#include "MemoryWriteStream.h"
#include "MemoryReadStream.h"
const char* const RecordManager::FileName = "RecordManagerMeta";

RecordManager::~RecordManager()
{
    auto& block = BufferManager::instance().find_or_alloc(FileName, 0, 0);
    block.lock();
    byte* rawMem = block.as<byte>();
    MemoryWriteStream ostream(rawMem, BufferBlock::BlockSize);

    ostream << static_cast<uint16_t>(_tableInfos.size());
    int tableNo = 1;
    for (auto& tableInfoPair : _tableInfos)
    {
        ostream << tableInfoPair.first;
        ostream << tableInfoPair.second._records.size();
        ostream << tableInfoPair.second._freeRecords.size();
        ostream << tableInfoPair.second._entrySize;
        ostream << tableInfoPair.second._nextPos.first << tableInfoPair.second._nextPos.second;

        const size_t blockCnt = BufferBlock::BlockSize / sizeof(TableRecordList::Record);

        uint16_t recordBlockNeeded = static_cast<uint16_t>(std::ceil(tableInfoPair.second._records.size() / double(blockCnt)));
        for (uint16_t i = 0; i != recordBlockNeeded; i++)
        {
            auto& tableInfoBlock = BufferManager::instance().find_or_alloc(FileName, tableNo, i);
            tableInfoBlock.lock();
            byte* tableRawMem = tableInfoBlock.as<byte>();
            MemoryWriteStream otableStream(tableRawMem, BufferBlock::BlockSize);

            size_t maxRange = std::min(tableInfoPair.second._records.size(), (i + 1) * blockCnt);

            for (size_t iRec = i * blockCnt; iRec < i * blockCnt + maxRange; iRec++)
            {
                otableStream << tableInfoPair.second._records[iRec].first << tableInfoPair.second._records[iRec].second;
            }
            tableInfoBlock.notify_modification();
            tableInfoBlock.unlock();
        }

        uint16_t freeBlockNeeded = static_cast<uint16_t>(std::ceil(tableInfoPair.second._freeRecords.size() / double(blockCnt)));
        for (uint16_t i = 0; i != freeBlockNeeded; i++)
        {
            auto& tableInfoBlock = BufferManager::instance().find_or_alloc(FileName, tableNo, i + recordBlockNeeded);
            tableInfoBlock.lock();
            byte* tableRawMem = tableInfoBlock.as<byte>();
            MemoryWriteStream otableStream(tableRawMem, BufferBlock::BlockSize);

            size_t maxRange = std::min(tableInfoPair.second._freeRecords.size(), (i + 1) * blockCnt);

            auto iterElm = tableInfoPair.second._freeRecords.begin();
            for (size_t iRec = i * blockCnt; iRec < maxRange; iRec++)
            {
                otableStream << iterElm->first << iterElm->second;
                ++iterElm;
            }
            tableInfoBlock.notify_modification();
            tableInfoBlock.unlock();
        }
        tableNo++;
    }
    block.notify_modification();
    block.unlock();
}

RecordManager::RecordManager()
    : _tableInfos()
{
    if (!BufferManager::instance().has_block(FileName, 0, 0))
    {
        return;
    }
    auto& block = BufferManager::instance().find_or_alloc(FileName, 0, 0);

    block.lock();
    byte* rawMem = block.as<byte>();

    MemoryReadStream ostream(rawMem, BufferBlock::BlockSize);

    const uint16_t entryPerBlock = BufferBlock::BlockSize / sizeof(TableRecordList::Record);

    uint16_t tableInfoSize;

    ostream >> tableInfoSize;

    for (uint16_t iTable = 0; iTable != tableInfoSize; iTable++)
    {
        std::string tableName;
        size_t recordCount;
        size_t freeRecordCount;
        uint16_t entrySize;
        TableRecordList::Record nextPos;

        ostream >> tableName >> recordCount >> freeRecordCount >> entrySize >> nextPos.first >> nextPos.second;

        std::deque<TableRecordList::Record> records;

        uint16_t recordBlockCount = static_cast<uint16_t>(std::ceil(recordCount / static_cast<double>(entryPerBlock)));
        uint16_t readBlock = 0;
        for (uint16_t iBlk = 0; iBlk != recordBlockCount; iBlk++)
        {
            auto& tableInfoBlock = BufferManager::instance().find_or_alloc(FileName, iTable + 1, iBlk);
            tableInfoBlock.lock();
            byte* tableRawMem = tableInfoBlock.as<byte>();
            MemoryReadStream otableStream(tableRawMem, BufferBlock::BlockSize);

            uint16_t upperRange = std::min(entryPerBlock, uint16_t(recordCount - readBlock));

            for (uint16_t i = 0; i != upperRange; i++)
            {
                TableRecordList::Record entry;
                otableStream >> entry.first >> entry.second;
                records.push_back(entry);
            }
            readBlock += upperRange;
            tableInfoBlock.unlock();
        }

        std::set<TableRecordList::Record> freeRecords;

        uint16_t freeRecordBlockCount = static_cast<uint16_t>(std::ceil(freeRecordCount / static_cast<double>(entryPerBlock)));
        uint16_t freeBlockRead = 0;
        for (uint16_t iBlk = 0; iBlk != freeRecordBlockCount; iBlk++)
        {
            auto& tableInfoBlock = BufferManager::instance().find_or_alloc(FileName, iTable + 1, iBlk + recordBlockCount);
            tableInfoBlock.lock();
            byte* tableRawMem = tableInfoBlock.as<byte>();
            MemoryReadStream otableStream(tableRawMem, BufferBlock::BlockSize);

            uint16_t upperRange = std::min(entryPerBlock, uint16_t(freeRecordCount - freeBlockRead));

            for (uint16_t i = 0; i != upperRange; i++)
            {
                TableRecordList::Record entry;
                otableStream >> entry.first >> entry.second;
                freeRecords.insert(entry);
            }
            freeBlockRead += upperRange;
            tableInfoBlock.unlock();
        }

        _tableInfos.insert({tableName, TableRecordList{tableName, move(records), move(freeRecords), entrySize, nextPos}});
    }
    block.unlock();
}
