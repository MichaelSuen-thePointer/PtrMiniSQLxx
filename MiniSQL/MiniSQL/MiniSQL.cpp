#include "stdafx.h"
#include "IndexManager.h"

int main()
{
    std::unique_ptr<BPlusTreeBase> tree(
        TreeCreater::create(Int, 4,
        BufferManager::instance().alloc_block("test_btree_record").ptr(),
        "test_btree_record", true));

    int counter = 0;
    for (int i = 0; i != 512; i += 2)
    {
        tree->insert((byte*)&i, BufferManager::instance().find_or_alloc("placeholderblock", 0, counter++).ptr());
    }
    for (int i = 1; i < 1024; i += 2)
    {
        tree->insert((byte*)&i, BufferManager::instance().find_or_alloc("placeholderblock", 0, counter++).ptr());
    }
    int upper = 512;
    auto result = tree->find_le((byte*)&upper);

}