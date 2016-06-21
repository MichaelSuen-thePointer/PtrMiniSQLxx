#pragma once

#include "Serialization.h"

struct ArrayDeleter
{
    void operator()(byte* array) const
    {
        delete[] array;
    }
};

class BufferBlock;
class BlockPtr;

class BufferManager : Uncopyable
{
    friend class BufferBlock;
    friend class BlockPtr;
public:
    const static int BlockCount = 128;

private:
    using IndexPair = std::pair<uint32_t, uint32_t>;
    using ListIter = std::list<std::unique_ptr<BufferBlock>>::iterator;

    std::map<uint32_t, std::string> _indexNameMap;
    std::map<std::string, uint32_t> _nameIndexMap;

    std::list<std::unique_ptr<BufferBlock>> _blocks;
    std::map<std::string, std::set<IndexPair>> _freeIndexPairs;

    const static char* const FileName;

    BufferManager()
        : _blocks()
    {
        load();
    }

    void load();

    void save();

public:
    ~BufferManager()
    {
        save();
    }

    //获取管理器的实例
    static BufferManager& instance()
    {
        static BufferManager instance;
        return instance;
    }

    //查找或分配指定的块
    BufferBlock& find_or_alloc(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex);

    //为指定的文件名创建整数token并返回
    uint32_t allocate_file_name_index(const std::string& fileName);
    //给定token返回文件名
    const std::string& check_file_name(uint32_t index);
    //给定文件名返回token
    uint32_t check_file_index(const std::string& file);
    //分配一个块，使用管理器内部索引管理器
    BufferBlock& alloc_block(const std::string& fileName);
    //丢弃一个块
    void drop_block(BufferBlock& block);
    void drop_block(const BlockPtr& block);
    void drop_block(const std::string& name);

    //检查内部索引管理器中是否包含某个块
    bool has_block(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex);
private:
    ListIter find_block(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex);
    void save_block(BufferBlock& block);
    void write_file(const byte* content, const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex);
    byte* read_file(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex);
    BufferBlock& alloc_block(const std::string& name, uint32_t fileIndex, uint32_t blockIndex);
    BufferBlock& replace_lru_block(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex);
};

class BufferBlock : Uncopyable
{
    friend class BufferManager;
    friend class BlockPtr;
public:
    //块大小
    const static int BlockSize = 4096;
private:
    std::unique_ptr<byte, ArrayDeleter> _buffer;
    std::string _fileName;
    uint32_t _fileIndex;
    uint32_t _blockIndex;
    int _lockTimes;
    bool _hasModified;
    //mutable boost::posix_time::ptime _lastModifiedTime;
    uint16_t _offset;

    BufferBlock()
        : BufferBlock(nullptr, "", -1, -1)
    {
    }
    BufferBlock(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex)
        : _buffer(buffer)
        , _fileName(fileName)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _lockTimes(0)
        , _hasModified(false)
        //, _lastModifiedTime(boost::posix_time::microsec_clock::universal_time())
        , _offset(0)
    {
        log("BB: ctor", fileName, fileIndex, blockIndex);
    }
    void release()
    {
        BufferManager::instance().save_block(*this);
    }
public:
    //析构函数
    ~BufferBlock()
    {
        log("BB: block dtor", _fileName, _fileIndex, _blockIndex);
        release();
    }

    //用户对内容进行修改后调用，将Block标记为dirty
    void notify_modification()
    {
        log("BB: get dirty");
        _hasModified = true;
        update_time();
    }

    //更新访问时间
    void update_time() const
    {
        //_lastModifiedTime = boost::posix_time::microsec_clock::universal_time();
    }

    //将Buffer内部的块内容解释为指定的类型
    template<typename T>
    T* as()
    {
        update_time();
        log("BB: content asked", _fileName, _fileIndex, _blockIndex);
        int tempOffset = _offset;
        _offset = 0;
        return reinterpret_cast<T*>(_buffer.get() + tempOffset);
    }

    //获取对应的BlockPtr
    BlockPtr ptr() const;

    //获取Buffer内部块的原生指针
    byte* raw_ptr() { return this->as<byte>(); }

    //确定块是否被锁住
    bool is_locked() const
    {
        return _lockTimes > 0;
    }

    //锁住块
    void lock()
    {
        log("BB: lock", _fileName, _fileIndex, _blockIndex);
        _lockTimes++;
    }

    //解锁块
    void unlock()
    {
        log("BB: unlock", _fileName, _fileIndex, _blockIndex);
        assert(_lockTimes > 0);
        _lockTimes--;
    }
};

class BlockPtr
{
    friend class BufferBlock;
    friend class BufferManager;
    friend class Serializer<BlockPtr>;
private:
    uint32_t _fileNameIndex;
    uint32_t _fileIndex;
    uint32_t _blockIndex;
    uint16_t _offset;
public:
    //构造函数
    BlockPtr(nullptr_t)
        : BlockPtr()
    {
    }
    BlockPtr()
        : BlockPtr(-1, -1, -1, -1)
    {
    }
    BlockPtr(uint32_t fileNameIndex, uint32_t fileIndex, uint32_t blockIndex)
        : BlockPtr(fileNameIndex, fileIndex, blockIndex, 0)
    {
    }
    BlockPtr(uint32_t fileNameIndex, uint32_t fileIndex, uint32_t blockIndex, uint16_t offset)
        : _fileNameIndex(fileNameIndex)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _offset(offset)
    {
        log("BP: ctor", _fileNameIndex, _fileIndex, _blockIndex, _offset);
    }
    //析构函数
    ~BlockPtr()
    {
        if (_fileNameIndex != -1)
        {
            assert(BufferManager::instance().find_or_alloc(BufferManager::instance().check_file_name(_fileNameIndex), _fileIndex, _blockIndex)._offset == 0);
            log("BP: dtor", _fileNameIndex, _fileIndex, _blockIndex, _offset);
        }
    }
    //拷贝构造
    BlockPtr(const BlockPtr& other)
        : BlockPtr(other._fileNameIndex, other._fileIndex, other._blockIndex, other._offset)
    {
    }
    BlockPtr& operator=(const BlockPtr& other)
    {
        _fileNameIndex = other._fileNameIndex;
        _fileIndex = other._fileIndex;
        _blockIndex = other._blockIndex;
        _offset = other._offset;
        return *this;
    }
    BlockPtr& operator=(nullptr_t)
    {
        _fileNameIndex = -1;
        _fileIndex = -1;
        _blockIndex = -1;
        _offset = -1;
        return *this;
    }
    //比较操作
    bool operator==(const BlockPtr& rhs) const
    {
        return _fileNameIndex == rhs._fileNameIndex &&
            _fileIndex == rhs._fileIndex &&
            _blockIndex == rhs._blockIndex &&
            _offset == rhs._offset;
    }
    bool operator!=(const BlockPtr& rhs) const
    {
        return !(*this == rhs);
    }
    //隐式转换操作
    explicit operator bool() const
    {
        return *this != nullptr;
    }
    //获取对应的block
    BufferBlock& operator*()
    {
        log("BP: deref");
        auto& block = BufferManager::instance().find_or_alloc(BufferManager::instance().check_file_name(_fileNameIndex), _fileIndex, _blockIndex);
        block._offset = _offset;
        return block;
    }
    const BufferBlock& operator*() const
    {
        log("BP: deref");
        return **(const_cast<BlockPtr*>(this));
    }
    BufferBlock* operator->()
    {
        log("BP: deref");
        auto& block = BufferManager::instance().find_or_alloc(BufferManager::instance().check_file_name(_fileNameIndex), _fileIndex, _blockIndex);
        block._offset = _offset;
        return &block;
    }
    const BufferBlock* operator->() const
    {
        log("BP: deref");
        return const_cast<BlockPtr*>(this)->operator->();
    }
};

template<>
class Serializer<BlockPtr>
{
public:
    static BlockPtr deserialize(MemoryReadStream& mrs)
    {
        BlockPtr t;
        mrs >> t._fileNameIndex >> t._fileIndex >> t._blockIndex >> t._offset;
        return t;
    }

    static void serialize(MemoryWriteStream& mws, const BlockPtr& value)
    {
        mws << value._fileNameIndex << value._fileIndex << value._blockIndex << value._offset;
    }

    static size_t size(const BlockPtr& value)
    {
        return sizeof(value._fileNameIndex) + sizeof(value._fileIndex)
            + sizeof(value._blockIndex) + sizeof(value._offset);
    }
};

inline BlockPtr BufferBlock::ptr() const
{
    update_time();
    return BlockPtr(BufferManager::instance().check_file_index(_fileName), _fileIndex, _blockIndex, _offset);
}