#include "BufferManager.h"

typedef int offsetNumber;



template <typename KeyType>
class TreeNode
{
public:
    size_t count;
    TreeNode* parent;
    std::vector<KeyType> keys;
    std::vector<TreeNode*> childs;
    std::vector<offsetNumber> vals;
    TreeNode* nextLeafNode;
    bool isLeaf;

private:
    int degree;

public:
    TreeNode(int degree, bool newLeaf = false);
    ~TreeNode();
    bool isRoot();
    bool search(KeyType key, size_t &index);
    TreeNode* splite(KeyType &key);
    size_t add(KeyType &key);
    size_t add(KeyType &key, offsetNumber val);
    bool removeAt(size_t index);
};

template <typename KeyType>
class BPlusTree
{
private:
    typedef TreeNode<KeyType>* Node;

    struct searchNodeParse
    {
        Node pNode;
        size_t index;
        bool ifFound;
    };
private:
    std::string fileName;
    Node root;
    Node leafHead;
    size_t keyCount;
    size_t level;
    size_t nodeCount;
    int keySize;
    int degree;
public:
    BPlusTree(const std::string& m_name, int keySize, int degree);
    ~BPlusTree();
    offsetNumber search(KeyType& key);
    bool insertKey(KeyType &key, offsetNumber val);
    bool deleteKey(KeyType &key);
    void dropTree(Node node);
private:
    void init_tree();
    bool adjustAfterinsert(Node pNode);
    bool adjustAfterDelete(Node pNode);
    void findToLeaf(Node pNode, KeyType key, searchNodeParse &snp);
};

template <class KeyType>
TreeNode<KeyType>::TreeNode(int m_degree, bool newLeaf)
    : count(0)
    , parent(nullptr)
    , nextLeafNode(nullptr)
    , isLeaf(newLeaf)
    , degree(m_degree)
{
    for (size_t i = 0; i < degree + 1; i++)
    {
        childs.push_back(nullptr);
        keys.push_back(KeyType());
        vals.push_back(offsetNumber());
    }
    childs.push_back(nullptr);
}

template <class KeyType>
TreeNode<KeyType>::~TreeNode()
{
}

template <class KeyType>
bool TreeNode<KeyType>::isRoot()
{
    if (parent != nullptr) return false;
    else return true;
}

template <class KeyType>
bool TreeNode<KeyType>::search(KeyType key, size_t &index)
{
    if (count == 0)
    {
        index = 0;
        return false;
    }
    else
    {
        if (keys[count - 1] < key)
        {
            index = count;
            return false;
        }
        else if (keys[0] > key)
        {
            index = 0;
            return false;
        }
        else if (count <= 20)
        {
            for (size_t i = 0; i < count; i++)
            {
                if (keys[i] == key)
                {
                    index = i;
                    return true;
                }
                else if (keys[i] < key)
                {
                    continue;
                }
                else if (keys[i] > key)
                {
                    index = i;
                    return false;
                }
            }
        }
        else if (count > 20)
        {
            size_t left = 0, right = count - 1, pos = 0;
            while (right > left + 1)
            {
                pos = (right + left) / 2;
                if (keys[pos] == key)
                {
                    index = pos;
                    return true;
                }
                else if (keys[pos] < key)
                {
                    left = pos;
                }
                else if (keys[pos] > key)
                {
                    right = pos;
                }
            }


            if (keys[left] >= key)
            {
                index = left;
                return (keys[left] == key);
            }
            else if (keys[right] >= key)
            {
                index = right;
                return (keys[right] == key);
            }
            else if (keys[right] < key)
            {
                index = right++;
                return false;
            }
        }
    }
    return false;
}

template <class KeyType>
TreeNode<KeyType>* TreeNode<KeyType>::splite(KeyType &key)
{
    size_t minmumNode = (degree - 1) / 2;
    TreeNode* newNode = new TreeNode(degree, this->isLeaf);

    if (isLeaf)
    {
        key = keys[minmumNode + 1];
        for (size_t i = minmumNode + 1; i < degree; i++)
        {
            newNode->keys[i - minmumNode - 1] = keys[i];
            keys[i] = KeyType();
            newNode->vals[i - minmumNode - 1] = vals[i];
            vals[i] = offsetNumber();
        }
        newNode->nextLeafNode = this->nextLeafNode;
        this->nextLeafNode = newNode;
        newNode->parent = this->parent;
        newNode->count = minmumNode;
        this->count = minmumNode + 1;
    }
    else if (!isLeaf)
    {
        key = keys[minmumNode];
        for (size_t i = minmumNode + 1; i < degree + 1; i++)
        {
            newNode->childs[i - minmumNode - 1] = this->childs[i];
            newNode->childs[i - minmumNode - 1]->parent = newNode;
            this->childs[i] = nullptr;
        }
        for (size_t i = minmumNode + 1; i < degree; i++)
        {
            newNode->keys[i - minmumNode - 1] = this->keys[i];
            this->keys[i] = KeyType();
        }
        this->keys[minmumNode] = KeyType();
        newNode->parent = this->parent;
        newNode->count = minmumNode;
        this->count = minmumNode;
    }
    return newNode;
}

template <class KeyType>
size_t TreeNode<KeyType>::add(KeyType &key)
{
    if (count == 0)
    {
        keys[0] = key;
        count++;
        return 0;
    }
    else
    {
        size_t index = 0;
        bool exist = search(key, index);
        if (exist)
        {
            throw KeyExist(std::to_string(key).c_str());
        }
        else
        {
            for (size_t i = count; i > index; i--)
            {
                keys[i] = keys[i - 1];
            }
            keys[index] = key;
            for (size_t i = count + 1; i > index + 1; i--)
            {
                childs[i] = childs[i - 1];
            }
            childs[index + 1] = nullptr;
            count++;
            return index;
        }
    }
}

template <class KeyType>
size_t TreeNode<KeyType>::add(KeyType &key, offsetNumber val)
{
    assert(!isLeaf);

    if (count == 0)
    {
        keys[0] = key;
        vals[0] = val;
        count++;
        return 0;
    }
    else
    {
        size_t index = 0;
        bool exist = search(key, index);
        if (exist)
        {
            throw KeyExist(std::to_string(key).c_str());
        }
        for (size_t i = count; i > index; i--)
        {
            keys[i] = keys[i - 1];
            vals[i] = vals[i - 1];
        }
        keys[index] = key;
        vals[index] = val;
        count++;
        return index;
    }
}

template <class KeyType>
bool TreeNode<KeyType>::removeAt(size_t index)
{
    assert(index > count);
    if (isLeaf)
    {
        for (size_t i = index; i < count - 1; i++)
        {
            keys[i] = keys[i + 1];
            vals[i] = vals[i + 1];
        }
        keys[count - 1] = KeyType();
        vals[count - 1] = offsetNumber();
    }
    else
    {
        for (size_t i = index; i < count - 1; i++)
        {
            keys[i] = keys[i + 1];
        }
        for (size_t i = index + 1; i < count; i++)
        {
            childs[i] = childs[i + 1];
        }
        keys[count - 1] = KeyType();
        childs[count] = nullptr;
    }
    count--;
    return true;
}

template <class KeyType>
BPlusTree<KeyType>::BPlusTree(const std::string& m_name, int keysize, int m_degree)
    : fileName(m_name)
    , root(nullptr)
    , leafHead(nullptr)
    , keyCount(0)
    , level(0)
    , nodeCount(0)
    , keySize(keysize)
    , degree(m_degree)
{
    init_tree();
}

template <class KeyType>
BPlusTree<KeyType>:: ~BPlusTree()
{
    dropTree(root);
    keyCount = 0;
    root = nullptr;
    level = 0;
}

template <class KeyType>
void BPlusTree<KeyType>::init_tree()
{
    root = new TreeNode<KeyType>(degree, true);
    keyCount = 0;
    level = 1;
    nodeCount = 1;
    leafHead = root;
}

template <class KeyType>
void BPlusTree<KeyType>::findToLeaf(Node pNode, KeyType key, searchNodeParse & snp)
{
    size_t index = 0;
    if (pNode->search(key, index))
    {
        if (pNode->isLeaf)
        {
            snp.pNode = pNode;
            snp.index = index;
            snp.ifFound = true;
        }
        else
        {
            pNode = pNode->childs[index + 1];
            while (!pNode->isLeaf)
            {
                pNode = pNode->childs[0];
            }
            snp.pNode = pNode;
            snp.index = 0;
            snp.ifFound = true;
        }
    }
    else
    {
        if (pNode->isLeaf)
        {
            snp.pNode = pNode;
            snp.index = index;
            snp.ifFound = false;
        }
        else
        {
            findToLeaf(pNode->childs[index], key, snp);
        }
    }
}

template <class KeyType>
bool BPlusTree<KeyType>::insertKey(KeyType &key, offsetNumber val)
{
    searchNodeParse snp;
    if (!root) init_tree();
    findToLeaf(root, key, snp);
    if (snp.ifFound)
    {
        throw KeyExist(std::to_string(key).c_str());
    }
    snp.pNode->add(key, val);
    if (snp.pNode->count == degree)
    {
        adjustAfterinsert(snp.pNode);
    }
    keyCount++;
    return true;
}

template <class KeyType>
bool BPlusTree<KeyType>::adjustAfterinsert(Node pNode)
{
    KeyType key;
    Node newNode = pNode->splite(key);
    nodeCount++;
    if (pNode->isRoot())
    {
        Node root = new TreeNode<KeyType>(degree, false);

        level++;
        nodeCount++;
        this->root = root;
        pNode->parent = root;
        newNode->parent = root;
        root->add(key);
        root->childs[0] = pNode;
        root->childs[1] = newNode;
        return true;
    }
    else
    {
        Node parent = pNode->parent;
        size_t index = parent->add(key);
        parent->childs[index + 1] = newNode;
        newNode->parent = parent;
        if (parent->count == degree)
        {
            return adjustAfterinsert(parent);
        }
        return true;
    }
}

template <class KeyType>
offsetNumber BPlusTree<KeyType>::search(KeyType& key)
{
    if (!root)
    {
        return -1;
    }
    searchNodeParse snp;
    findToLeaf(root, key, snp);
    if (!snp.ifFound)
    {
        return -1;
    }
    else
    {
        return snp.pNode->vals[snp.index];
    }
}

template <class KeyType>
bool BPlusTree<KeyType>::deleteKey(KeyType &key)
{
    searchNodeParse snp;
    assert(root);
    findToLeaf(root, key, snp);
    assert(snp.ifFound);
    if (snp.pNode->isRoot())
    {
        snp.pNode->removeAt(snp.index);
        keyCount--;
        return adjustAfterDelete(snp.pNode);
    }
    else
    {
        if (snp.index == 0 && leafHead != snp.pNode)
        {
            size_t index = 0;
            Node now_parent = snp.pNode->parent;
            bool if_found_inBranch = now_parent->search(key, index);
            while (!if_found_inBranch)
            {
                if (now_parent->parent)
                {
                    now_parent = now_parent->parent;
                }
                else
                {
                    break;
                }
                if_found_inBranch = now_parent->search(key, index);
            }

            now_parent->keys[index] = snp.pNode->keys[1];
            snp.pNode->removeAt(snp.index);
            keyCount--;
            return adjustAfterDelete(snp.pNode);
        }
        else
        {
            snp.pNode->removeAt(snp.index);
            keyCount--;
            return adjustAfterDelete(snp.pNode);
        }
    }
}

template <class KeyType>
bool BPlusTree<KeyType>::adjustAfterDelete(Node pNode)
{
    size_t minmumKey = (degree - 1) / 2;
    if (((pNode->isLeaf) && (pNode->count >= minmumKey)) || ((degree != 3) && (!pNode->isLeaf) && (pNode->count >= minmumKey - 1)) || ((degree == 3) && (!pNode->isLeaf) && (pNode->count < 0)))
    {
        return  true;
    }
    if (pNode->isRoot())
    {
        if (pNode->count > 0)
        {
            return true;
        }
        else
        {
            if (root->isLeaf)
            {
                delete pNode;
                root = nullptr;
                leafHead = nullptr;
                level--;
                nodeCount--;
            }
            else
            {
                root = pNode->childs[0];
                root->parent = nullptr;
                delete pNode;
                level--;
                nodeCount--;
            }
        }
    }
    else
    {
        Node parent = pNode->parent, brother;
        if (pNode->isLeaf)
        {
            size_t index = 0;
            parent->search(pNode->keys[0], index);
            if ((parent->childs[0] != pNode) && (index + 1 == parent->count))
            {
                brother = parent->childs[index];
                if (brother->count > minmumKey)
                {
                    for (size_t i = pNode->count; i > 0; i--)
                    {
                        pNode->keys[i] = pNode->keys[i - 1];
                        pNode->vals[i] = pNode->vals[i - 1];
                    }
                    pNode->keys[0] = brother->keys[brother->count - 1];
                    pNode->vals[0] = brother->vals[brother->count - 1];
                    brother->removeAt(brother->count - 1);
                    ++pNode->count;
                    parent->keys[index] = pNode->keys[0];
                    return true;
                }
                else
                {
                    parent->removeAt(index);
                    for (size_t i = 0; i < pNode->count; i++)
                    {
                        brother->keys[i + brother->count] = pNode->keys[i];
                        brother->vals[i + brother->count] = pNode->vals[i];
                    }
                    brother->count += pNode->count;
                    brother->nextLeafNode = pNode->nextLeafNode;
                    delete pNode;
                    nodeCount--;
                    return adjustAfterDelete(parent);
                }

            }
            else
            {
                if (parent->childs[0] == pNode)
                    brother = parent->childs[1];
                else
                    brother = parent->childs[index + 2];
                if (brother->count > minmumKey)
                {
                    pNode->keys[pNode->count] = brother->keys[0];
                    pNode->vals[pNode->count] = brother->vals[0];
                    ++pNode->count;
                    brother->removeAt(0);
                    if (parent->childs[0] == pNode)
                        parent->keys[0] = brother->keys[0];
                    else
                        parent->keys[index + 1] = brother->keys[0];
                    return true;
                }
                else
                {
                    for (int i = 0; i < brother->count; i++)
                    {
                        pNode->keys[pNode->count + i] = brother->keys[i];
                        pNode->vals[pNode->count + i] = brother->vals[i];
                    }
                    if (pNode == parent->childs[0])
                        parent->removeAt(0);
                    else
                        parent->removeAt(index + 1);
                    pNode->count += brother->count;
                    pNode->nextLeafNode = brother->nextLeafNode;
                    delete brother;
                    nodeCount--;
                    return adjustAfterDelete(parent);
                }
            }
        }
        else
        {
            size_t index = 0;
            parent->search(pNode->childs[0]->keys[0], index);
            if ((parent->childs[0] != pNode) && (index + 1 == parent->count))
            {
                brother = parent->childs[index];
                if (brother->count > minmumKey - 1)
                {

                    pNode->childs[pNode->count + 1] = pNode->childs[pNode->count];
                    for (size_t i = pNode->count; i > 0; i--)
                    {
                        pNode->childs[i] = pNode->childs[i - 1];
                        pNode->keys[i] = pNode->keys[i - 1];
                    }
                    pNode->childs[0] = brother->childs[brother->count];
                    pNode->keys[0] = parent->keys[index];
                    ++pNode->count;

                    parent->keys[index] = brother->keys[brother->count - 1];

                    if (brother->childs[brother->count])
                    {
                        brother->childs[brother->count]->parent = pNode;
                    }
                    brother->removeAt(brother->count - 1);
                    return true;
                }
                else
                {

                    brother->keys[brother->count] = parent->keys[index];
                    parent->removeAt(index);
                    ++brother->count;
                    for (int i = 0; i < pNode->count; i++)
                    {
                        brother->childs[brother->count + i] = pNode->childs[i];
                        brother->keys[brother->count + i] = pNode->keys[i];
                        brother->childs[brother->count + i]->parent = brother;
                    }
                    brother->childs[brother->count + pNode->count] = pNode->childs[pNode->count];
                    brother->childs[brother->count + pNode->count]->parent = brother;
                    brother->count += pNode->count;
                    delete pNode;
                    nodeCount--;
                    return adjustAfterDelete(parent);
                }
            }
            else
            {
                if (parent->childs[0] == pNode)
                    brother = parent->childs[1];
                else
                    brother = parent->childs[index + 2];
                if (brother->count > minmumKey - 1)
                {

                    pNode->childs[pNode->count + 1] = brother->childs[0];
                    pNode->keys[pNode->count] = brother->keys[0];
                    pNode->childs[pNode->count + 1]->parent = pNode;
                    ++pNode->count;

                    if (pNode == parent->childs[0])
                        parent->keys[0] = brother->keys[0];
                    else
                        parent->keys[index + 1] = brother->keys[0];

                    brother->childs[0] = brother->childs[1];
                    brother->removeAt(0);
                    return true;
                }
                else
                {

                    pNode->keys[pNode->count] = parent->keys[index];
                    if (pNode == parent->childs[0])
                        parent->removeAt(0);
                    else
                        parent->removeAt(index + 1);
                    ++pNode->count;
                    for (int i = 0; i < brother->count; i++)
                    {
                        pNode->childs[pNode->count + i] = brother->childs[i];
                        pNode->keys[pNode->count + i] = brother->keys[i];
                        pNode->childs[pNode->count + i]->parent = pNode;
                    }
                    pNode->childs[pNode->count + brother->count] = brother->childs[brother->count];
                    pNode->childs[pNode->count + brother->count]->parent = pNode;
                    pNode->count += brother->count;
                    delete brother;
                    nodeCount--;
                    return adjustAfterDelete(parent);
                }
            }
        }
    }
    return false;
}

template <class KeyType>
void BPlusTree<KeyType>::dropTree(Node node)
{
    if (!node) return;
    if (!node->isLeaf)
    {
        for (size_t i = 0; i <= node->count; i++)
        {
            dropTree(node->childs[i]);
            node->childs[i] = nullptr;
        }
    }
    delete node;
    nodeCount--;
    return;
}

