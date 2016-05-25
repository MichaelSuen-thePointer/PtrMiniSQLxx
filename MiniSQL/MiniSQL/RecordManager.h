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
    class TableRecordList;
    class TableRecordInfo
    {
        friend class TableRecordList;
    private:
        size_t _recordSize;
        std::vector<BlockPtr> _recordPtrs;
    public:
        TableRecordInfo(size_t size, const std::vector<BlockPtr>& records)
            : _recordSize(size)
            , _recordPtrs(records)
        {
        }
    };

    const static size_t ptr_count = (BufferBlock::BlockSize - sizeof(size_t)) / sizeof(BlockPtr);

    struct TableRecordListModel
    {
        size_t _count;
        BlockPtr _head[1];
    };

    class TableRecordListIterator
    {
    private:
        TableRecordList* _parent;
        size_t _blockIndex;
        size_t _entryIndex;
    public:

    };

    class TableRecordList
    {
    private:
        TableRecordInfo* _info;
        std::vector<TableRecordListModel*> _bases;
    public:
        explicit TableRecordList(TableRecordInfo& info)
            : _info(&info)
            , _bases()
        {
            _bases.reserve(info._recordPtrs.size());
            for (auto& ptr : info._recordPtrs)
            {
                _bases.push_back(ptr->as<TableRecordListModel>());
            }
        }

        size_t size() const
        {
            size_t count = 0;
            for(auto& model : _bases)
            {
                count += model->_count;
            }
            return count;
        }

        BlockPtr operator[](size_t index) const
        {
            assert(_bases.size());
            size_t i = 0;
            while (index >= _bases[i]->_count)
            {
                i++;
                index -= _bases[i]->_count;
            }
            return _bases[i]->_head[index];
        }
    };

private:
    std::map<std::string, TableRecordInfo> _tableInfos;

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

