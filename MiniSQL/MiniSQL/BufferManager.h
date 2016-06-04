#pragma once

class InvalidIndex : public std::runtime_error
{
public:
    explicit InvalidIndex(const char* msg)
        : runtime_error(msg)
    {
    }
};

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
    std::map<std::string, std::set<IndexPair>> _indices;
    std::map<std::string, std::set<IndexPair>> _freeIndices;
    std::map<int, std::string> _indexNameMap;
    std::map<std::string, int> _nameIndexMap;
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

    BufferBlock& find_or_alloc(const std::string& fileName, int fileIndex, int blockIndex);
    BufferBlock& alloc_block(const std::string& fileName);

    int allocate_file_name_index(const std::string& fileName);
    const std::string& check_file_name(int index);
    int check_file_index(const std::string& file);
    void drop_block(BufferBlock& block);

    bool has_block(const std::string& fileName, int fileIndex, int blockIndex);
private:
    size_t find_block(const std::string& fileName, int fileIndex, int blockIndex);
    void save_block(BufferBlock& block);
    void write_file(const byte* content, const std::string& fileName, int fileIndex, int blockIndex);
    byte* read_file(byte* buffer, const std::string& fileName, int fileIndex, int blockIndex);
    BufferBlock& alloc_block(const std::string& name, int fileIndex, int blockIndex);
    BufferBlock& replace_lru_block(byte* buffer, const std::string& fileName, int fileIndex, int blockIndex);

    IndexPair allocate_index(const std::string& fileName);
    void deallocate_index(const std::string& fileName, int fileIndex, int blockIndex);

};

class BufferBlock : Uncopyable
{
    friend class BufferManager;
    friend class BlockPtr;
public:
    const static int BlockSize = 4096;
private:
    std::unique_ptr<byte, BufferArrayDeleter> _buffer;
    std::string _fileName;
    int _fileIndex;
    int _blockIndex;
    bool _isLocked;
    bool _hasModified;
    boost::posix_time::ptime _lastModifiedTime;
    int _offset;

    BufferBlock()
        : BufferBlock(nullptr, "", -1, -1)
    {
    }
    BufferBlock(byte* buffer, const std::string& fileName, int fileIndex, int blockIndex)
        : _buffer(buffer)
        , _fileName(fileName)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _isLocked(false)
        , _hasModified(false)
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

    void notify_modification()
    {
        _hasModified = true;
        _lastModifiedTime = boost::posix_time::microsec_clock::universal_time();
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
    BlockPtr ptr() const;

    byte* raw_ptr() { return this->as<byte>(); }

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
    int _fileNameIndex;
    int _fileIndex;
    int _blockIndex;
    int _offset;
public:
    BlockPtr(nullptr_t)
        : BlockPtr()
    {
    }
    BlockPtr()
        : BlockPtr(-1, -1, -1)
    {
    }
    BlockPtr(int fileNameIndex, int fileIndex, int blockIndex)
        : BlockPtr(fileNameIndex, fileIndex, blockIndex, 0)
    {
    }
    BlockPtr(int fileNameIndex, int fileIndex, int blockIndex, int offset)
        : _fileNameIndex(fileNameIndex)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _offset(offset)
    {
    }
    BlockPtr& operator=(const BlockPtr& other)
    {
        _fileIndex = other._fileIndex;
        _blockIndex = other._blockIndex;
        _offset = other._offset;
        return *this;
    }
    BlockPtr& operator=(nullptr_t)
    {
        _fileIndex = -1;
        _blockIndex = -1;
        _offset = 0;
        return *this;
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
        auto& block = BufferManager::instance().find_or_alloc(BufferManager::instance().check_file_name(_fileNameIndex), _fileIndex, _blockIndex);
        block._offset = _offset;
        return block;
    }
    const BufferBlock& operator*() const
    {
        return **(const_cast<BlockPtr*>(this));
    }
    BufferBlock* operator->()
    {
        auto& block = BufferManager::instance().find_or_alloc(BufferManager::instance().check_file_name(_fileNameIndex), _fileIndex, _blockIndex);
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

inline BlockPtr BufferBlock::ptr() const
{

    return BlockPtr(_fileIndex, _blockIndex, _offset);
}