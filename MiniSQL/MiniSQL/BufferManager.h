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

    using file_name_index_t = uint32_t;
    using file_index_t = uint32_t;
    using block_index_t = uint16_t;
    using offset_t = uint16_t;
private:
    using IndexPair = std::pair<uint32_t, uint16_t>;
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

    static BufferManager& instance()
    {
        static BufferManager instance;
        return instance;
    }

    BufferBlock& find_or_alloc(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex);

    uint32_t allocate_file_name_index(const std::string& fileName);
    const std::string& check_file_name(uint32_t index);
    uint32_t check_file_index(const std::string& file);

    BufferBlock& alloc_block(const std::string& fileName);
    void drop_block(BufferBlock& block);
    void drop_block(const BlockPtr& block);
    void drop_block(const std::string& name);

    bool has_block(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex);
private:
    ListIter find_block(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex);
    void save_block(BufferBlock& block);
    void write_file(const byte* content, const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex);
    byte* read_file(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex);
    BufferBlock& alloc_block(const std::string& name, uint32_t fileIndex, uint16_t blockIndex);
    BufferBlock& replace_lru_block(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex);

};

class BufferBlock : Uncopyable
{
    friend class BufferManager;
    friend class BlockPtr;
public:
    const static int BlockSize = 4096;
private:
    std::unique_ptr<byte, ArrayDeleter> _buffer;
    std::string _fileName;
    uint32_t _fileIndex;
    uint16_t _blockIndex;
    int _lockTimes;
    bool _hasModified;
    mutable boost::posix_time::ptime _lastModifiedTime;
    uint16_t _offset;

    BufferBlock()
        : BufferBlock(nullptr, "", -1, -1)
    {
    }
    BufferBlock(byte* buffer, const std::string& fileName, int fileIndex, int blockIndex)
        : _buffer(buffer)
        , _fileName(fileName)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _lockTimes(0)
        , _hasModified(false)
        , _lastModifiedTime(boost::posix_time::microsec_clock::universal_time())
        , _offset(0)
    {
        log("BB: ctor", fileName, fileIndex, blockIndex);
    }
    void release()
    {
        BufferManager::instance().save_block(*this);
    }
public:
    ~BufferBlock()
    {
        log("BB: block dtor", _fileName, _fileIndex, _blockIndex);
        release();
    }

    void notify_modification()
    {
        log("BB: get dirty");
        _hasModified = true;
        update_time();
    }

    void update_time() const
    {
        _lastModifiedTime = boost::posix_time::microsec_clock::universal_time();
    }

    void reset()
    {
        release();
        _buffer.reset();
        _fileIndex = -1;
        _blockIndex = -1;
        _lockTimes = 0;
        _lastModifiedTime = boost::posix_time::special_values::not_a_date_time;
    }

    template<typename T>
    T* as()
    {
        update_time();
        log("BB: content asked", _fileName, _fileIndex, _blockIndex);
        int tempOffset = _offset;
        _offset = 0;
        return reinterpret_cast<T*>(_buffer.get() + tempOffset);
    }
    BlockPtr ptr() const;

    byte* raw_ptr() { return this->as<byte>(); }

    explicit operator bool() const
    {
        return _buffer != nullptr;
    }
    bool is_locked() const
    {
        return _lockTimes > 0;
    }
    void lock()
    {
        log("BB: lock", _fileName, _fileIndex, _blockIndex);
        _lockTimes++;
    }
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
    uint16_t _blockIndex;
    uint16_t _offset;
public:
    BlockPtr(nullptr_t)
        : BlockPtr()
    {
    }
    BlockPtr()
        : BlockPtr(-1, -1, -1, -1)
    {
    }
    BlockPtr(uint32_t fileNameIndex, uint32_t fileIndex, uint16_t blockIndex)
        : BlockPtr(fileNameIndex, fileIndex, blockIndex, 0)
    {
    }
    BlockPtr(uint32_t fileNameIndex, uint32_t fileIndex, uint16_t blockIndex, uint16_t offset)
        : _fileNameIndex(fileNameIndex)
        , _fileIndex(fileIndex)
        , _blockIndex(blockIndex)
        , _offset(offset)
    {
        log("BP: ctor", _fileNameIndex, _fileIndex, _blockIndex, _offset);
    }
    ~BlockPtr()
    {
        if (_fileNameIndex != -1)
        {
            assert(BufferManager::instance().find_or_alloc(BufferManager::instance().check_file_name(_fileNameIndex), _fileIndex, _blockIndex)._offset == 0);
            log("BP: dtor", _fileNameIndex, _fileIndex, _blockIndex, _offset);
        }
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
    explicit operator bool() const
    {
        return *this != nullptr;
    }
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
};

inline BlockPtr BufferBlock::ptr() const
{
    update_time();
    return BlockPtr(BufferManager::instance().check_file_index(_fileName), _fileIndex, _blockIndex, _offset);
}