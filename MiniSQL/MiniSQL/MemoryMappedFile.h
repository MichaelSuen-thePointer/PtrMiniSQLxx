#pragma once
#define NOMINMAX
#include <Windows.h>

class MemoryMappedFile
{
private:
    HANDLE _hFile;
    HANDLE _hMappedFile;
    void* _memory;

    void destruct()
    {
        if (_memory != nullptr)
        {
            UnmapViewOfFile(_memory);
            auto e = GetLastError();
            assert(e == 0);
            CloseHandle(_hMappedFile);
            e = GetLastError();
            assert(e == 0);
            CloseHandle(_hFile);
            e = GetLastError();
            assert(e == 0);
            _memory = nullptr;
            _hFile = nullptr;
            _hMappedFile = nullptr;
        }
    }
    
public:
    MemoryMappedFile(const std::string& fileName, size_t fileSize)
    {
        _hFile = CreateFileA(fileName.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        auto e = GetLastError();
        assert(_hFile != INVALID_HANDLE_VALUE);
        _hMappedFile = CreateFileMappingA(_hFile, NULL, PAGE_READWRITE, 0, fileSize, NULL);
        assert(_hMappedFile != INVALID_HANDLE_VALUE);
        _memory = MapViewOfFile(_hMappedFile, FILE_MAP_ALL_ACCESS, 0, 0, fileSize);
        assert(_memory != nullptr);
    }

    MemoryMappedFile(const MemoryMappedFile&) = delete;

    MemoryMappedFile(MemoryMappedFile&& other)
    {
        _hFile = other._hFile;
        _hMappedFile = other._hMappedFile;
        _memory = other._memory;

        other._hFile = NULL;
        other._hMappedFile = NULL;
        other._memory = nullptr;
    }

    MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

    MemoryMappedFile& operator=(MemoryMappedFile&& other)
    {
        destruct();
        _hFile = other._hFile;
        _hMappedFile = other._hMappedFile;
        _memory = other._memory;

        other._hFile = NULL;
        other._hMappedFile = NULL;
        other._memory = nullptr;

        return *this;
    }

    ~MemoryMappedFile()
    {
        destruct();
    }

    void* get() const
    {
        return _memory;
    }

    template<class T>
    T* as()
    {
        return reinterpret_cast<T*>(_memory);
    }
};
