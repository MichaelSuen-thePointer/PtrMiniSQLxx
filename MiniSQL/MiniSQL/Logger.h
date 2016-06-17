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



#define log(...)
#define log_enum(enum_val)
#define log_expr(expr)

extern Logger logger;