#include "stdafx.h"
#include "BufferManager.h"

BufferManager & BufferManager::instance()
{
    static BufferManager instance;
    return instance;
}

void BufferManager::release_block(BufferBlock& block)
{
    if (block)
    {
        write_file(block._buffer.get(), block._fileIndex, block._blockIndex);
    }
}

void BufferManager::write_file(const byte * content, int fileIndex, int blockIndex)
{
    static char fileName[11];
    FILE* stream = fopen(itoa(fileIndex, fileName), "a+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    fwrite(content, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
}

byte* BufferManager::read_file(byte* buffer, int fileIndex, int blockIndex)
{
    static char fileName[11];
    FILE* stream = fopen(itoa(fileIndex, fileName), "a+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    fread(buffer, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
    return buffer;
}

BufferBlock& BufferManager::find_or_alloc(int fileIndex, int blockIndex)
{
    for (auto& block : _blocks)
    {
        if (block->_fileIndex == fileIndex && block->_blockIndex == blockIndex)
        {
            return *block;
        }
    }
    return alloc_block(fileIndex, blockIndex);
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
