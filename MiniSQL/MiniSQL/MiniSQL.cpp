#include "stdafx.h"
#include "IndexManager.h"

int main()
{
    std::unique_ptr<BPlusTreeBase> tree(
        TreeCreater::create(Int, 4,
                            BufferManager::instance().alloc_block("test_btree_record").ptr(),
                            "test_btree_record", true));

    int arr[] = {1,2,3,4,5,6,7,8,9,10};
    for (auto& i : arr)
    {
        tree->insert((byte*)&i, nullptr);
    }
}