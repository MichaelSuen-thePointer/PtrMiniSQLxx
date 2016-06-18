#pragma once

#include "BufferManager.h"

class BPlusTreeBase
{
public:
    virtual ~BPlusTreeBase() {}
    virtual std::vector<BlockPtr> find_le(byte* pkey) = 0;
    virtual std::vector<BlockPtr> find_ge(byte* pkey) = 0;
    virtual std::vector<BlockPtr> find_range(byte* lower, byte* upper) = 0;
    virtual std::vector<BlockPtr> find_all() = 0;

    virtual BlockPtr find_eq(byte* key) = 0;

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
                                     - sizeof(ptr_type)
                                     ) / (key_size + ptr_size);
    const static size_t ptr_count = key_count + 1;
    const static size_t next_index = ptr_count - 1;
#pragma pack(1)
    struct BTreeNodeModel
    {
        bool is_leaf;
        size_t total_key;
        size_t total_ptr;
        ptr_type parent;
        key_type keys[key_count];
        ptr_type ptrs[ptr_count];
    };
#pragma pack()
    static_assert(sizeof(BTreeNodeModel) <= BufferBlock::BlockSize, "sizeof(BTreeNode) > BufferBlock::BlockSize");

    explicit BPlusTree(const BlockPtr& ptr, const std::string& fileName, bool isNew)
        : BPlusTreeBase(ptr, fileName)
    {
        if (isNew)
        {
            TreeNode rootNode{_root};
            rootNode.reset(true);
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
            assert(0);
        }

        void default_initialize()
        {
            for (size_t i = 0; i != capacity(); i++)
            {
                _array[i] = T();
            }
        }

        void append(const T& value)
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
            std::move_backward(where, end(), end() + length);
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
            _base->parent = nullptr;
            _base->total_key = 0;
            _base->total_ptr = 0;

#ifdef _DEBUG
            ptrs().default_initialize();
            keys().default_initialize();
#endif
            if (is_leaf)
            {
                ptrs().capacity(BPlusTree::ptr_count - 1);
            }
            if (is_leaf)
            {
                next_ptr() = nullptr;
            }

            _selfPtr->notify_modification();
        }
        explicit TreeNode(ptr_type blockPtr)
            : TreeNode(*blockPtr)
        {
        }
        explicit TreeNode(BufferBlock& block)
            : _selfPtr(block.ptr())
            , _base(block.as<BTreeNodeModel>())
            , _keys(_base->keys, _base->total_key, BPlusTree::key_count)
            , _ptrs(_base->ptrs, _base->total_ptr, _base->is_leaf ? BPlusTree::ptr_count - 1 : BPlusTree::ptr_count)
        {
            block.lock();
        }
        ~TreeNode()
        {
            if (_selfPtr != nullptr)
            {
                _selfPtr->unlock();
            }
        }
        TreeNode(const TreeNode& other)
            : _selfPtr(other._selfPtr)
            , _base(other._base)
            , _keys(_base->keys, _base->total_key, other._keys.capacity())
            , _ptrs(_base->ptrs, _base->total_ptr, other._ptrs.capacity())
        {
            self_ptr()->lock();
        }
        TreeNode(TreeNode&& other)
            : _selfPtr(other._selfPtr)
            , _base(other._base)
            , _keys(other._keys)
            , _ptrs(other._ptrs)
        {
            other._selfPtr = nullptr;
            other._base = nullptr;
        }

        TreeNode& operator=(TreeNode&& other)
        {
            _selfPtr->unlock();

            _selfPtr = other._selfPtr;
            _base = other._base;
            _keys = other._keys;
            _ptrs = other._ptrs;

            other._selfPtr = nullptr;
            other._base = nullptr;
            return *this;
        }

        TreeNode& operator=(const TreeNode& other)
        {
            _selfPtr->unlock();

            _selfPtr = other._selfPtr;
            _base = other._base;
            _keys = other._keys;
            _ptrs = other._ptrs;

            _selfPtr->lock();
            return *this;
        }

        void insert_after_ptr(size_t i, const key_type& key, const ptr_type& ptr)
        {
            assert(ptr_count() < BPlusTree<TKey>::ptr_count);
            assert(key_count() < BPlusTree<TKey>::key_count);
            assert(i + 1 >= 0 && i + 1 <= ptr_count());
            assert(i >= 0 && i <= key_count());

            std::move_backward(ptrs().begin() + i + 1, ptrs().end(), ptrs().end() + 1);
            ptr_count() += 1;
            ptrs().begin()[i + 1] = ptr;

            std::move_backward(keys().begin() + i, keys().end(), keys().end() + 1);
            key_count() += 1;
            keys().begin()[i] = key;
        }

        void insert_before_ptr(size_t i, const ptr_type& ptr, const key_type& key)
        {
            assert(ptr_count() < BPlusTree<TKey>::ptr_count);
            assert(key_count() < BPlusTree<TKey>::key_count);
            assert(i >= 0 && i <= ptr_count());
            assert(i >= 0 && i <= key_count());

            std::move_backward(ptrs().begin() + i, ptrs().end(), ptrs().end() + 1);
            ptr_count() += 1;
            ptrs().begin()[i] = ptr;

            std::move_backward(keys().begin() + i, keys().end(), keys().end() + 1);
            key_count() += 1;
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
        void remove_entry(const key_type& key, const ptr_type& ptr)
        {
            auto iterEntry = std::find(keys().begin(), keys().end(), key);
            auto offset = iterEntry - keys().begin();
            std::move(keys().begin() + offset + 1, keys().end(), keys().begin() + offset);
            key_count() -= 1;
            if (ptr != nullptr)
            {
                auto iterPtr = std::find(ptrs().begin(), ptrs().end(), ptr);
                offset = iterPtr - ptrs().begin();
            }
            std::move(ptrs().begin() + offset + 1, ptrs().end(), ptrs().begin() + offset);
            ptr_count() -= 1;
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
            auto rawNode = _ptr->as<BTreeNodeModel>();
            if (_i == rawNode->total_ptr - 1)
            {
                _i = 0;
                _ptr = rawNode->ptrs[BPlusTree::next_index];
            }
            else
            {
                _i++;
            }
            return *this;
        }
        explicit operator bool() const
        {
            return _ptr != nullptr;
        }
        BlockPtr operator*()
        {
            return _ptr->as<BTreeNodeModel>()->ptrs[_i];
        }
        bool operator==(const TreeIterator& other) const
        {
            if (other._ptr == nullptr && _ptr == nullptr)
            {
                return true;
            }
            return other._ptr == _ptr && other._i == _i;
        }
        bool operator!=(const TreeIterator& other) const
        {
            return !((*this) == other);
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
                node = TreeNode(node.ptrs().back());
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
        if (leaf.key_count() == 0)
        {
            assert(leaf.ptr_count() == 0);
            leaf.ptrs().append(ptr);
            leaf.keys().append(key);
        }
        else if (key < leaf.keys()[0])
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
        leaf.self_ptr()->notify_modification();
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

    void insert_parent(TreeNode& node, const key_type& key, TreeNode& newNode)
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
            newRoot.ptrs()[1] = newNode.self_ptr();

            node.parent_ptr() = newRoot.self_ptr();
            newNode.parent_ptr() = newRoot.self_ptr();

            _root = newBlock.ptr();
            newBlock.notify_modification();
            node.self_ptr()->notify_modification();
            newNode.self_ptr()->notify_modification();
        }
        else
        {
            //not root
            TreeNode p = node.parent();
            if (p.ptr_count() < p.ptrs().capacity())
            {

                auto iterInsertAfter = std::find(p.ptrs().begin(), p.ptrs().end(), node.self_ptr());
                auto iInsertAfter = iterInsertAfter - p.ptrs().begin();
                p.insert_after_ptr(iInsertAfter, key, newNode.self_ptr());
                newNode.parent_ptr() = p.self_ptr();
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
                auto pair = insert_split_node(p, key, newNode.self_ptr(), node.self_ptr());
                insert_parent(p, pair.second, pair.first);
            }
            p.self_ptr()->notify_modification();
        }
    }

    void remove_entry(TreeNode& node, const key_type& key, const ptr_type& cptr)
    {
        node.remove_entry(key, cptr);
        if (node.parent_ptr() == nullptr)
        {
            if (node.ptr_count() == 1) //is root and only one child
            {
                _root = node.ptrs()[0];
                node.parent_ptr() = nullptr;
                node.self_ptr()->notify_modification();
                BufferManager::instance().drop_block(node.self_ptr());
            }
        }
        else if (node.ptr_count() < node.ptrs().capacity() / 2)
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
                sibling.ptr_count() + node.ptr_count() <= node.ptrs().capacity())
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
                    left->ptrs().append(right->ptrs().begin(), right->ptrs().end());
                    left->next_ptr() = right->next_ptr();
                    remove_entry(right->parent(), sideKey, right->self_ptr());
                    BufferManager::instance().drop_block(right->self_ptr());
                }
                left->self_ptr()->notify_modification();
            }
            else //redistribution
            {
                if (!nodeIsPredecessor)
                {
                    if (!node.is_leaf())
                    { //correct
                        auto lastKey = sibling.keys().back();
                        auto lastPtr = sibling.ptrs().back();
                        sibling.keys().pop_back();
                        sibling.ptrs().pop_back();
                        node.insert_before_ptr(0, lastPtr, sideKey);
                        node.parent().keys().replace(sideKey, lastKey);
                    }
                    else
                    {
                        auto lastKey = sibling.keys().back();
                        auto lastPtr = sibling.ptrs().back();
                        sibling.keys().pop_back();
                        sibling.ptrs().pop_back();
                        node.insert_before_ptr(0, lastPtr, lastKey);
                        node.parent().keys().replace(sideKey, lastKey);
                    }
                }
                else
                {
                    if (!sibling.is_leaf())
                    {
                        auto firstKey = sibling.keys().front();
                        auto firstPtr = sibling.ptrs().front();
                        sibling.keys().erase(0);
                        sibling.ptrs().erase(0);
                        node.keys().append(sideKey);
                        node.ptrs().append(firstPtr);

                        sibling.parent().keys().replace(sideKey, firstKey);
                    }
                    else
                    { //correct
                        auto frontKey = sibling.keys().front();
                        auto frontPtr = sibling.ptrs().front();
                        node.keys().append(frontKey);
                        node.ptrs().append(frontPtr);
                        sibling.keys().erase(0);
                        sibling.ptrs().erase(0);
                        sibling.parent().keys().replace(sideKey, sibling.keys().front());
                    }
                }
                node.self_ptr()->notify_modification();
                sibling.self_ptr()->notify_modification();
            }
        }

    }

    void delete_adjust(const TreeNode& node)
    {

    }

    std::pair<TreeNode, key_type> insert_split_node(TreeNode& node,
                                                    const key_type& key,
                                                    const ptr_type& ptr,
                                                    const ptr_type& nodePtr)
    {
        assert(node.ptr_count() == ptr_count);
        assert(node.key_count() == key_count);

        key_type* temp_keys = new key_type[key_count + 1];
        ptr_type* temp_ptrs = new ptr_type[ptr_count + 1];

        std::copy(node.keys().begin(), node.keys().end(), temp_keys);
        std::copy(node.ptrs().begin(), node.ptrs().end(), temp_ptrs);

        auto iterNodePlace = std::find(temp_ptrs, temp_ptrs + ptr_count, nodePtr);
        assert(iterNodePlace != temp_ptrs + ptr_count);

        auto iNodePlace = iterNodePlace - temp_ptrs;

        std::move_backward(temp_keys + iNodePlace, temp_keys + key_count, temp_keys + key_count + 1);
        std::move_backward(temp_ptrs + iNodePlace + 1, temp_ptrs + ptr_count, temp_ptrs + ptr_count + 1);

        temp_keys[iNodePlace] = key;
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

        for (auto& sonPtr : newNode.ptrs())
        {
            TreeNode son{sonPtr};
            son.parent_ptr() = newNode.self_ptr();
            son.self_ptr()->notify_modification();
        }

        delete[] temp_keys;
        delete[] temp_ptrs;

        newBlock.notify_modification();
        node.self_ptr()->notify_modification();

        return{newNode, tempKey};
    }

    TreeNode insert_split_leaf(TreeNode& node, const key_type& key, const ptr_type& ptr)
    {
        assert(node.ptr_count() == node.ptrs().capacity());
        assert(node.key_count() == node.keys().capacity());

        size_t leafPtrCount = node.ptrs().capacity();

        size_t total = key_count + 1;

        key_type* temp_keys = new key_type[total];
        ptr_type* temp_ptrs = new ptr_type[total];
        std::copy(node.keys().begin(), node.keys().end(), temp_keys);
        std::copy(node.ptrs().begin(), node.ptrs().end(), temp_ptrs);

        if (key < temp_keys[0])
        {
            std::move_backward(temp_keys, temp_keys + total - 1, temp_keys + total);
            std::move_backward(temp_ptrs, temp_ptrs + total - 1, temp_ptrs + total);
            temp_keys[0] = key;
            temp_ptrs[0] = ptr;
        }
        else
        {
            auto place = std::find_if(temp_keys, temp_keys + total - 1, [&key](const key_type& ckey) {
                return ckey > key;
            });
            auto iInsertAfter = place - temp_keys - 1;

            std::move_backward(temp_keys + iInsertAfter + 1, temp_keys + total - 1, temp_keys + total);
            std::move_backward(temp_ptrs + iInsertAfter + 1, temp_ptrs + total - 1, temp_ptrs + total);

            temp_keys[iInsertAfter + 1] = key;
            temp_ptrs[iInsertAfter + 1] = ptr;
        }

        BufferBlock& newBlock = BufferManager::instance().alloc_block(_fileName);
        TreeNode newNode(newBlock);
        newNode.reset(true);
        newNode.next_ptr() = node.next_ptr();
        node.next_ptr() = newNode.self_ptr();


        node.key_count() = total / 2;
        node.ptr_count() = total / 2;
        std::copy(temp_ptrs, temp_ptrs + total / 2, node.ptrs().begin());
        std::copy(temp_keys, temp_keys + total / 2, node.keys().begin());

        newNode.key_count() = total - total / 2;
        newNode.ptr_count() = total - total / 2;
        std::copy(temp_ptrs + total / 2, temp_ptrs + total, newNode.ptrs().begin());
        std::copy(temp_keys + total / 2, temp_keys + total, newNode.keys().begin());

        delete[] temp_ptrs;
        delete[] temp_keys;

        newBlock.notify_modification();
        node.self_ptr()->notify_modification();

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
        assert(iValue >= 0);
        if (iterValue != targetLeaf.keys().end())
        {
            TreeIterator iter{targetLeaf.self_ptr(), (size_t)iValue};
            return iter;
        }
        return{targetLeaf.next_ptr(), 0};
    }

    void remove(const key_type& key)
    {
        auto leaf = find_leaf(key);
        remove_entry(leaf, key, nullptr);
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
            insert_parent(targetLeaf, newNode.keys()[0], newNode);
        }
    }

    TreeIterator begin() const
    {
        TreeNode node{_root};
        while (!node.is_leaf())
        {
            assert(node.ptr_count() > 0);
            node = TreeNode{node.ptrs()[0]};
        }
        if (node.ptr_count() == 0)
        {
            return{nullptr, (size_t)-1};
        }
        return TreeIterator{node.self_ptr(), 0};
    }

    TreeIterator end() const
    {
        return TreeIterator{nullptr, (size_t)-1};
    }

    virtual std::vector<BlockPtr> find_le(byte* pkey) override
    {
        std::vector<BlockPtr> result;
        TreeIterator left = begin();
        TreeIterator right = find(*reinterpret_cast<key_type*>(pkey));
        if (right != end())
        {
            ++right;
        }
        if (left && right)
        {
            while (left != right)
            {
                result.push_back(*left);
                ++left;
            }
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
        auto left = find(*reinterpret_cast<key_type*>(lower));
        auto right = find(*reinterpret_cast<key_type*>(upper));
        if (right != end())
        {
            ++right;
        }
        std::vector<BlockPtr> result;
        if (left)
        {
            while (left != right)
            {
                result.push_back(*left);
                ++left;
            }
        }
        return result;
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

    BlockPtr find_eq(byte* key) override
    {
        TreeNode targetLeaf = find_leaf(*reinterpret_cast<const key_type*>(key));
        auto place = std::find(targetLeaf.keys().begin(), targetLeaf.keys().end(), *reinterpret_cast<const key_type*>(key));
        if (place == targetLeaf.keys().end())
        {
            return nullptr;
        }
        return targetLeaf.ptrs()[place - targetLeaf.keys().begin()];
    }

    std::vector<BlockPtr> find_all() override
    {
        std::vector<BlockPtr> result;
        for (auto iter = begin(); iter != end(); ++iter)
        {
            result.push_back(*iter);
        }
        return result;
    }
};

