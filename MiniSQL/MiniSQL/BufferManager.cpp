#include "stdafx.h"
#include "BufferManager.h"
#include <algorithm>

const char* const BufferManager::FileName = "BufferManagerMeta";

bool BufferManager::has_block(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex)
{
    FILE* fp;
    if ((fp = fopen((fileName + "." + std::to_string(fileIndex) + "." + std::to_string(blockIndex)).c_str(), "rb")) == nullptr)
    {
        return false;
    }
    fclose(fp);
    return true;
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

void BufferManager::drop_block(const std::string & name)
{
    _freeIndexPairs.erase(name);
}

BufferBlock& BufferManager::find_or_alloc(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex)
{
    log("BM: ask block", fileName, fileIndex, blockIndex);
    auto iter = find_block(fileName, fileIndex, blockIndex);
    if (iter != _blocks.end())
    {
        log("BM: found");
        if (_blocks.size() > 5 && iter != _blocks.begin())
        {
            log("BM: move block to first place");
            _blocks.splice(_blocks.begin(), _blocks, iter, std::next(iter));
            return _blocks.front();
        }
        return *iter;
    }
    log("BM: not found");
    return alloc_block(fileName, fileIndex, blockIndex);
}

BufferManager::ListIter BufferManager::find_block(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex)
{
    return std::find_if(_blocks.begin(), _blocks.end(), [&](const auto& elm) {
        return elm._fileName == fileName &&
            elm._fileIndex == fileIndex &&
            elm._blockIndex == blockIndex;
    });
}

BufferBlock& BufferManager::alloc_block(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex)
{
    log("BM: alloate block");
    allocate_file_name_index(fileName);

    //byte* buffer = new byte[BufferBlock::BlockSize];
    //read_file(buffer, fileName, fileIndex, blockIndex);

    if (_blocks.size() < BlockCount)
    {
        log("BM: place block at block array");
        _blocks.push_front({fileName, fileIndex, blockIndex});
        return _blocks.front();
    }
    return replace_lru_block(fileName, fileIndex, blockIndex);
}

BufferBlock& BufferManager::replace_lru_block(const std::string& fileName, uint32_t fileIndex, uint32_t blockIndex)
{
    auto lruIter = std::find_if(_blocks.rbegin(), _blocks.rend(), [](const auto& block)
    {
        return !block.is_locked();
    });
    if (lruIter == _blocks.rend())
    {
        throw InsuffcientSpace("Cannot find a block to be replaced.");
    }
    log("BM: replace lru block:", lruIter->_fileName, lruIter->_fileIndex, lruIter->_blockIndex);

    _blocks.erase(--lruIter.base());
    _blocks.push_front({fileName, fileIndex, blockIndex});

    return _blocks.front();
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
