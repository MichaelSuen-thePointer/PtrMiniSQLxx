#include "stdafx.h"
#include "RecordManager.h"

const char* const RecordManager::FileName = "RecordManagerConfig";

RecordManager::~RecordManager()
{
    auto& block = BufferManager::instance().find_or_alloc(FileName, 0, 0);

    block.lock();
    byte* rawMem = block.as<byte>();

    MemoryWriteStream ostream(rawMem, BufferBlock::BlockSize);

    ostream << _tableInfos.size();

    for (auto& tableInfoPair : _tableInfos)
    {
        ostream << tableInfoPair.first;
    }
    block.notify_modification();
    block.unlock();

    int tableNo = 1;
    for (auto& tableInfoPair : _tableInfos)
    {
        auto& tableInfoBlock = BufferManager::instance().find_or_alloc(FileName, 0, tableNo);
        tableInfoBlock.lock();
        byte* tableRawMem = tableInfoBlock.as<byte>();
        MemoryWriteStream otableStream(tableRawMem, BufferBlock::BlockSize);

        otableStream << tableInfoPair.second._records.size();
        for (auto& entry : tableInfoPair.second._records)
        {
            otableStream << entry.first << entry.second;
        }

        otableStream << tableInfoPair.second._freeRecords.size();
        for (auto& entry : tableInfoPair.second._freeRecords)
        {
            otableStream << entry.first << entry.second;
        }

        otableStream << tableInfoPair.second._entrySize;

        otableStream << tableInfoPair.second._nextPos.first << tableInfoPair.second._nextPos.second;

        tableInfoBlock.notify_modification();
        tableInfoBlock.unlock();
        tableNo++;
    }
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

    size_t tableInfoSize;

    ostream >> tableInfoSize;

    for (size_t i = 0; i != tableInfoSize; i++)
    {
        std::string tableName;
        ostream >> tableName;


        auto& tableInfoBlock = BufferManager::instance().find_or_alloc(FileName, 0, i + 1);
        tableInfoBlock.lock();
        byte* tableRawMem = tableInfoBlock.as<byte>();
        MemoryReadStream otableStream(tableRawMem, BufferBlock::BlockSize);

        std::deque<TableRecordList::Record> records;
        size_t recordSize;
        otableStream >> recordSize;

        for (size_t i = 0; i != recordSize; i++)
        {
            std::pair<int, int> entry;
            otableStream >> entry.first >> entry.second;
            records.push_back(entry);
        }

        std::set<TableRecordList::Record> freeRecords;
        size_t freeRecordSize;

        otableStream >> freeRecordSize;
        for (size_t i = 0; i != freeRecordSize; i++)
        {
            std::pair<int, int> entry;
            otableStream >> entry.first >> entry.second;
            freeRecords.insert(entry);
        }

        size_t entrySize;
        otableStream >> entrySize;

        TableRecordList::Record tableNextPos;
        otableStream >> tableNextPos.first >> tableNextPos.second;

        tableInfoBlock.unlock();

        _tableInfos.insert({tableName, TableRecordList{tableName, move(records), move(freeRecords), entrySize, tableNextPos}});
    }
    block.unlock();
}
