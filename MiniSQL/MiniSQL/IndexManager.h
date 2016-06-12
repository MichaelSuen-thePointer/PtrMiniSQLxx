#pragma once

#include "BPlusTree.h"
#include "MemoryReadStream.h"

enum Type
{
    Int,
    Float,
    Chars
};


template<size_t l, size_t r>
class CharTreeCreater
{
public:
    static std::unique_ptr<BPlusTreeBase> create(size_t size, const BlockPtr& ptr, const std::string& name)
    {
        if (size > (l + r) / 2)
        {
            return CharTreeCreater<(l + r) / 2, r>::create(size, ptr, name);
        }
        else if (size < (l + r) / 2)
        {
            return CharTreeCreater<l, (l + r) / 2>::create(size, ptr, name);
        }
        else
        {
            return std::make_unique<BPlusTree<std::array<char, (l + r) / 2>>>(ptr, name);
        }
    }
};

template<>
class CharTreeCreater<1, 1>
{
public:

    static std::unique_ptr<BPlusTreeBase> create(size_t size, const BlockPtr& ptr, const std::string& name)
    {
        if (size != 1)
        {
            throw InvalidType("invalid type");
        }
        else
        {
            return std::make_unique<BPlusTree<std::array<char, 1>>>(ptr, name);
        }
    }
};

template<>
class CharTreeCreater<255, 255>
{
public:
    static std::unique_ptr<BPlusTreeBase> create(size_t size, const BlockPtr& ptr, const std::string& name)
    {
        if (size != 1)
        {
            throw InvalidType("invalid type");
        }
        else
        {
            return std::make_unique<BPlusTree<std::array<char, 255>>>(ptr, name);
        }
    }
};


class TreeCreater
{
public:
    static std::unique_ptr<BPlusTreeBase> create(Type type, size_t size, const BlockPtr& ptr, const std::string& name)
    {
        if (type == Int)
        {
            return std::make_unique<BPlusTree<int>>(ptr, name);
        }
        else if (type == Float)
        {
            return std::make_unique<BPlusTree<float>>(ptr, name);
        }
        else
        {
            return CharTreeCreater<1, 255>::create(size, ptr, name);
        }
    }
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
    std::map<std::string, std::unique_ptr<BPlusTreeBase>> _tables;

    const static char* const FileName;
public:
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
        uint16_t usedBlocks;
        ostream >> usedBlocks;

        for (uint16_t iBlock = 0; iBlock != usedBlocks; iBlock++)
        {
            uint16_t totalEntry;
            ostream >> totalEntry;
            auto& entryBlock = BufferManager::instance().find_or_alloc(FileName, 0, iBlock + 1);
            MemoryReadStream blockStream(entryBlock.as<byte>(), BufferBlock::BlockSize);
            for (uint16_t iEntry = 0; iEntry != totalEntry; iEntry++)
            {
                std::string tableName;
                BlockPtr rootPtr;
                Type type;
                size_t size;
                blockStream >> tableName;
                rootPtr = Serializer<BlockPtr>::deserialize(blockStream);
                blockStream >> type;
                blockStream >> size;
                _tables.insert({tableName, TreeCreater::create(type, size, rootPtr, tableName + "_index")});
            }
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

        size_t iBlock = 0;
        for (const auto& pair : _tables)
        {
            uint16_t totalEntry = 0;

            auto& entryBlock = BufferManager::instance().find_or_alloc(FileName, 0, iBlock + 1);
            MemoryWriteStream blockStream(entryBlock.as<byte>(), BufferBlock::BlockSize);
            while(blockStream.remain() > pair.first.size() + sizeof(BlockPtr) + sizeof(Type) + sizeof(size_t))
            {
                blockStream << pair.first;
                Serializer<BlockPtr>::serialize(blockStream, pair.second->root());
                blockStream << pair.

                totalEntry++;
            }
            ostream << totalEntry;
            iBlock++;
        }

        block.notify_modification();
        block.unlock();
    }
};
