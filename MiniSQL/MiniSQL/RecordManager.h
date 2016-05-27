#pragma once

#include "BufferManager.h"
#include "CatalogManager.h"

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
    private:
        using Record = std::pair<int, int>;
        TableInfo* _info;
        std::deque<Record> _records;
        std::set<Record> _freeRecords;

    private:
        TableRecordList(TableInfo* info, std::deque<Record>&& records,
                                 std::set<Record>&& freeRecords)
            : _info(info)
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

        TableInfo::TupleProxy operator[](size_t index) const
        {
            auto& pair = _records[index];
            BlockPtr ptr(BufferManager::instance().check_file_index(_info->name()), pair.first, pair.second);

            return (*_info)[ptr];
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
        if(tableInfo == _tableInfos.end())
        {
            throw TableNotExist(tableName.c_str());
        }
        TableRecordList entries(tableInfo->second);


    }
};

