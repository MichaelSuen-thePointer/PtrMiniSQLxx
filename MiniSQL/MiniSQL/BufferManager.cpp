#include "stdafx.h"
#include "BufferManager.h"

BufferManager & BufferManager::instance()
{
    static BufferManager instance;
    return instance;
}

void BufferManager::recycle_block(BufferBlock & block)
{
    if (block._hasModified)
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

BufferBlock& BufferManager::get_block(int fileIndex, int blockIndex)
{
    for (auto& block : _blocks)
    {
        if (block->_fileIndex == fileIndex && block->_blockIndex == blockIndex)
        {
            return *block;
        }
    }
    return add_block(fileIndex, blockIndex);
}

BufferBlock& BufferManager::add_block(int fileIndex, int blockIndex)
{
    for (auto& block : _blocks)
    {
        if (block == nullptr)
        {
            byte* buffer = new byte[BufferBlock::BlockSize];
            read_file(buffer, fileIndex, blockIndex);
            block.reset(new BufferBlock(buffer, fileIndex, blockIndex));
            return *block;
        }
    }
    return replace_lru_block(fileIndex, blockIndex);
}

BufferBlock& BufferManager::replace_lru_block(int fileIndex, int blockIndex)
{
    byte* buffer = new byte[BufferBlock::BlockSize];
    read_file(buffer, fileIndex, blockIndex);
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
