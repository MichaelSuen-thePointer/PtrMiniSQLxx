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
        using Record = std::pair<uint32_t, uint16_t>;
        std::string _fileName;
        std::deque<Record> _records;
        std::set<Record> _freeRecords;
        uint16_t _entrySize;
        Record _nextPos;

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

        //构造函数
        TableRecordList(TableRecordList&& other)
            : _fileName(std::move(other._fileName))
            , _records(std::move(other._records))
            , _freeRecords(std::move(other._freeRecords))
            , _entrySize(other._entrySize)
            , _nextPos(other._nextPos)
        {
        }
        //移动构造
        TableRecordList& operator=(TableRecordList&& other)
        {
            _fileName = std::move(other._fileName);
            _records = std::move(other._records);
            _freeRecords = std::move(other._freeRecords);
            _entrySize = other._entrySize;
            _nextPos = other._nextPos;
            return *this;
        }
        //获取record数
        size_t size() const
        {
            return _records.size();
        }
        //获取条目指针
        BlockPtr operator[](size_t index) const
        {
            auto& pair = _records[index];
            assert((int)pair.second * (int)_entrySize <= std::numeric_limits<uint16_t>::max());
            return{BufferManager::instance().check_file_index(_fileName), 0, pair.first, static_cast<uint16_t>(pair.second * _entrySize)};
        }
        //插入条目
        BlockPtr insert(byte* buffer)
        {
            Record entry;
            if (_freeRecords.size())
            {
                entry = *_freeRecords.begin();
                _freeRecords.erase(_freeRecords.begin());
            }
            else
            {
                entry = _nextPos;
                _nextPos += 1;
                if (_nextPos.second * _entrySize >= BufferBlock::BlockSize)
                {
                    if (_nextPos.first == std::numeric_limits<uint32_t>::max())
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

            return{BufferManager::instance().check_file_index(_fileName), 0, entry.first, static_cast<uint16_t>(entry.second * _entrySize)};
        }
        //删除条目
        void erase(size_t i)
        {
            assert(i >= 0 && i < _records.size());

            auto result = _freeRecords.insert(_records[i]);
            assert(result.second);

            _records.erase(_records.begin() + i);
        }
        //清空条目
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
    //获取管理器实例
    static RecordManager& instance()
    {
        static RecordManager instance;
        return instance;
    }
    //删除记录
    void drop_record(const std::string& tableName)
    {
        _tableInfos.erase(tableName);
    }
    //查找表
    TableRecordList& find_table(const std::string& tableName)
    {
        auto tableInfo = _tableInfos.find(tableName);
        if (tableInfo == _tableInfos.end())
        {
            throw TableNotExist(tableName.c_str());
        }
        return tableInfo->second;
    }
    //插入条目并返回文件指针
    BlockPtr insert_entry(const std::string& tableName, byte* entry)
    {
        auto& table = find_table(tableName);
        return table.insert(entry);
    }
    //移除条目
    void remove_entry(const std::string& tableName, size_t i)
    {
        auto& table = find_table(tableName);
        table.erase(i);
    }
    //获取表信息
    TableRecordList& operator[](const std::string& tableName)
    {
        return find_table(tableName);
    }
    //新建表
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
    //移除表
    void remove_table(const std::string& tableName)
    {
        if (!table_exists(tableName))
        {
            throw TableNotExist(tableName.c_str());
        }
        _tableInfos.erase(tableName);
    }
};

