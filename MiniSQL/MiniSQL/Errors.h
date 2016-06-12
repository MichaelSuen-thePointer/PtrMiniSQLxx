#pragma once

using namespace std::string_literals;

class SQLError : public std::runtime_error
{
public:
    explicit SQLError(const char* msg)
        : runtime_error(msg)
    {
    }
};

class InvalidKey : public std::runtime_error
{
public:
    explicit InvalidKey(const char* msg)
        : std::runtime_error(("invalid key: " + std::string(msg)).c_str())
    {
    }
};

class InvalidType : public std::runtime_error
{
public:
    explicit InvalidType(const char* msg)
        : std::runtime_error(("invalid type: " + std::string(msg)).c_str())
    {
    }
};

class InvalidComparisonType : public std::runtime_error
{
public:
    InvalidComparisonType(const char* msg)
        : runtime_error(("invalid comparison type: " + std::string(msg)).c_str())
    {
    }
};


class InvalidIndex : public std::runtime_error
{
public:
    explicit InvalidIndex(const char* msg)
        : runtime_error(("invalid index: " + std::string(msg)).c_str())
    {
    }
};

class InsuffcientSpace : public std::runtime_error
{
public:
    explicit InsuffcientSpace(const char* msg)
        : std::runtime_error(("not enough space: " + std::string(msg)).c_str())
    {
    }
};

class SyntaxError : public std::runtime_error
{
public:
    int line, column;
    SyntaxError(const char* str, int line = -1, int column = -1)
        : runtime_error(str), line(line), column(column)
    {
    }
};

class LexicalError : public std::runtime_error
{
public:
    int line, column;
    explicit LexicalError(const char* msg, int line, int column)
        : std::runtime_error(msg)
        , line(line)
        , column(column)
    {
    }
};

class TableNotExist : public std::runtime_error
{
public:
    explicit TableNotExist(const char* msg)
        : runtime_error(("table not exist: " + std::string(msg)).c_str())
    {
    }
};

class TableExist : public std::runtime_error
{
public:
    explicit TableExist(const char* msg)
        : runtime_error(("table already exist: " + std::string(msg)).c_str())
    {
    }
};

class InvalidField : public std::runtime_error
{
public:
    explicit InvalidField(const char* msg)
        : runtime_error(("invalid field: " + std::string(msg)).c_str())
    {
    }
};

