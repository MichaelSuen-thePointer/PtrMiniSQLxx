#include "stdafx.h"
#include "IndexManager.h"

int main()
{
    std::unique_ptr<BPlusTreeBase> tree(
        TreeCreater::create(Int, 4,
                            BufferManager::instance().alloc_block("test_btree_record").ptr(),
                            "test_btree_record", true));

    BlockPtr placeholder{255,255,255,0};
    for (int i = 0; i != 255; i++)
    {
        tree->insert((byte*)&i, placeholder);
    }
}