#pragma once

class Logger
{
private:
    std::ofstream _stream;
public:
    Logger()
        : _stream("log.dat")
    {
    }
    
    template<class T, class... TArgs>
    Logger& operator()(T&& value, TArgs&&... rest)
    {
        _stream << std::forward<T>(value) << " "; 
        return (*this)(std::forward<TArgs>(rest)...);
    }

    template<class T>
    Logger& operator()(T&& value)
    {
        _stream << std::forward<T>(value) << "\n";
        return *this;
    }
};

#if defined _DEBUG || defined DEBUG

#define log(...) logger(__VA_ARGS__)
#define log_enum(enum_val) (__TEXT(enum_val), enum_val) 
#define log_expr(expr) (__TEXT(expr), expr)

#else 

#define log(...)
#define log_enum(enum_val)
#define log_expr(expr)

#endif 
extern Logger logger;