#pragma once

#include <sstream>

class MemoryReadStream : Uncopyable
{
    byte* _buffer;
    size_t _current;
    size_t _size;
public:
    MemoryReadStream(byte* buffer, size_t size)
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

    template<typename  T, typename = std::enable_if_t<std::is_pod<std::decay_t<T>>::value>>
    MemoryReadStream& operator >> (T& obj)
    {
        assert(remain() >= sizeof(T));

        memcpy(reinterpret_cast<char*>(&obj), _buffer + _current, sizeof(T));

        _current += sizeof(T);

        return *this;
    }

    MemoryReadStream& operator >> (std::string& obj)
    {
        obj.clear();
        while (_buffer[_current] != '\0')
        {
            assert(_current < _size);
            obj.push_back(_buffer[_current]);
            _current++;
        }
        _current++;

        return *this;
    }

    ~MemoryReadStream() {}
};

