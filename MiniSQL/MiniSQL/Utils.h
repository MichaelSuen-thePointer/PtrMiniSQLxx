#pragma once
using byte = unsigned char;

template<size_t size>
char* itoa(int num, char(&buf)[size])
{
    assert(size > 0);
    static const char _digits[] = { '9','8','7','6','5','4','3','2','1','0','1','2','3','4','5','6','7','8','9' };
    static const char* digits = _digits + 9;

    char* str = buf;
    bool neg = num < 0;
    do
    {
        assert(str - buf < size);
        *str++ = digits[num % 10];
        num /= 10;
    } while (num != 0);
    if (neg)
    {
        *str++ = '-';
    }
    *str = '\0';
    std::reverse(buf, str);

    return buf;
}

template<typename T, typename U, std::enable_if_t<
    std::is_integral<T>::value &&
    std::is_convertible<U, T>::value>* = nullptr>
std::pair<T, T>& operator+=(std::pair<T, T>& lhs, U value)
{
    if(lhs.second <= std::numeric_limits<T>::max() - value)
    {
        lhs.second += value;
    }
    else
    {
        lhs.first += 1;
        lhs.second = value - (std::numeric_limits<T>::max() - lhs.second);
    }
    return lhs;
}

template<typename T, typename U, std::enable_if_t<
    std::is_integral<T>::value &&
    std::is_convertible<U, T>::value>* = nullptr>
std::pair<T, T> operator+(const std::pair<T, T>& lhs, U value)
{
    std::pair<T, T> result = lhs;
    if (result.second <= std::numeric_limits<T>::max() - value)
    {
        result.second += value;
    }
    else
    {
        result.first += 1;
        result.second = value - (std::numeric_limits<T>::max() - result.second);
    }
    return result;
}