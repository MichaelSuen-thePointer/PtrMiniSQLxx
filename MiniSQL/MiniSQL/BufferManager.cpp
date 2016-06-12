#include "stdafx.h"
#include "BufferManager.h"

const char* const BufferManager::FileName = "BufferManagerMeta";

void BufferManager::save_block(BufferBlock& block)
{
    if (block._hasModified)
    {
        log("BM: block need to be saved");
        write_file(block._buffer.get(), block._fileName, block._fileIndex, block._blockIndex);
    }
    log("BM: done");
}

bool BufferManager::has_block(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
{
    FILE* fp;
    if ((fp = fopen((fileName + std::to_string(fileIndex)).c_str(), "rb")) == nullptr)
    {
        return false;
    }
    fseek(fp, 0, SEEK_END);
    auto size = ftell(fp);
    fclose(fp);
    if (size < blockIndex * BufferBlock::BlockSize)
    {
        return false;
    }
    return true;
}

void BufferManager::write_file(const byte * content, const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
{
    log("BM: write file", fileName, fileIndex, blockIndex);
    FILE* stream;
    stream = fopen((fileName + std::to_string(fileIndex)).c_str(), "rb+");
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);
    auto cur = ftell(stream);
    fwrite(content, 1, BufferBlock::BlockSize, stream);
    auto pos = ftell(stream);
    fclose(stream);
}

byte* BufferManager::read_file(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
{
    log("BM: read file", fileName, fileIndex, blockIndex);
    FILE* stream;
    stream = fopen((fileName + std::to_string(fileIndex)).c_str(), "rb+");
    if (stream == nullptr)
    {
        stream = fopen((fileName + std::to_string(fileIndex)).c_str(), "wb");
        fclose(stream);
        stream = fopen((fileName + std::to_string(fileIndex)).c_str(), "rb+");
    }
    fseek(stream, 0, SEEK_END);
    while (ftell(stream) < (blockIndex + 1) * BufferBlock::BlockSize)
    {
        fwrite(buffer, 1, BufferBlock::BlockSize, stream);
        fseek(stream, 0, SEEK_END);
    }
    fseek(stream, blockIndex * BufferBlock::BlockSize, SEEK_SET);

    fread(buffer, 1, BufferBlock::BlockSize, stream);
    fclose(stream);
    return buffer;
}

void BufferManager::load()
{
    std::ifstream config{FileName};
    if (config.good())
    {
        size_t fileCount;
        config >> fileCount;
        for (size_t i = 0; i != fileCount; i++)
        {
            int label;
            std::string fileName;
            config >> label >> fileName;
            _indexNameMap.insert({label, fileName});
            _nameIndexMap.insert({fileName, label});
        }
        size_t managedFileCount;
        config >> managedFileCount;
        for (size_t i = 0; i != managedFileCount; i++)
        {
            std::string fileName;
            config >> fileName;
            size_t freeIndexCount;
            config >> freeIndexCount;
            for (size_t j = 0; j != freeIndexCount; j++)
            {
                IndexPair pair;
                config >> pair.first >> pair.second;
                _freeIndexPairs[fileName].insert(pair);
            }
        }
    }
    log("BM: loaded");
}

void BufferManager::save()
{
    std::ofstream config{FileName};
    if (config.good())
    {
        config << _indexNameMap.size() << "\n";
        for (auto& pair : _indexNameMap)
        {
            config << pair.first << " " << pair.second << "\n";
        }
    }

    config << _freeIndexPairs.size() << "\n";
    for (const auto& file : _freeIndexPairs)
    {
        config << file.first << " " << file.second.size() << "\n";
        for (const auto& pair : file.second)
        {
            config << pair.first << " " << pair.second << "\n";
        }
    }
    log("BM: saved");
}

BufferBlock& BufferManager::alloc_block(const std::string& fileName)
{
    log("alloc block for", fileName);
    if (_freeIndexPairs[fileName].size() == 0)
    {
        _freeIndexPairs[fileName].insert({0,0});
    }
    auto& managedFileIndices = _freeIndexPairs[fileName];

    auto iterPair = managedFileIndices.begin();
    auto pair = *iterPair;
    managedFileIndices.erase(iterPair);
    if (managedFileIndices.size() == 0)
    {
        managedFileIndices.insert(pair + 1);
    }
    log("choose pair", pair.first, pair.second);
    return find_or_alloc(fileName, pair.first, pair.second);
}

void BufferManager::drop_block(BufferBlock& block)
{
    _freeIndexPairs[block._fileName].insert({block._fileIndex, block._blockIndex});
}

void BufferManager::drop_block(const BlockPtr& block)
{
    _freeIndexPairs[check_file_name(block._fileNameIndex)].insert({block._fileIndex, block._blockIndex});
}


BufferBlock& BufferManager::find_or_alloc(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
{
    log("BM: ask block", fileName, fileIndex, blockIndex);
    auto i = find_block(fileName, fileIndex, blockIndex);
    if (i != -1)
    {
        log("BM: found");
        return *_blocks[i];
    }
    log("BM: not found");
    return alloc_block(fileName, fileIndex, blockIndex);
}

size_t BufferManager::find_block(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
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

BufferBlock& BufferManager::alloc_block(const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
{
    log("BM: alloate block");
    allocate_file_name_index(fileName);

    byte* buffer = new byte[BufferBlock::BlockSize];
    read_file(buffer, fileName, fileIndex, blockIndex);

    for (auto& block : _blocks)
    {
        if (block == nullptr)
        {
            log("BM: place block at block array");
            block.reset(new BufferBlock(buffer, fileName, fileIndex, blockIndex));
            return *block;
        }
    }
    return replace_lru_block(buffer, fileName, fileIndex, blockIndex);
}

BufferBlock& BufferManager::replace_lru_block(byte* buffer, const std::string& fileName, uint32_t fileIndex, uint16_t blockIndex)
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
    log("BM: replace lru block:", lruIndex, _blocks[lruIndex]->_fileName,
        _blocks[lruIndex]->_fileIndex, _blocks[lruIndex]->_blockIndex);
    _blocks[lruIndex].reset(new BufferBlock(buffer, fileName, fileIndex, blockIndex));
    return *_blocks[lruIndex];
}

uint32_t BufferManager::allocate_file_name_index(const std::string& fileName)
{
    int index;
    auto place = _nameIndexMap.find(fileName);
    if (place == _nameIndexMap.end())
    {
        if (_indexNameMap.size())
        {
            auto iter = --_indexNameMap.end();
            index = iter->first + 1;
        }
        else
        {
            index = 0;
        }
        _indexNameMap.insert({index, fileName});
        _nameIndexMap.insert({fileName, index});
    }
    else
    {
        index = place->second;
    }
    log("BM: create token", index, "for file name", fileName);
    return index;
}

const std::string& BufferManager::check_file_name(uint32_t index)
{
    auto place = _indexNameMap.find(index);
    assert(place != _indexNameMap.end());
    return place->second;
}

uint32_t BufferManager::check_file_index(const std::string& file)
{
    auto place = _nameIndexMap.find(file);
    assert(place != _nameIndexMap.end());
    return place->second;
}
