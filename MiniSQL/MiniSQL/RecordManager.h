#pragma once

#include "BufferManager.h"

class RecordManager
{
public:
    class TableRecordList : Uncopyable
    {
        friend class RecordManager;
        friend class std::map<std::string, TableRecordList>;
    private:
        using Record = std::pair<uint16_t, uint16_t>;
        std::string _fileName;
        std::deque<Record> _records;
        std::set<Record> _freeRecords;
        uint16_t _entrySize;
        Record _nextPos;

        const static size_t MaxRecordCount = std::numeric_limits<uint16_t>::max() / sizeof(Record) * (size_t)BufferBlock::BlockSize;

        TableRecordList(const std::string& tableName,
            std::deque<Record>&& records,
            std::set<Record>&& freeRecords,
            uint16_t entrySize,
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
            assert((int)pair.second * (int)_entrySize <= std::numeric_limits<uint16_t>::max());
            return{BufferManager::instance().check_file_index(_fileName), 0, pair.first, static_cast<uint16_t>(pair.second * _entrySize)};
        }

        void insert(byte* buffer)
        {
            Record entry;
            if (_freeRecords.size())
            {
                entry = *_freeRecords.begin();
                _freeRecords.erase(_freeRecords.begin());
            }
            else
            {
                if (_records.size() + _freeRecords.size() >= MaxRecordCount)
                {
                    throw InsuffcientSpace("not enough space for new record");
                }
                entry = _nextPos;
                _nextPos += 1;
                if (_nextPos.second * _entrySize >= BufferBlock::BlockSize)
                {
                    if (_nextPos.first == std::numeric_limits<uint16_t>::max())
                    {
                        throw InsuffcientSpace("not enough space for new record");
                    }
                    _nextPos.first++;
                    _nextPos.second = 0;
                }
            }
            auto& block = BufferManager::instance().find_or_alloc(_fileName, 0, entry.first);
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
            _freeRecords.insert(_records.begin(), _records.end());
            _records.clear();
        }
    };

    const static char* const FileName; /*= "RecordManagerMeta";*/

    ~RecordManager();

private:
    std::map<std::string, TableRecordList> _tableInfos;

    RecordManager();

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

    TableRecordList& find_table(const std::string& tableName)
    {
        auto tableInfo = _tableInfos.find(tableName);
        if (tableInfo == _tableInfos.end())
        {
            throw TableNotExist(tableName.c_str());
        }
        return tableInfo->second;
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

    TableRecordList& create_table(const std::string& tableName, uint16_t entrySize)
    {
        if (_tableInfos.size() > std::numeric_limits<uint16_t>::max())
        {
            throw InsuffcientSpace("allows at most 65536 tables");
        }
        if (table_exists(tableName))
        {
            throw TableExist(tableName.c_str());
        }
        auto place = _tableInfos.insert({tableName,TableRecordList{tableName, {}, {}, entrySize, {0,0}}});
        return place.first->second;
    }

    void delete_table(const std::string& tableName)
    {
        if (!table_exists(tableName))
        {
            throw TableNotExist(tableName.c_str());
        }
        _tableInfos.erase(tableName);
    }
};

