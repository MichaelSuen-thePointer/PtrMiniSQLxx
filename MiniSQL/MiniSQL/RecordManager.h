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

        //���캯��
        TableRecordList(TableRecordList&& other)
            : _fileName(std::move(other._fileName))
            , _records(std::move(other._records))
            , _freeRecords(std::move(other._freeRecords))
            , _entrySize(other._entrySize)
            , _nextPos(other._nextPos)
        {
        }
        //�ƶ�����
        TableRecordList& operator=(TableRecordList&& other)
        {
            _fileName = std::move(other._fileName);
            _records = std::move(other._records);
            _freeRecords = std::move(other._freeRecords);
            _entrySize = other._entrySize;
            _nextPos = other._nextPos;
            return *this;
        }
        //��ȡrecord��
        size_t size() const
        {
            return _records.size();
        }
        //��ȡ��Ŀָ��
        BlockPtr operator[](size_t index) const
        {
            auto& pair = _records[index];
            assert((int)pair.second * (int)_entrySize <= std::numeric_limits<uint16_t>::max());
            return{BufferManager::instance().check_file_index(_fileName), 0, pair.first, static_cast<uint16_t>(pair.second * _entrySize)};
        }
        //������Ŀ
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
        //ɾ����Ŀ
        void erase(size_t i)
        {
            assert(i >= 0 && i < _records.size());

            auto result = _freeRecords.insert(_records[i]);
            assert(result.second);

            _records.erase(_records.begin() + i);
        }
        //�����Ŀ
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
    //��ȡ������ʵ��
    static RecordManager& instance()
    {
        static RecordManager instance;
        return instance;
    }
    //ɾ����¼
    void drop_record(const std::string& tableName)
    {
        _tableInfos.erase(tableName);
    }
    //���ұ�
    TableRecordList& find_table(const std::string& tableName)
    {
        auto tableInfo = _tableInfos.find(tableName);
        if (tableInfo == _tableInfos.end())
        {
            throw TableNotExist(tableName.c_str());
        }
        return tableInfo->second;
    }
    //������Ŀ�������ļ�ָ��
    BlockPtr insert_entry(const std::string& tableName, byte* entry)
    {
        auto& table = find_table(tableName);
        return table.insert(entry);
    }
    //�Ƴ���Ŀ
    void remove_entry(const std::string& tableName, size_t i)
    {
        auto& table = find_table(tableName);
        table.erase(i);
    }
    //��ȡ����Ϣ
    TableRecordList& operator[](const std::string& tableName)
    {
        return find_table(tableName);
    }
    //�½���
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
    //�Ƴ���
    void remove_table(const std::string& tableName)
    {
        if (!table_exists(tableName))
        {
            throw TableNotExist(tableName.c_str());
        }
        _tableInfos.erase(tableName);
    }
};

