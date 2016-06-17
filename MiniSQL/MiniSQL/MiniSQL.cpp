#include "stdafx.h"
#include "IndexManager.h"

int main()
{
    std::unique_ptr<BPlusTreeBase> tree(
        TreeCreater::create(Int, 4,
        BufferManager::instance().alloc_block("test_btree_record").ptr(),
        "test_btree_record", true));

    int counter = 0;

    for (int i = 0; i < 10000; i ++)
    {
        tree->insert((byte*)&i, BufferManager::instance().find_or_alloc("placeholderblock", 0, counter++).ptr());
    }

    int x = 200;
    tree->remove((byte*)&x);
    x++;
    tree->remove((byte*)&x);
    x++;
    tree->remove((byte*)&x);

    x = 0;
    tree->remove((byte*)&x);
    x++;
    tree->remove((byte*)&x);
    x++;
    tree->remove((byte*)&x);
    
}