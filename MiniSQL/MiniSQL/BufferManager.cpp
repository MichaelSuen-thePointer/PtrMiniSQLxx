#include "stdafx.h"
#include "BufferManager.h"


void BufferManager::save_block(BufferBlock& block)
{
    if (block)
    {
        write_file(block._buffer.get(), block._fileIndex, block._blockIndex);
    }
}

void BufferManager::drop_block(BufferBlock& block)
{
    deallocate_index(block._fileIndex, block._blockIndex);
    auto i = find_block(block._fileIndex, block._blockIndex);
    assert(i != -1);
    _blocks[i].reset();
}

void BufferManager::write_file(const byte * content, int fileIndex, int blockIndex)
{
    static char fileName[11];
    FILE* stream;
    fopen_s(&stream, itoa(fileIndex, fileName), "a+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    fwrite(content, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
}

byte* BufferManager::read_file(byte* buffer, int fileIndex, int blockIndex)
{
    static char fileName[11];
    FILE* stream;
    fopen_s(&stream, itoa(fileIndex, fileName), "a+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    fread(buffer, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
    return buffer;
}

BufferBlock& BufferManager::find_or_alloc(int fileIndex, int blockIndex)
{
    auto i = find_block(fileIndex, blockIndex);
    if(i != -1)
    {
        return *_blocks[i];
    }
    return alloc_block(fileIndex, blockIndex);
}

BufferBlock& BufferManager::alloc_block()
{
    auto pair = allocate_index();
    return alloc_block(pair.first, pair.second);
}

size_t BufferManager::find_block(int fileIndex, int blockIndex)
{
    for (size_t i = 0; i != _blocks.size(); ++i)
    {
        if (_blocks[i] != nullptr && 
            _blocks[i]->_fileIndex == fileIndex &&
            _blocks[i]->_blockIndex == blockIndex)
        {
            return i;
        }
    }
    return -1;
}

BufferBlock& BufferManager::alloc_block(int fileIndex, int blockIndex)
{
    byte* buffer = new byte[BufferBlock::BlockSize];
    read_file(buffer, fileIndex, blockIndex);

    for (auto& block : _blocks)
    {
        if (block == nullptr)
        {
            block.reset(new BufferBlock(buffer, fileIndex, blockIndex));
            save_block(*block);
            return *block;
        }
    }
    return replace_lru_block(buffer, fileIndex, blockIndex);
}

BufferBlock& BufferManager::replace_lru_block(byte* buffer, int fileIndex, int blockIndex)
{
    int lruIndex = -1;
    for (size_t i = 0; i != _blocks.size(); i++)
    {
        if (_blocks[i] != nullptr && !_blocks[i]->_isLocked)
        {
            if (lruIndex != -1)
            {
                if (_blocks[lruIndex]->_lastModifiedTime < _blocks[i]->_lastModifiedTime)
                {
                    lruIndex = i;
                }
            }
            else
            {
                lruIndex = i;
            }
        }
    }
    if (lruIndex == -1)
    {
        throw InsuffcientSpace("Cannot find a block to be replaced.");
    }
    _blocks[lruIndex].reset(new BufferBlock(buffer, fileIndex, blockIndex));
    return *_blocks[lruIndex];
}

BufferManager::IndexPair BufferManager::allocate_index()
{
    if (_freeIndices.begin() != _freeIndices.end())
    {
        auto pair = *_freeIndices.begin();
        _freeIndices.erase(_freeIndices.begin());
        return pair;
    }
    auto maxPlace = _indices.end();
    --maxPlace;
    IndexPair newPair = *maxPlace;
    if (newPair.second == std::numeric_limits<int>::max())
    {
        if (newPair.first == std::numeric_limits<int>::max())
        {
            throw InsuffcientSpace("Not enough internal label to allocate.");
        }
        newPair.first++;
        newPair.second = 0;
    }
    else
    {
        newPair.second++;
    }
    _indices.insert(newPair);
    return newPair;
}

void BufferManager::deallocate_index(int fileIndex, int blockIndex)
{
    _freeIndices.insert(IndexPair(fileIndex, blockIndex));
}
