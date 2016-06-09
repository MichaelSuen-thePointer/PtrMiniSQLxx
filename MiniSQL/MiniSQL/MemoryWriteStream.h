#pragma once
#include <sstream>
class MemoryWriteStream : Uncopyable
{
    byte* _buffer;
    size_t _current;
    size_t _size;
public:
    MemoryWriteStream(byte* buffer, size_t size)
        : _buffer{buffer}
        , _current{}
        , _size{size}
    {
    }

    void seek_cur(int offset)
    {
        assert(_current + offset >= 0 && _current + offset < _size);
        _current += offset;
    }

    void seek_to_begin()
    {
        _current = 0;
    }

    size_t tell() const
    {
        return _current;
    }

    size_t size() const
    {
        return _size;
    }

    size_t remain() const
    {
        return _size - _current;
    }

    MemoryWriteStream& operator<<(const std::string& value)
    {
        assert(value.size() + 1 <= remain());

        memcpy(_buffer + _current, value.c_str(), value.size());

        _current += value.size() + 1;
        _buffer[_current - 1] = '\0';

        return *this;
    }

    template<typename T, typename = std::enable_if_t<std::is_pod<std::decay_t<T>>::value>>
    MemoryWriteStream& operator<<(const T& value)
    {
        assert(sizeof(T) <= remain());
    
        memcpy(_buffer + _current, reinterpret_cast<const char*>(&value), sizeof(T));

        _current += sizeof(T);

        return *this;
    }

    ~MemoryWriteStream() {}
};

