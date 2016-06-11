#pragma once

using byte = unsigned char;

template<size_t size>
char* itoa(int num, char(&buf)[size])
{
    assert(size > 0);
    static const char _digits[] = {'9','8','7','6','5','4','3','2','1','0','1','2','3','4','5','6','7','8','9'};
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
    if (lhs.second <= (T)(std::numeric_limits<T>::max() - (T)value))
    {
        lhs.second += value;
    }
    else
    {
        lhs.first += 1;
        lhs.second = (T)value - (T)(std::numeric_limits<T>::max() - lhs.second);
    }
    return lhs;
}

template<typename T, typename U, std::enable_if_t<
    std::is_integral<T>::value &&
    std::is_convertible<U, T>::value>* = nullptr>
    std::pair<T, T> operator+(const std::pair<T, T>& lhs, U value)
{
    std::pair<T, T> result = lhs;
    if (lhs.second <= (T)(std::numeric_limits<T>::max() - (T)value))
    {
        lhs.second += value;
    }
    else
    {
        lhs.first += 1;
        lhs.second = (T)value - (T)(std::numeric_limits<T>::max() - lhs.second);
    }
    return result;
}

template<typename T1, typename T2, typename U, std::enable_if_t<
    std::is_integral<T1>::value &&
    std::is_integral<T2>::value &&
    std::is_convertible<U, T1>::value &&
    std::is_convertible<U, T2>::value>* = nullptr>
std::pair<T1, T2>& operator+=(std::pair<T1, T2>& lhs, U value)
{
    if (lhs.second <= (T2)(std::numeric_limits<T2>::max() - (T2)value))
    {
        lhs.second += value;
    }
    else
    {
        lhs.first += 1;
        lhs.second = (T2)value - (T2)(std::numeric_limits<T2>::max() - lhs.second);
    }
    return lhs;
}


template<typename T1, typename T2, typename U, std::enable_if_t<
    std::is_integral<T1>::value &&
    std::is_integral<T2>::value &&
    std::is_convertible<U, T1>::value &&
    std::is_convertible<U, T2>::value>* = nullptr>
    std::pair<T1, T2> operator+(const std::pair<T1, T2>& lhs, U value)
{
    std::pair<T1, T2> result = lhs;

    if (result.second <= (T2)(std::numeric_limits<T2>::max() - (T2)value))
    {
        result.second += value;
    }
    else
    {
        result.first += 1;
        result.second = (T2)value - (T2)(std::numeric_limits<T2>::max() - result.second);
    }
    return result;
}

/*

template<typename T, size_t Size>
int compare(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    for (size_t i = 0; i != Size; i++)
    {
        if (lhs[i] > rhs[i])
        {
            return 1;
        }
        if (rhs[i] < rhs[i])
        {
            return -1;
        }
    }
    return 0;
}

template<typename T, size_t Size>
bool operator==(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    return compare(lhs, rhs) == 0;
}

template<typename T, size_t Size>
bool operator!=(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    return compare(lhs, rhs) != 0;
}

template<typename T, size_t Size>
bool operator>(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    return compare(lhs, rhs) > 0;
}

template<typename T, size_t Size>
bool operator>=(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    return compare(lhs, rhs) >= 0;
}

template<typename T, size_t Size>
bool operator<(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    return compare(lhs, rhs) < 0;
}

template<typename T, size_t Size>
bool operator<=(const std::array<T, Size>& lhs, const std::array<T, Size>& rhs)
{
    return compare(lhs, rhs) <= 0;
}

*/
