#pragma once

#include "BPlusTree.h"
#include "MemoryReadStream.h"
#include "TypeInfo.h"

template<size_t l, size_t r>
class CharTreeCreater
{
public:
    static BPlusTreeBase* create(size_t size, const BlockPtr& ptr, const std::string& name, bool isNew)
    {
        if (size > (l + r) / 2)
        {
            return CharTreeCreater<(l + r) / 2, r>::create(size, ptr, name, isNew);
        }
        else if (size < (l + r) / 2)
        {
            return CharTreeCreater<l, (l + r) / 2>::create(size, ptr, name, isNew);
        }
        else
        {
            return new BPlusTree<std::array<char, (l + r) / 2>>(ptr, name, isNew);
        }
    }
};

template<>
class CharTreeCreater<1, 1>
{
public:

    static BPlusTreeBase* create(size_t size, const BlockPtr& ptr, const std::string& name, bool isNew)
    {
        if (size != 1)
        {
            throw InvalidType("invalid type");
        }
        else
        {
            return new BPlusTree<std::array<char, 1>>(ptr, name, isNew);
        }
    }
};

template<>
class CharTreeCreater<255, 255>
{
public:
    static BPlusTreeBase* create(size_t size, const BlockPtr& ptr, const std::string& name, bool isNew)
    {
        if (size != 1)
        {
            throw InvalidType("invalid type");
        }
        else
        {
            return new BPlusTree<std::array<char, 255>>(ptr, name, isNew);
        }
    }
};


class TreeCreater
{
public:
    static BPlusTreeBase* create(Type type, size_t size, const BlockPtr& ptr, const std::string& name, bool isNew)
    {
        if (type == Int)
        {
            return new BPlusTree<int>(ptr, name, isNew);
        }
        else if (type == Float)
        {
            return new BPlusTree<float>(ptr, name, isNew);
        }
        else
        {
            return CharTreeCreater<1, 255>::create(size, ptr, name, isNew);
        }
    }
};

class IndexInfo
{
    friend class Serializer<IndexInfo>;
private:
    std::string _indexName;
    std::string _fieldName;
    std::unique_ptr<BPlusTreeBase> _tree;
    Type _type;
    size_t _size;
public:
    IndexInfo(const std::string& indexName, const std::string& cs, BPlusTreeBase* tree, Type type, size_t size)
        : _indexName(indexName)
        , _fieldName(cs)
        , _tree(tree)
        , _type(type)
        , _size(size)
    {
    }

    IndexInfo(IndexInfo&& rhs)
        : _indexName(std::move(rhs._indexName))
        , _fieldName(std::move(rhs._fieldName))
        , _tree(rhs._tree.release())
        , _type(rhs._type)
        , _size(rhs._size)
    {
    }
    const std::string& index_name() const { return _indexName; }
    const std::string& field_name() const { return _fieldName; }
    BPlusTreeBase* tree() const { return _tree.get(); }
};


template<>
class Serializer<IndexInfo>
{
public:
    static IndexInfo deserialize(MemoryReadStream& mrs);
    static void serialize(MemoryWriteStream& mws, const IndexInfo& value);
    static size_t size(const IndexInfo& value);
};

class IndexManager : Uncopyable
{
public:
    //获取管理器实例
    static IndexManager& instance()
    {
        static IndexManager instance;
        return instance;
    }

private:
    std::multimap<std::string, IndexInfo> _tables;

    const static char* const FileName;
public:
    //创建索引树
    IndexInfo& create_index(const std::string& tableName, const std::string& indexName,
                            const std::string& fieldName, const TypeInfo& type)
    {
        if (has_index(tableName, fieldName))
        {
            throw AlreadyHaveIndex((tableName + " " + fieldName).c_str());
        }
        auto result = _tables.insert({tableName,
            IndexInfo{indexName, fieldName,
                TreeCreater::create(type.type(), type.size(),
                    BufferManager::instance().alloc_block(tableName + "_" + fieldName + "_bplustree").ptr(),
                                     tableName + "_" + fieldName + "_bplustree", true), type.type(), type.size()}});

        return result->second;
    }
    //丢弃特定名称的索引
    void drop_index_with_name(const std::string& indexName)
    {
        auto iter = std::find_if(_tables.begin(), _tables.end(), [&indexName](const auto& idxnfo) {
            return idxnfo.second.index_name() == indexName;
        });
        if (iter != _tables.end())
        {
            iter->second.tree()->drop_tree();
            _tables.erase(iter);
        }
        else
        {
            throw NoIndex((indexName).c_str());
        }
    }
    //丢弃所有索引
    void drop_index(const std::string& tableName)
    {
        auto iterPair = _tables.equal_range(tableName);
        if (iterPair.first == iterPair.second)
        {
            throw NoIndex(tableName.c_str());
        }
        for (auto iter = iterPair.first; iter != iterPair.second; ++iter)
        {
            iter->second.tree()->drop_tree();
            _tables.erase(iter);
            return;
        }
        throw NoIndex(tableName.c_str());
    }
    //检查特定字段上是否有索引
    bool has_index(const std::string& tableName, const std::string& fieldName)
    {
        auto iterPair = _tables.equal_range(tableName);

        for (auto iter = iterPair.first; iter != iterPair.second; ++iter)
        {
            if (iter->second.field_name() == fieldName)
            {
                return true;
            }
        }
        return false;
    }
    //检查表是否有索引
    bool has_index(const std::string& tableName)
    {
        auto iterPair = _tables.equal_range(tableName);

        if (iterPair.first != iterPair.second)
        {
            return true;
        }
        return false;
    }
    //返回所有有索引的字段名
    std::vector<std::string> indexed_fields(const std::string& tableName) const
    {
        std::vector<std::string> results;

        auto iterPair = _tables.equal_range(tableName);

        for (auto iter = iterPair.first; iter != iterPair.second; ++iter)
        {
            results.push_back(iter->second.field_name());
        }
        return results;
    }
    //检查值在索引中是否已经存在
    bool check_index_unique(const std::string& tableName, const std::string& fieldName, byte* key)
    {
        auto& info = check_index(tableName, fieldName);
        return info.tree()->find_eq(key) == nullptr;
    }
    //获取索引信息
    const IndexInfo& check_index(const std::string& tableName, const std::string& fieldName)
    {
        auto iterPair = _tables.equal_range(tableName);

        for (auto iter = iterPair.first; iter != iterPair.second; ++iter)
        {
            if (iter->second.field_name() == fieldName)
            {
                return iter->second;
            }
        }
        throw NoIndex(tableName.c_str());
    }
    //插入索引和对应的值
    void insert(const std::string& tableName, const std::string& fieldName, byte* key, const BlockPtr& ptr)
    {
        auto& indexInfo = check_index(tableName, fieldName);
        indexInfo.tree()->insert(key, ptr);
    }
    //移除key对应的项
    void remove(const std::string& tableName, const std::string& fieldName, byte* key)
    {
        auto& indexInfo = check_index(tableName, fieldName);
        indexInfo.tree()->remove(key);
    }
    //进行搜索
    std::vector<BlockPtr> search(const std::string& tableName, const std::string& fieldName, byte* lower, byte* upper)
    {
        auto& indexInfo = check_index(tableName, fieldName);

        if (lower == nullptr && upper == nullptr)
        {
            return indexInfo.tree()->find_all();
        }
        else if (lower == nullptr)
        {
            return indexInfo.tree()->find_le(upper);
        }
        if (upper == nullptr)
        {
            return indexInfo.tree()->find_ge(lower);
        }
        return indexInfo.tree()->find_range(lower, upper);
    }

    const std::multimap<std::string, IndexInfo>& tables() const { return _tables; }

    //构造和析构
    IndexManager()
        : _tables()
    {
        if (!BufferManager::instance().has_block(FileName, 0, 0))
        {
            return;
        }
        auto& block = BufferManager::instance().find_or_alloc(FileName, 0, 0);

        block.lock();
        byte* rawMem = block.as<byte>();

        MemoryReadStream ostream(rawMem, BufferBlock::BlockSize);

        uint16_t iBlock = 0;
        for (;;)
        {
            uint16_t totalEntry;
            ostream >> totalEntry;
            if (totalEntry == (uint16_t)-1)
            {
                break;
            }
            auto& entryBlock = BufferManager::instance().find_or_alloc(FileName, 0, iBlock + 1);
            MemoryReadStream blockStream(entryBlock.as<byte>(), BufferBlock::BlockSize);
            for (uint16_t iEntry = 0; iEntry != totalEntry; iEntry++)
            {
                std::string tableName;
                blockStream >> tableName;
                _tables.insert({tableName, Serializer<IndexInfo>::deserialize(blockStream)});
            }
            iBlock++;
        }
        block.unlock();
    }

    ~IndexManager()
    {
        auto& block = BufferManager::instance().find_or_alloc(FileName, 0, 0);

        block.lock();
        byte* rawMem = block.as<byte>();

        MemoryWriteStream ostream(rawMem, BufferBlock::BlockSize);

        uint16_t iBlock = 0;

        auto iter = _tables.begin();

        for (;;)
        {
            uint16_t totalEntry = 0;
            if (iter == _tables.end())
            {
                break;
            }
            auto& entryBlock = BufferManager::instance().find_or_alloc(FileName, 0, iBlock + 1);
            MemoryWriteStream blockStream(entryBlock.as<byte>(), BufferBlock::BlockSize);
            while (blockStream.remain() >= sizeof(iter->first) + Serializer<IndexInfo>::size(iter->second))
            {
                blockStream << iter->first;
                Serializer<IndexInfo>::serialize(blockStream, iter->second);

                totalEntry++;
                ++iter;
                if (iter == _tables.end())
                {
                    break;
                }
            }
            entryBlock.notify_modification();
            ostream << totalEntry;
            iBlock++;
        }

        ostream << (uint16_t)-1;

        block.notify_modification();
        block.unlock();
    }
};
