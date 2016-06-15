#pragma once

#include "BufferManager.h"

class BPlusTreeBase
{
public:
    virtual ~BPlusTreeBase() {}
    virtual std::vector<BlockPtr> find_le(byte* pkey) = 0;
    virtual std::vector<BlockPtr> find_ge(byte* pkey) = 0;
    virtual std::vector<BlockPtr> find_range(byte* lower, byte* upper) = 0;

    virtual void remove(byte* key) = 0;
    virtual void insert(byte* pkey, const BlockPtr& ptr) = 0;

    virtual void drop_tree() = 0;

    const BlockPtr& root() const { return _root; }
    const std::string& file_name() const { return _fileName; }

    BPlusTreeBase(const BlockPtr& root, const std::string& fileName)
        : _root(root)
        , _fileName(fileName)
    {
    }
protected:
    BlockPtr _root;
    std::string _fileName;
};

template<typename TKey>
class BPlusTree : public BPlusTreeBase
{
public:
    using key_type = TKey; // std::array<char, 255>;
    using ptr_type = BlockPtr;
    const static size_t key_size = sizeof(key_type);
    const static size_t ptr_size = sizeof(ptr_type);
    const static size_t key_count = (BufferBlock::BlockSize
                                     - ptr_size
                                     - sizeof(size_t)
                                     - sizeof(size_t)
                                     - sizeof(bool)
                                     - sizeof(BlockPtr)
                                     ) / (key_size + ptr_size);
    const static size_t ptr_count = key_count + 1;
    const static size_t next_index = ptr_count - 1;
    struct BTreeNodeModel
    {
        bool is_leaf;
        size_t total_key;
        size_t total_ptr;
        ptr_type parent;
        key_type keys[key_count];
        ptr_type ptrs[ptr_count];
    };
    static_assert(sizeof(BTreeNodeModel) <= BufferBlock::BlockSize, "sizeof(BTreeNode) > BufferBlock::BlockSize");

    explicit BPlusTree(const BlockPtr& ptr, const std::string& fileName, bool isNew)
        : BPlusTreeBase(ptr, fileName)
    {
        if (isNew)
        {
            TreeNode{_root}.reset(true);
        }
    }
private:
    template<typename T>
    class OberservableArray
    {
    private:
        T* _array;
        size_t* _psize;
        size_t _capacity;
    public:
        OberservableArray(T* base, size_t& size, size_t capacity)
            : _array(base)
            , _psize(&size)
            , _capacity(capacity)
        {
        }
        T& operator[](size_t i) { assert(i < *_psize); return _array[i]; }
        T* begin() { return _array; }
        T* end() { return _array + *_psize; }
        void capacity(size_t size) { _capacity = size; }
        size_t capacity() const { return _capacity; }
        size_t& size() { return *_psize; }
        const size_t& size() const { return *_psize; }

        T& front()
        {
            assert(size());
            return *_array;
        }

        T& back()
        {
            assert(size());
            return _array[size() - 1];
        }

        void replace(const T& oldOne, const T& newVal)
        {
            for (auto& elm : *this)
            {
                if (elm == oldOne)
                {
                    elm = newVal;
                    return;
                }
            }
        }

        void append(T value)
        {
            assert(size() < capacity());
            _array[size()] = value;
            (*_psize)++;
        }

        void append(T* from, T* to)
        {
            insert(end(), from, to);
        }

        void insert(T* where, T* from, T* to)
        {
            int length = to - from;
            assert(capacity() - size() >= (size_t)(to - from));
            std::copy(where, end(), where + length);
            std::copy(from, to, where);
            size() += length;
        }

        void erase(size_t where)
        {
            assert(where >= 0 && where < size());
            std::move(_array + where + 1, _array + size(), _array + where);
            size()--;
        }

        void pop_back()
        {
            assert(size() > 0);
            size()--;
        }

        void pop_back_n(size_t n)
        {
            assert(size() >= n);
            size() -= n;
        }
    };

    class TreeNode
    {
        //friend class BPlusTree<TKey>;
        friend class BPlusTree;
        ptr_type _selfPtr;
        BTreeNodeModel* _base;
        mutable OberservableArray<key_type> _keys;
        mutable OberservableArray<ptr_type> _ptrs;
    public:
        bool is_leaf() const { return _base->is_leaf; }
        bool is_root() { return parent_ptr() != nullptr; }

        size_t& key_count() { return _base->total_key; }
        size_t& ptr_count() { return _base->total_ptr; }

        TreeNode parent() const { return TreeNode(_base->parent); }
        ptr_type& parent_ptr() { return _base->parent; }

        TreeNode next() const { assert(is_leaf()); return TreeNode(_base->ptrs[next_index]); }
        ptr_type& next_ptr() { assert(is_leaf()); return _base->ptrs[next_index]; }

        size_t self_pos()
        {
            assert(!is_root());
            auto selfPos = std::find(parent().ptrs().begin(), parent().ptrs().end(), self_ptr());
            if (selfPos != parent().ptrs().end())
            {
                return selfPos - parent().ptrs().begin();
            }
            return -1;
        }

        std::pair<key_type, ptr_type> right_key_and_sibling()
        {
            assert(parent_ptr());
            auto selfPos = self_pos();
            assert(selfPos != -1);
            if (selfPos < parent().ptr_count() - 1)
            {
                return{parent().keys()[selfPos], parent().ptrs()[selfPos + 1]};
            }
            return{key_type(), nullptr};
        }

        std::pair<key_type, ptr_type> left_key_and_sibling()
        {
            assert(parent_ptr() != nullptr);
            auto selfPos = self_pos();
            assert(selfPos != -1);
            if (selfPos > 1)
            {
                return{parent().keys()[selfPos - 1], parent().ptrs()[selfPos - 1]};
            }
            return{key_type(), nullptr};
        }

        ptr_type self_ptr() const { return _selfPtr; }

        OberservableArray<key_type>& keys() { return _keys; }
        OberservableArray<ptr_type>& ptrs() { return _ptrs; }
        void reset(bool is_leaf = false)
        {
            _base->is_leaf = is_leaf;
            _base->total_key = 0;
            _base->total_ptr = 0;
            _base->parent = nullptr;

            for (size_t i = 0; i != BPlusTree::ptr_count; i++)
            {
                ptrs()[i] = nullptr;
            }
            if (is_leaf)
            {
                ptrs().capacity(BPlusTree::ptr_count - 1);
            }
            for (size_t i = 0; i != BPlusTree::key_count; i++)
            {
                keys()[i] = key_type{};
            }
        }
        explicit TreeNode(ptr_type blockPtr)
            : _selfPtr(blockPtr)
            , _base(blockPtr->as<BTreeNodeModel>())
            , _keys(_base->keys, _base->total_key, BPlusTree::key_count)
            , _ptrs(_base->ptrs, _base->total_ptr, _base->is_leaf ? BPlusTree::ptr_count - 1 : BPlusTree::ptr_count)
        {
        }
        explicit TreeNode(BufferBlock& block)
            : TreeNode(block.ptr())
        {
        }

        TreeNode(const TreeNode& other)
            : _selfPtr(other._selfPtr)
            , _base(other._base)
            , _keys(_base->keys, _base->total_key, other._keys.capacity())
            , _ptrs(_base->ptrs, _base->total_ptr, other._ptrs.capacity())
        {
        }

        TreeNode& operator=(const TreeNode& other)
        {
            _selfPtr = other._selfPtr;
            _base = other._base;
            _keys = other._keys;
            _ptrs = other._ptrs;

            return *this;
        }

        void insert_after_ptr(size_t i, const key_type& key, const ptr_type& ptr)
        {
            assert(ptr_count() < BPlusTree/*<TKey>*/::ptr_count);
            assert(key_count() < BPlusTree/*<TKey>*/::key_count);
            assert(i + 1 >= 0 && i + 1 < ptr_count());
            assert(i >= 0 && i < key_count());

            ptr_count() += 1;
            std::move(ptrs().begin() + i + 1, ptrs().end(), ptrs().begin() + i + 2);
            ptrs().begin()[i + 1] = ptr;

            key_count() += 1;
            std::move(keys().begin() + i, keys().end(), keys().begin() + i + 1);
            keys().begin()[i] = key;
        }
        void insert_before_ptr(size_t i, const ptr_type& ptr, const key_type& key)
        {
            assert(ptr_count() < BPlusTree/*<TKey>*/::ptr_count);
            assert(key_count() < BPlusTree/*<TKey>*/::key_count);
            assert(i >= 0 && i < ptr_count());
            assert(i >= 0 && i < key_count());

            ptr_count() += 1;
            std::move(ptrs().begin() + i, ptrs().end(), ptrs().begin() + i + 1);
            ptrs().begin()[i] = ptr;

            key_count() += 1;
            std::move(keys().begin() + i, keys().end(), keys().begin() + i + 1);
            keys().begin()[i] = key;
        }
        void insert_after_key(size_t i, const ptr_type& ptr, const key_type& key)
        {
            insert_before_ptr(i + 1, ptr, key);
        }
        void insert_before_key(size_t i, const key_type& key, const ptr_type& ptr)
        {
            insert_after_ptr(i, key, ptr);
        }
        void remove_entry(const key_type& key)
        {
            auto iterEntry = std::find(keys().begin(), keys().end(), key);
            if (iterEntry != keys().end())
            {
                auto offset = iterEntry - keys().begin();
                std::move(keys().begin() + offset + 1, keys().end(), keys().begin() + offset);
                std::move(ptrs().begin() + offset + 1, ptrs().end(), ptrs().begin() + offset);
                key_count() -= 1;
                ptr_count() -= 1;
            }
        }
    };

    class TreeIterator
    {
    private:
        BlockPtr _ptr;
        size_t _i;
    public:
        TreeIterator(const BlockPtr& leafPtr, size_t i)
            : _ptr(leafPtr)
            , _i(i)
        {
        }

        TreeIterator& operator++()
        {
            if (TreeNode{_ptr}.ptr_count() - 1 == _i)
            {
                _i = 0;
                _ptr = TreeNode{_ptr}.next_ptr();
            }
            _i++;
            return;
        }
        operator bool() const
        {
            return _ptr != nullptr;
        }
        BlockPtr operator*() const
        {
            return TreeNode{_ptr}.ptrs()[_i];
        }
        bool operator==(const TreeIterator& other) const
        {
            if (other._ptr == nullptr && _ptr == nullptr)
            {
                return true;
            }
            return other._ptr == _ptr && other._i == _i;
        }

    };


    TreeNode find_leaf(const key_type& key)
    {
        TreeNode node(_root);
        while (!node.is_leaf())
        {
            auto iterFirstGTKey = std::find_if(node.keys().begin(), node.keys().end(), [&key](const key_type& ckey) {
                return ckey >= key;
            });
            auto iFirstGTKey = iterFirstGTKey - node.keys().begin();
            if (iterFirstGTKey == node.keys().end()) //iFirstGTKey == end
            {
                auto iLastNonNullptr = std::find_if(std::make_reverse_iterator(node.ptrs().end()),
                                                    std::make_reverse_iterator(node.ptrs().begin()),
                                                    [](const ptr_type& cptr) {
                    return cptr != ptr_type(nullptr);
                });
                node = TreeNode(*iLastNonNullptr);
            }
            else if (key == node.keys()[iFirstGTKey])
            {

                node = TreeNode(node.ptrs()[iFirstGTKey + 1]);
            }
            else
            {
                node = TreeNode(node.ptrs()[iFirstGTKey]);
            }
        }
        return node;
    }


    static void insert_leaf(TreeNode& leaf, const key_type& key, const BlockPtr& ptr)
    {
        /*
        auto place = std::find_if(leaf.keys().begin(), leaf.keys().end(), [&key](const key_type& ckey) {
            return ckey > key;
        });
        auto offset = place - leaf.keys().begin();
        assert(leaf.keys().begin() + offset + 1 < leaf.keys().begin() + key_count);
        std::move(leaf.keys().begin() + offset, leaf.keys().end(), leaf.keys().begin() + offset + 1);
        std::move(leaf.ptrs().begin() + offset, leaf.ptrs().end(), leaf.ptrs().begin() + offset + 1);

        leaf.keys()[offset] = key;
        leaf.ptrs()[offset] = pValue;
        ++leaf.keys().count();
        ++leaf.ptrs().count();
        */
        if (key < leaf.keys()[0])
        {
            leaf.insert_before_ptr(0, ptr, key);
        }
        else
        {
            auto place = std::find_if(leaf.keys().begin(), leaf.keys().end(), [&key](const key_type& ckey) {
                return ckey > key;
            });
            auto iInsertAfter = place - leaf.keys().begin() - 1;
            leaf.insert_after_key(iInsertAfter, ptr, key);
        }
    }
    static key_type find_min_key(TreeNode& leaf)
    {
        assert(leaf.is_leaf());
        assert(leaf.key_count() > 0);
        key_type minKey = leaf.keys()[0];
        for (auto& key : leaf.keys())
        {
            if (key < minKey)
            {
                minKey = key;
            }
        }
        return minKey;
    }

    template<typename TContainer, typename T>
    static void sorted_insert(TContainer& cont, const T& value)
    {
        auto place = std::find_if(cont.begin(), cont.end(), [&value](const T& cvalue) {
            return cvalue > value;
        });
        place_insert(cont, place - cont.begin(), value);
    }

    template<typename TContainer, typename T>
    static void place_insert(TContainer& cont, size_t place, const T& value)
    {
        cont.count() += 1;
        std::move(cont.begin() + place, cont.end(), cont.begin() + place + 1);
        cont[place] = value;
    }

    void insert_parent(TreeNode& node, const key_type& key, const BlockPtr& ptr)
    {
        if (node.parent_ptr() == nullptr) //node is _root
        {
            auto& newBlock = BufferManager::instance().alloc_block(_fileName);
            TreeNode newRoot(newBlock);
            newRoot.reset(false);
            newRoot.key_count() = 1;
            newRoot.keys()[0] = key;
            newRoot.ptr_count() = 2;
            newRoot.ptrs()[0] = node.self_ptr();
            newRoot.ptrs()[1] = ptr;
            _root = newBlock.ptr();
            return;
        }
        //not root
        TreeNode p = node.parent();
        if (p.ptr_count() < ptr_count)
        {

            auto iterInsertAfter = std::find(p.ptrs().begin(), p.ptrs().end(), node.self_ptr());
            auto iInsertAfter = iterInsertAfter - p.ptrs().begin();
            p.insert_after_ptr(iInsertAfter, key, ptr);
            /*
            p.ptrs().count() += 1;
            std::move(p.ptrs().begin() + iNode, p.ptrs().end(), p.ptrs().begin() + iNode + 1);
            p.ptrs()[iNode] = ptr;

            p.keys().count() += 1;
            std::move(p.keys().begin() + iNode, p.keys().end(), p.keys().begin() + iNode + 1);
            p.keys()[iNode] = key;
            */
        }
        else
        {
            auto pair = insert_split_node(p, key, ptr);
            insert_parent(p, pair.second, pair.first.self_ptr());
        }
    }

    void remove_entry(TreeNode& node, const key_type& key)
    {
        node.remove_entry(key);
        if (node.parent_ptr() == nullptr && node.ptr_count() == 1) //is root and only one child
        {
            _root = node.ptrs()[0];
            BufferManager::instance().drop_block(node.self_ptr());
        }
        else if (node.ptr_count() < ptr_count / 2)
        {
            bool nodeIsPredecessor = true;
            auto psibling = node.right_key_and_sibling();
            if (psibling.second == nullptr)
            {
                nodeIsPredecessor = false;
                psibling = node.left_key_and_sibling();
            }
            assert(psibling.second != nullptr);
            TreeNode sibling(psibling.second);
            key_type sideKey = psibling.first;
            if (sibling.key_count() + node.key_count() <= key_count &&
                sibling.ptr_count() + node.ptr_count() <= ptr_count)
            {
                TreeNode* left = &sibling;
                TreeNode* right = &node;
                if (nodeIsPredecessor)
                {
                    left = &node;
                    right = &sibling;
                }
                if (!right->is_leaf())
                {
                    left->keys().append(sideKey);
                    left->keys().append(right->keys().begin(), right->keys().end());
                    left->ptrs().append(right->ptrs().begin(), right->ptrs().end());
                }
                else
                {
                    left->keys().append(right->keys().begin(), right->keys().end());
                    left->ptrs().append(right->ptrs().begin(), right->ptrs().end() - 1);
                    left->next_ptr() = right->next_ptr();
                    remove_entry(right->parent(), sideKey);
                    BufferManager::instance().drop_block(right->self_ptr());
                }
            }
            else //redistribution
            {
                if (!nodeIsPredecessor)
                {
                    if (!node.is_leaf())
                    {
                        auto ptr = sibling.ptrs().back();
                        sibling.ptrs().pop_back();
                        auto skey = sibling.keys().back();
                        sibling.keys().pop_back();
                        node.insert_before_ptr(0, ptr, sideKey);
                        node.parent().keys().replace(sideKey, skey);
                    }
                    else
                    {
                        auto lastKey = sibling.keys().back();
                        auto preLastPtr = sibling.ptrs()[sibling.keys().size() - 1];
                        sibling.keys().pop_back();
                        sibling.ptrs().erase(sibling.keys().size() - 1);
                        node.insert_before_ptr(0, preLastPtr, lastKey);
                        node.parent().keys().replace(sideKey, lastKey);
                    }
                }
                else
                {
                    if (!sibling.is_leaf())
                    {
                        auto ptr = node.ptrs().back();
                        node.ptrs().pop_back();
                        auto skey = node.keys().back();
                        node.keys().pop_back();
                        sibling.insert_before_ptr(0, ptr, sideKey);
                        sibling.parent().keys().replace(sideKey, skey);
                    }
                    else
                    {
                        auto lastKey = node.keys().back();
                        auto preLastPtr = node.ptrs()[node.keys().size() - 1];
                        node.keys().pop_back();
                        node.ptrs().erase(node.keys().size() - 1);
                        sibling.insert_before_ptr(0, preLastPtr, lastKey);
                        sibling.parent().keys().replace(sideKey, lastKey);
                    }
                }
            }
        }
    }


    std::pair<TreeNode, key_type> insert_split_node(TreeNode& node, const key_type& key, const ptr_type& ptr)
    {
        assert(node.ptr_count() == ptr_count);
        assert(node.key_count() == key_count);

        key_type* temp_keys = new key_type[key_count + 1];
        ptr_type* temp_ptrs = new ptr_type[ptr_count + 1];

        std::copy(node.keys().begin(), node.keys().end(), temp_keys);
        std::copy(node.ptrs().begin(), node.ptrs().end(), temp_ptrs);

        auto iterNodePlace = std::find(temp_ptrs, temp_ptrs + ptr_count, node.self_ptr());

        auto iNodePlace = iterNodePlace - temp_ptrs;

        std::move(temp_keys + iNodePlace + 1, temp_keys + key_count, temp_keys + iNodePlace + 2);
        std::move(temp_ptrs + iNodePlace + 1, temp_ptrs + ptr_count, temp_ptrs + iNodePlace + 2);

        temp_keys[iNodePlace + 1] = key;
        temp_ptrs[iNodePlace + 1] = ptr;

        BufferBlock& newBlock = BufferManager::instance().alloc_block(_fileName);
        TreeNode newNode(newBlock);

        newNode.reset(false);

        node.ptr_count() = ptr_count / 2;
        node.key_count() = ptr_count / 2 - 1;

        std::copy(temp_ptrs, temp_ptrs + ptr_count / 2, node.ptrs().begin());
        std::copy(temp_keys, temp_keys + ptr_count / 2 - 1, node.keys().begin());

        auto tempKey = temp_keys[ptr_count / 2 - 1];

        newNode.ptr_count() = ptr_count + 1 - ptr_count / 2;
        newNode.key_count() = newNode.ptr_count() - 1;

        std::copy(temp_ptrs + ptr_count / 2, temp_ptrs + ptr_count + 1, newNode.ptrs().begin());
        std::copy(temp_keys + ptr_count / 2, temp_keys + key_count + 1, newNode.keys().begin());

        delete[] temp_keys;
        delete[] temp_ptrs;

        return std::make_pair(newNode, tempKey);
    }

    TreeNode insert_split_leaf(TreeNode& node, const key_type& key, const ptr_type& ptr)
    {
        assert(node.ptr_count() == ptr_count);
        assert(node.key_count() == key_count);

        key_type* temp_keys = new key_type[key_count + 1];
        ptr_type* temp_ptrs = new ptr_type[ptr_count];
        std::copy(node.keys().begin(), node.keys().end(), temp_keys);
        std::copy(node.ptrs().begin(), node.ptrs().end() - 1, temp_ptrs);

        if (key < temp_keys[0])
        {
            std::move(temp_keys, temp_keys + key_count, temp_keys + 1);
            std::move(temp_ptrs, temp_ptrs + ptr_count - 1, temp_ptrs + 1);
            temp_keys[0] = key;
            temp_ptrs[0] = ptr;
        }
        else
        {
            auto place = std::find_if(temp_keys, temp_keys + key_count, [&key](const key_type& ckey) {
                return ckey > key;
            });
            auto iInsertAfter = place - temp_keys - 1;

            std::move(temp_keys + iInsertAfter, temp_keys + key_count, temp_keys + iInsertAfter + 1);
            std::move(temp_ptrs + iInsertAfter, temp_ptrs + ptr_count - 1, temp_ptrs + iInsertAfter + 1);

            temp_keys[iInsertAfter] = key;
            temp_ptrs[iInsertAfter] = ptr;
        }

        BufferBlock& newBlock = BufferManager::instance().alloc_block(_fileName);
        TreeNode newNode(newBlock);
        newNode.reset(true);
        newNode.next_ptr() = node.next_ptr();
        node.next_ptr() = newNode.self_ptr();
        node.key_count() = ptr_count / 2;
        node.ptr_count() = ptr_count / 2;
        std::copy(temp_ptrs, temp_ptrs + ptr_count / 2, node.ptrs().begin());
        std::copy(temp_keys, temp_keys + ptr_count / 2, node.keys().begin());

        newNode.key_count() = ptr_count - ptr_count / 2;
        newNode.key_count() = ptr_count - ptr_count / 2;
        std::copy(temp_ptrs + ptr_count / 2, temp_ptrs + ptr_count, newNode.ptrs().begin());
        std::copy(temp_keys + ptr_count / 2, temp_keys + ptr_count, newNode.keys().begin());

        delete[] temp_keys;
        delete[] temp_ptrs;

        return newNode;
    }

public:
    TreeIterator find(const key_type& key)
    {
        TreeNode targetLeaf = find_leaf(key);

        auto iterValue = std::find_if(targetLeaf.keys().begin(), targetLeaf.keys().end(), [&key](const key_type& ckey) {
            return ckey >= key;
        });
        auto iValue = iterValue - targetLeaf.keys().begin();
        if (iterValue != targetLeaf.keys().end())
        {
            return{targetLeaf.self_ptr(), iValue};
        }
        return{nullptr, -1};
    }

    void remove(const key_type& key)
    {
        auto leaf = find_leaf(key);
        remove_entry(leaf, key);
    }

    void insert(const key_type& key, const ptr_type& ptr)
    {
        TreeNode targetLeaf = find_leaf(key);
        if (targetLeaf.key_count() < key_count)
        {
            insert_leaf(targetLeaf, key, ptr);
        }
        else
        {
            TreeNode newNode = insert_split_leaf(targetLeaf, key, ptr);
            insert_parent(targetLeaf, newNode.keys()[0], newNode.self_ptr());
        }
    }

    TreeIterator left_most() const
    {
        TreeNode node{_root};
        while (!node.is_leaf())
        {
            assert(node.ptr_count() > 0);
            node = TreeNode{node.ptrs()[0]};
        }
        if (node.ptr_count() == 0)
        {
            return{nullptr, -1};
        }
        return TreeIterator{node.self_ptr(), 0};
    }

    TreeIterator right_most() const
    {
        TreeNode node{_root};
        while (!node.is_leaf())
        {
            assert(node.ptr_count() > 0);
            node = TreeNode{node.ptrs().back()};
        }
        if (node.ptr_count() == 0)
        {
            return{nullptr, -1};
        }
        return TreeIterator{node.ptrs().back(), node.ptrs().size() - 1};
    }

    virtual std::vector<BlockPtr> find_le(byte* pkey) override
    {
        std::vector<BlockPtr> result;
        TreeIterator left = left_most();
        TreeIterator right = find(*reinterpret_cast<key_type*>(pkey));
        if (left && right)
        {
            while (left != right)
            {
                result.push_back(*left);
                ++left;
            }
            result.push_back(*right);
        }
        return result;
    }
    virtual std::vector<BlockPtr> find_ge(byte* pkey) override
    {
        std::vector<BlockPtr> result;
        TreeIterator left = find(*reinterpret_cast<key_type*>(pkey));
        while (left)
        {
            result.push_back(*left);
            ++left;
        }
        return result;
    }
    virtual std::vector<BlockPtr> find_range(byte* lower, byte* upper) override
    {
        auto left = find(lower);
        auto right = find(upper);

        
    }

    void remove(byte* pkey) override
    {
        remove(*reinterpret_cast<const key_type*>(pkey));
    }
    void insert(byte* pkey, const BlockPtr& ptr) override
    {
        insert(*reinterpret_cast<const key_type*>(pkey), ptr);
    }

public:

    virtual void drop_tree() override
    {
        BufferManager::instance().drop_block(_fileName);
    }
};

