#pragma once

#include "BufferManager.h"
#include "MemoryWriteStream.h"
#include "MemoryReadStream.h"
#include <map>
#include <deque>
#include <set>
#include <cassert>
class TableNotExist : public std::runtime_error
{
public:
    explicit TableNotExist(const char* msg)
        : runtime_error(msg)
    {
    }
};

class TableExist : public std::runtime_error
{
public:
    explicit TableExist(const char* msg)
        : runtime_error(msg)
    {
    }
};

class RecordManager
{
public:
    class TableRecordList : Uncopyable
    {
        friend class RecordManager;
        friend class std::map<std::string, TableRecordList>;
    private:
        using Record = std::pair<int, int>;
        std::string _fileName;
        std::deque<Record> _records;
        std::set<Record> _freeRecords;
        size_t _entrySize;
        Record _nextPos;

        TableRecordList(const std::string& tableName,
                        std::deque<Record>&& records,
                        std::set<Record>&& freeRecords,
                        size_t entrySize,
                        Record nextPos)
            : _fileName(tableName + "_records")
            , _records(std::move(records))
            , _freeRecords(std::move(freeRecords))
            , _entrySize(entrySize)
            , _nextPos(nextPos)
        {
        }
    public:

        TableRecordList(TableRecordList&& other)
            : _fileName(std::move(other._fileName))
            , _records(std::move(other._records))
            , _freeRecords(std::move(other._freeRecords))
            , _entrySize(other._entrySize)
            , _nextPos(other._nextPos)
        {
        }

        TableRecordList& operator=(TableRecordList&& other)
        {
            _fileName = std::move(other._fileName);
            _records = std::move(other._records);
            _freeRecords = std::move(other._freeRecords);
            _entrySize = other._entrySize;
            _nextPos = other._nextPos;
            return *this;
        }

        size_t size() const
        {
            return _records.size();
        }

        BlockPtr operator[](size_t index) const
        {
            auto& pair = _records[index];
            return{BufferManager::instance().check_file_index(_fileName), 0, pair.first, static_cast<int>(pair.second * _entrySize)};
        }

        void insert(byte* buffer)
        {
            std::pair<int, int> entry;
            if (_freeRecords.size())
            {
                entry = *_freeRecords.begin();
                _freeRecords.erase(_freeRecords.begin());
            }
            else
            {
                entry = _nextPos;
                _nextPos += 1;
            }
            auto& block = BufferManager::instance().find_or_alloc(_fileName, 0, entry.first);
            assert(BufferBlock::BlockSize - _entrySize >= static_cast<size_t>(entry.second));
            memcpy(block.raw_ptr() + entry.second * _entrySize, buffer, _entrySize);
            block.notify_modification();

            _records.push_back(entry);
        }

        void erase(size_t i)
        {
            assert(i >= 0 && i < _records.size());

            auto result = _freeRecords.insert(_records[i]);
            assert(result.second);

            _records.erase(_records.begin() + i);
        }

        void clear()
        {
            _records.clear();
            _freeRecords.clear();
        }
    };

    const static char* const FileName; /*= "RecordManagerConfig";*/

    ~RecordManager()
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

private:
    std::map<std::string, TableRecordList> _tableInfos;

    RecordManager()
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

    TableRecordList& find_table(const std::string& tableName)
    {
        auto tableInfo = _tableInfos.find(tableName);
        if (tableInfo == _tableInfos.end())
        {
            throw TableNotExist(tableName.c_str());
        }
        return tableInfo->second;
    }

    bool table_exists(const std::string& tableName)
    {
        return _tableInfos.find(tableName) != _tableInfos.end();
    }

public:
    static RecordManager& instance()
    {
        static RecordManager instance;
        return instance;
    }

    void insert_entry(const std::string& tableName, byte* entry)
    {
        auto& table = find_table(tableName);
        table.insert(entry);
    }

    void remove_entry(const std::string& tableName, size_t i)
    {
        auto& table = find_table(tableName);
        table.erase(i);
    }

    TableRecordList& operator[](const std::string& tableName)
    {
        return find_table(tableName);
    }

    void create_table(const std::string& tableName, size_t entrySize)
    {
        if (table_exists(tableName))
        {
            throw TableExist(tableName.c_str());
        }
        _tableInfos.insert({tableName,TableRecordList{tableName, std::deque<TableRecordList::Record>{}, std::set<TableRecordList::Record>{}, entrySize, {0,0}}});
    }

    void delete_table(const std::string& tableName)
    {
        auto& table = find_table(tableName);
        table.clear();
    }
};

