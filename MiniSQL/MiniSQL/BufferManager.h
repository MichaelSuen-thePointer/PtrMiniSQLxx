#pragma once

struct BufferArrayDeleter
{
    void operator()(byte* array)
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
    std::array<std::unique_ptr<BufferBlock>, BlockCount> _blocks;
    BufferManager();
public:
    static BufferManager& instance();
private:
    void recycle_block(BufferBlock& block);
    void write_file(const byte* content, int fileIndex, int blockIndex);
    byte* read_file(byte* buffer, int fileIndex, int blockIndex);
    BufferBlock& get_block(int fileIndex, int blockIndex);
    BufferBlock& add_block(int fileIndex, int blockIndex);
    BufferBlock& replace_lru_block(int fileIndex, int blockIndex);
};

class BlockPtr
{
private:
    int _fileIndex;
    int _blockIndex;
public:
    BlockPtr(nullptr_t)
        : BlockPtr()
    {
    }
    BlockPtr()
        : BlockPtr(-1, -1)
    {
    }
    BlockPtr(int fileIndex, int blockIndex)
        : _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
    {
    }
    bool operator==(const BlockPtr& rhs)
    {
        return _fileIndex == rhs._fileIndex && _blockIndex == rhs._blockIndex;
    }
    bool operator!=(const BlockPtr& rhs)
    {
        return !(*this == rhs);
    }
    explicit operator bool()
    {
        return _fileIndex != -1 && _blockIndex != -1;
    }
    BufferBlock& operator*()
    {
        return BufferManager::instance().get_block(_fileIndex, _blockIndex);
    }
    BufferBlock& operator->()
    {
        return BufferManager::instance().get_block(_fileIndex, _blockIndex);
    }
};

class BufferBlock : Uncopyable
{
    friend class BufferManager;
public:
    const static int BlockSize = 4096;
private:
    std::unique_ptr<byte, BufferArrayDeleter> _buffer;
    int _fileIndex;
    int _blockIndex;
    bool _isLocked;
    boost::posix_time::ptime _lastModifiedTime;

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
    {
    }
public:
    ~BufferBlock()
    {
        BufferManager::instance().recycle_block(*this);
    }
    void update_time()
    {
        _lastModifiedTime = boost::posix_time::microsec_clock::universal_time();
    }
    template<typename T>
    T& reinterpret_block(int begin)
    {
        return &reinterpret_cast<T*>(_buffer.get() + begin);
    }
    explicit operator bool()
    {
        return _buffer != nullptr;
    }
    bool is_locked()
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

class InsuffcientSpace : public std::runtime_error
{
public:
    InsuffcientSpace(const char* msg)
        : std::runtime_error(msg)
    {
    }
};