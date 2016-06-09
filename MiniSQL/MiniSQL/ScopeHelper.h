#pragma once

class ScopeExit : Uncopyable
{
private:
    std::function<void()> _function;
public:
    explicit ScopeExit(const std::function<void()>& func)
        : _function(func)
    {
    }

    void operator()() const
    {
        _function();
    }

    ~ScopeExit()
    {
        _function();
    }
};

class ScopeEnter : Uncopyable
{
private:
    std::function<void()> _function;
public:
    explicit ScopeEnter(const std::function<void()>& func)
        : _function(func)
    {
        _function();
    }

    void operator()() const
    {
        _function();
    }
};