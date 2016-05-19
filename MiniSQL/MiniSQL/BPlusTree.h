#pragma once

#include "BufferManager.h"
#include "IndexManager.h"

template<typename TKey>
class BPlusTree
{
public:
    struct BTreeNode
    {
        using key_type = TKey;
        const static size_t key_size = sizeof(key_type);
        using ptr_type = BlockPtr;
        const static size_t ptr_size = sizeof(ptr_type);
        const static size_t key_count = (BufferBlock::BlockSize - ptr_size - sizeof(size_t) - sizeof(bool)) / (key_size + ptr_size);
        const static size_t ptr_count = key_count + 1;
        bool is_leaf;
        size_t count;
        key_type keys[key_count];
        ptr_type ptrs[ptr_count];
        
        static_assert(sizeof(BTreeNode) <= BufferBlock::BlockSize, "sizeof(BTreeNode) > BufferBlock::BlockSize");
    };
private:
    BlockPtr _root;

    BTreeNode* find_leaf(const TKey& key)
    {
        BTreeNode* current = &(_root->as<BTreeNode>());
        while (!current->is_leaf)
        {
            auto iFirstGTKey = std::find_if(current->keys, current->keys + current->count, [&key](const key_type& ckey) {
                return ckey >= key;
            });
            if (iFirstGTKey == current->keys + current->count) //iFirstGTKey == end
            {
                auto iLastNonNullptr = std::find_if(std::make_reverse_iterator(current->ptrs + current->count),
                                                    std::make_reverse_iterator(current->ptrs)
                                                    [](const ptr_type& cptr) {
                    return cptr != nullptr;
                });
                current = current->ptrs[iLastNonNullptr]->as<BTreeNode>();
            }
            else if (key == current->keys[iFirstGTKey])
            {
                current = current->ptrs[iFirstGTKey + 1]->as<BTreeNode>();
            }
            else
            {
                current = current->ptrs[iFirstGTKey]->as<BTreeNode>();
            }
        }
        return current;
    }

    BlockPtr find(const TKey& key)
    {
        BTreeNode* targetLeaf = find_leaf(key);

        auto iValue = std::find(targetLeaf->keys, targetLeaf->keys + targetLeaf->count, [&key](const key_type& ckey) {
            return key = ckey;
        });
        if (iValue != targetLeaf->keys + targetLeaf->count)
        {
            return targetLeaf->ptrs[iValue];
        }
        return nullptr;
    }

    void insert_leaf(BTreeNode* leaf, const TKey& key, const BlockPtr& pValue)
    {
        leaf->keys[leaf->count] = key;
        leaf->ptrs[leaf->count] = pValue;
        leaf->count++;
    }

    void insert(const TKey& key, const BlockPtr& pValue)
    {
        BTreeNode* targetLeaf = find_leaf(key);
        if (targetLeaf->count < BTreeNode::key_count)
        {
            insert_leaf(targetLeaf, key, pValue);
        }
        else
        {
            auto newIndex = IndexManager::instance().allocate();
            auto& newBlock = BufferManager::instance().find_or_alloc(newIndex.first, newIndex.second);
            auto newLeaf = &(newBlock.as<BTreeNode>());
            std::copy(targetLeaf->keys, targetLeaf->keys + targetLeaf->count, newLeaf->keys);
            std::copy(targetLeaf->ptrs, targetLeaf->ptrs + targetLeaf->count, newLeaf->ptrs);
            //
        }
    }

};

