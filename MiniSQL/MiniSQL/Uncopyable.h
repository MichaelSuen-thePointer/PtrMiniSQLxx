#pragma once

class Uncopyable
{
protected:
    constexpr Uncopyable() = default;
    ~Uncopyable() = default;
    Uncopyable(const Uncopyable&) = delete;
    Uncopyable operator=(const Uncopyable&) = delete;
};