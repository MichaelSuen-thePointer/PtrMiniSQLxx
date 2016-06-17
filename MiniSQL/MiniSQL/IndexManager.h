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
#ifdef _DEBUG
            throw std::exception("disabled in debug mode");
#else
            return CharTreeCreater<1, 255>::create(size, ptr, name, isNew);
#endif
        }
    }
};

class IndexInfo
{
    friend class Serializer<IndexInfo>;
private:
    std::string _fieldName;
    std::unique_ptr<BPlusTreeBase> _tree;
    Type _type;
    size_t _size;
public:
    IndexInfo(const std::string& cs, BPlusTreeBase* tree, Type type, size_t size)
        : _fieldName(cs)
        , _tree(tree)
        , _type(type)
        , _size(size)
    {
    }

    IndexInfo(IndexInfo&& rhs)
        : _fieldName(std::move(rhs._fieldName))
        , _tree(rhs._tree.release())
        , _type(rhs._type)
        , _size(rhs._size)
    {
    }

    const std::string& field_name() const { return _fieldName; }
    BPlusTreeBase* tree() const { return _tree.get(); }
};


template<>
class Serializer<IndexInfo>
{
public:
    static IndexInfo deserialize(MemoryReadStream& mrs);
    static void serialize(MemoryWriteStream& mws, const IndexInfo& value);
};

class IndexManager : Uncopyable
{
public:
    static IndexManager& instance()
    {
        static IndexManager instance;
        return instance;
    }

private:
    std::map<std::string, IndexInfo> _tables;

    const static char* const FileName;
public:

    void create_index(const std::string& tableName, const std::string& fieldName,
                      const TypeInfo& type)
    {
        auto iterPrev = _tables.find(tableName);
        if (iterPrev != _tables.end())
        {
            iterPrev->second.tree()->drop_tree();
        }
        _tables.insert({tableName,
            IndexInfo{fieldName,
                TreeCreater::create(type.type(), type.size(),
                    BufferManager::instance().alloc_block(tableName + "_record").ptr(),
                    tableName + "_record", true), type.type(), type.size()}});
    }

    void drop_index(const std::string& tableName)
    {
        auto iterPrev = _tables.find(tableName);
        if (iterPrev != _tables.end())
        {
            iterPrev->second.tree()->drop_tree();
        }
        else
        {
            throw NoIndex(tableName.c_str());
        }
    }

    bool has_index(const std::string& tableName)
    {
        return _tables.find(tableName) != _tables.end();
    }

    const IndexInfo& check_index(const std::string& tableName)
    {
        auto iter = _tables.find(tableName);
        if (iter != _tables.end())
        {
            return iter->second;
        }
        throw NoIndex(tableName.c_str());
    }

    void insert(const std::string& tableName, byte* key, const BlockPtr& ptr)
    {
        auto iter = _tables.find(tableName);
        if (iter == _tables.end())
        {
            throw NoIndex(tableName.c_str());
        }
        iter->second.tree()->insert(key, ptr);
    }

    void remove(const std::string& tableName, byte* key)
    {
        auto& indexInfo = check_index(tableName);
        indexInfo.tree()->remove(key);
    }

    std::vector<BlockPtr> search(const std::string& tableName, byte* lower, byte* upper)
    {
        assert(lower != nullptr || upper != nullptr);
        auto& indexInfo = check_index(tableName);

        if (lower == nullptr)
        {
            return indexInfo.tree()->find_le(upper);
        }
        if (upper == nullptr)
        {
            return indexInfo.tree()->find_ge(lower);
        }
        return indexInfo.tree()->find_range(lower, upper);
    }

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
            if (totalEntry == -1)
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

        ostream << (uint16_t)_tables.size();

        uint16_t iBlock = 0;
        for (const auto& pair : _tables)
        {
            uint16_t totalEntry = 0;

            auto& entryBlock = BufferManager::instance().find_or_alloc(FileName, 0, iBlock + 1);
            MemoryWriteStream blockStream(entryBlock.as<byte>(), BufferBlock::BlockSize);
            while (blockStream.remain() > pair.first.size() + sizeof(BlockPtr) + sizeof(Type) + sizeof(size_t))
            {
                blockStream << pair.first;
                Serializer<IndexInfo>::serialize(blockStream, pair.second);

                totalEntry++;
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
