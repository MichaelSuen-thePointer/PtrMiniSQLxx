#pragma once

struct BufferArrayDeleter
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
    using IndexPair = std::pair<int, int>;
    std::set<IndexPair> _indices;
    std::set<IndexPair> _freeIndices;
    std::array<std::unique_ptr<BufferBlock>, BlockCount> _blocks;
    BufferManager()
        : _indices()
        , _freeIndices()
        , _blocks()
    {
    }
public:
    static BufferManager& instance()
    {
        static BufferManager instance;
        return instance;
    }

    BufferBlock& find_or_alloc(int fileIndex, int blockIndex);
    BufferBlock& alloc_block();
private:
    size_t find_block(int fileIndex, int blockIndex);
    void save_block(BufferBlock& block);
    void drop_block(BufferBlock& block);
    void write_file(const byte* content, int fileIndex, int blockIndex);
    byte* read_file(byte* buffer, int fileIndex, int blockIndex);
    BufferBlock& alloc_block(int fileIndex, int blockIndex);
    BufferBlock& replace_lru_block(byte* buffer, int fileIndex, int blockIndex);

    IndexPair allocate_index();
    void deallocate_index(int fileIndex, int blockIndex);
};

class BufferBlock : Uncopyable
{
    friend class BufferManager;
    friend class BlockPtr;
public:
    const static int BlockSize = 4096;
private:
    std::unique_ptr<byte, BufferArrayDeleter> _buffer;
    int _fileIndex;
    int _blockIndex;
    bool _isLocked;
    boost::posix_time::ptime _lastModifiedTime;
    int _offset;

    BufferBlock()
        : BufferBlock(nullptr, -1, -1)
    {
    }
    BufferBlock(byte* buffer, int fileIndex, int blockIndex)
        : _buffer(buffer)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _isLocked(false)
        , _lastModifiedTime(boost::posix_time::microsec_clock::universal_time())
        , _offset(0)
    {
    }
    void release()
    {
        BufferManager::instance().save_block(*this);
    }
public:
    ~BufferBlock()
    {
        release();
    }
    void reset()
    {
        release();
        _buffer.reset();
        _fileIndex = -1;
        _blockIndex = -1;
        _isLocked = false;
        _lastModifiedTime = boost::posix_time::special_values::not_a_date_time;
    }
    void update_time()
    {
        _lastModifiedTime = boost::posix_time::microsec_clock::universal_time();
    }
    template<typename T>
    T* as()
    {
        return reinterpret_cast<T*>(_buffer.get() + _offset);
    }
    BlockPtr ptr();
    explicit operator bool() const
    {
        return _buffer != nullptr;
    }
    bool is_locked() const
    {
        return _isLocked;
    }
    void lock()
    {
        _isLocked = true;
    }
    void unlock()
    {
        assert(_isLocked == true);
        _isLocked = false;
    }

};

class BlockPtr
{
    friend class BufferBlock;
private:
    int _fileIndex;
    int _blockIndex;
    int _offset;
public:
    explicit BlockPtr(nullptr_t)
        : BlockPtr()
    {
    }
    BlockPtr()
        : BlockPtr(-1, -1)
    {
    }
    BlockPtr(int fileIndex, int blockIndex)
        : BlockPtr(fileIndex, blockIndex, 0)
    {
    }
    BlockPtr(int fileIndex, int blockIndex, int offset)
        : _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _offset(offset)
    {
    }
    bool operator==(const BlockPtr& rhs) const
    {
        return _fileIndex == rhs._fileIndex && _blockIndex == rhs._blockIndex;
    }
    bool operator!=(const BlockPtr& rhs) const
    {
        return !(*this == rhs);
    }
    explicit operator bool() const
    {
        return _fileIndex != -1 && _blockIndex != -1;
    }
    BufferBlock& operator*()
    {
        auto& block = BufferManager::instance().find_or_alloc(_fileIndex, _blockIndex);
        block._offset = _offset;
        return block;
    }
    const BufferBlock& operator*() const
    {
        return **(const_cast<BlockPtr*>(this));
    }
    BufferBlock* operator->()
    {
        auto& block = BufferManager::instance().find_or_alloc(_fileIndex, _blockIndex);
        block._offset = _offset;
        return &block;
    }
    const BufferBlock* operator->() const
    {
        return const_cast<BlockPtr*>(this)->operator->();
    }
};

class InsuffcientSpace : public std::runtime_error
{
public:
    explicit InsuffcientSpace(const char* msg)
        : std::runtime_error(msg)
    {
    }
};

inline BlockPtr BufferBlock::ptr()
{
    return BlockPtr(_fileIndex, _blockIndex, _offset);
}