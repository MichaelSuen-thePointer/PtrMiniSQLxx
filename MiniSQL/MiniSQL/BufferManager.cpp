#include "stdafx.h"
#include "BufferManager.h"


void BufferManager::save_block(BufferBlock& block)
{
    if (block)
    {
        write_file(block._buffer.get(), block._fileName, block._fileIndex, block._blockIndex);
    }
}

void BufferManager::drop_block(BufferBlock& block)
{
    deallocate_index(block._fileName, block._fileIndex, block._blockIndex);
    auto i = find_block(block._fileName, block._fileIndex, block._blockIndex);
    assert(i != -1);
    _blocks[i].reset();
}

void BufferManager::write_file(const byte * content, const std::string& fileName, int fileIndex, int blockIndex)
{
    FILE* stream;
    fopen_s(&stream, (fileName + std::to_string(fileIndex)).c_str(), "a+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    fwrite(content, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
}

byte* BufferManager::read_file(byte* buffer, const std::string& fileName, int fileIndex, int blockIndex)
{
    FILE* stream;
    fopen_s(&stream, (fileName + std::to_string(fileIndex)).c_str(), "a+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    fread(buffer, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
    return buffer;
}

BufferBlock& BufferManager::find_or_alloc(const std::string& fileName, int fileIndex, int blockIndex)
{
    auto i = find_block(fileName, fileIndex, blockIndex);
    if (i != -1)
    {
        return *_blocks[i];
    }
    return alloc_block(fileName, fileIndex, blockIndex);
}

BufferBlock& BufferManager::alloc_block(const std::string& fileName)
{
    auto pair = allocate_index(fileName);
    return alloc_block(fileName, pair.first, pair.second);
}

size_t BufferManager::find_block(const std::string& fileName, int fileIndex, int blockIndex)
{
    for (size_t i = 0; i != _blocks.size(); ++i)
    {
        if (_blocks[i] != nullptr &&
            _blocks[i]->_fileName == fileName &&
            _blocks[i]->_fileIndex == fileIndex &&
            _blocks[i]->_blockIndex == blockIndex)
        {
            return i;
        }
    }
    return -1;
}

BufferBlock& BufferManager::alloc_block(const std::string& fileName, int fileIndex, int blockIndex)
{
    byte* buffer = new byte[BufferBlock::BlockSize];
    read_file(buffer, fileName, fileIndex, blockIndex);

    for (auto& block : _blocks)
    {
        if (block == nullptr)
        {
            block.reset(new BufferBlock(buffer, fileName, fileIndex, blockIndex));
            save_block(*block);
            return *block;
        }
    }
    return replace_lru_block(buffer, fileName, fileIndex, blockIndex);
}

BufferBlock& BufferManager::replace_lru_block(byte* buffer, const std::string& fileName, int fileIndex, int blockIndex)
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
    _blocks[lruIndex].reset(new BufferBlock(buffer, fileName, fileIndex, blockIndex));
    return *_blocks[lruIndex];
}

BufferManager::IndexPair BufferManager::allocate_index(const std::string& fileName)
{
    auto iterFreeIndices = _freeIndices.find(fileName);
    if (iterFreeIndices != _freeIndices.end())
    {
        auto& freeIndices = iterFreeIndices->second;
        if (freeIndices.begin() != freeIndices.end())
        {
            auto pair = freeIndices.begin();
            freeIndices.erase(freeIndices.begin());
            return *pair;
        }
    }

    auto& indices = _indices[fileName];
    auto maxPlace = indices.rbegin();

    IndexPair newPair = *maxPlace;
    if (maxPlace != indices.rend())
    {
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
    }
    else
    {
        newPair.first = 0;
        newPair.second = 0;
    }
    indices.insert(newPair);
    return newPair;
}

void BufferManager::deallocate_index(const std::string& fileName, int fileIndex, int blockIndex)
{
    _freeIndices[fileName].insert(IndexPair(fileIndex, blockIndex));
}

int BufferManager::allocate_file_name_index(const std::string& fileName)
{
    int index;
    auto place = _nameIndexMap.find(fileName);
    if (place == _nameIndexMap.end())
    {
        if (_indexNameMap.size())
        {
            auto iter = _indexNameMap.end();
            --iter;
            index = iter->first + 1;
        }
        else
        {
            index = 0;
        }
        _indexNameMap.insert({ index, fileName });
        _nameIndexMap.insert({ fileName, index });
    }
    else
    {
        index = place->second;
    }
    return index;
}

const std::string& BufferManager::check_file_name(int index)
{
    auto place = _indexNameMap.find(index);
    assert(place != _indexNameMap.end());
    return place->second;
}

int BufferManager::check_file_index(const std::string& file)
{
    auto place = _nameIndexMap.find(file);
    assert(place != _nameIndexMap.end());
    return place->second;
}
