#pragma once

#include "BufferManager.h"

class TableNotExist : public std::runtime_error
{
public:
    explicit TableNotExist(const char* msg)
        : runtime_error(msg)
    {
    }
};

class RecordManager
{
public:
    class TableRecordList
    {
        friend class RecordManager;
    private:
        using Record = std::pair<int, int>;
        std::string _fileName;
        std::deque<Record> _records;
        std::set<Record> _freeRecords;
        std::pair<int, int> _currentMax;

        TableRecordList(const std::string& tableName,
                        std::deque<Record>&& records,
                        std::set<Record>&& freeRecords)
            : _fileName(tableName + "_records.txt")
            , _records(std::move(records))
            , _freeRecords(std::move(freeRecords))
        {
        }
    public:
        size_t size() const
        {
            return _records.size();
        }

        BlockPtr operator[](size_t index) const
        {
            auto& pair = _records[index];
            return{ BufferManager::instance().check_file_index(_fileName), 0, pair.first, pair.second };
        }

        void insert(byte* buffer, size_t size)
        {
            std::pair<int, int> entry;
            if(_freeRecords.size())
            {
                entry = *_freeRecords.begin();
                _freeRecords.erase(_freeRecords.begin());
            }
            else
            {
                _currentMax += 1;
                entry = _currentMax;
            }
            auto& block = BufferManager::instance().find_or_alloc(_fileName, 0, entry.first);
            assert((BufferBlock::BlockSize - size) >= entry.second);
            memcpy(block.raw_ptr() + entry.second, buffer, size);
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
    };

private:
    std::map<std::string, TableRecordList> _tableInfos;

    RecordManager()
        : _tableInfos()
    {
        //TODO : initialize it from file
    }
public:
    static RecordManager& instance()
    {
        static RecordManager instance;
        return instance;
    }

    void insert_entry(const std::string& tableName, byte* entry)
    {
        auto tableInfo = _tableInfos.find(tableName);
        if (tableInfo == _tableInfos.end())
        {
            throw TableNotExist(tableName.c_str());
        }
        TableRecordList entries(tableInfo->second);


    }
};

