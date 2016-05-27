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
        using record_list = decltype(_records);
        using iterator = typename record_list::iterator;
        using const_iterator = typename record_list::const_iterator;
        using reverse_iterator = typename record_list::reverse_iterator;
        using const_reverse_iterator = typename record_list::const_reverse_iterator;
        size_t size() const
        {
            return _records.size();
        }

        iterator begin() { return _records.begin(); }
        const_iterator begin() const { return _records.begin(); }
        iterator end() { return _records.end(); }
        const_iterator end() const { return _records.end(); }
        reverse_iterator rbegin() { return _records.rbegin(); }
        const_reverse_iterator rbegin() const { return _records.rbegin(); }
        reverse_iterator rend() { return _records.rend(); }
        const_reverse_iterator rend() const { return _records.rend(); }

        BlockPtr operator[](size_t index) const
        {
            auto& pair = _records[index];
            return{ BufferManager::instance().check_file_index(_fileName), pair.first, pair.second };
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

