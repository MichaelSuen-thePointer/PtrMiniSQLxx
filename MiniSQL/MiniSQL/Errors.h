#pragma once

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
        : std::runtime_error(msg)
    {
    }
};

class InvalidType : public std::runtime_error
{
public:
    explicit InvalidType(const char* msg)
        : std::runtime_error(msg)
    {
    }
};

class InvalidComparisonType : public std::runtime_error
{
public:
    InvalidComparisonType(const char* str)
        : runtime_error(str)
    {
    }
};


class InvalidIndex : public std::runtime_error
{
public:
    explicit InvalidIndex(const char* msg)
        : runtime_error(msg)
    {
    }
};

class InsuffcientSpace : public std::runtime_error
{
public:
    explicit InsuffcientSpace(const char* msg)
        : std::runtime_error(msg)
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
        : runtime_error(msg)
    {
    }
};

class TableExist : public std::runtime_error
{
public:
    explicit TableExist(const char* msg)
        : runtime_error(msg)
    {
    }
};

class InvalidField : public std::runtime_error
{
public:
    explicit InvalidField(const char* msg)
        : runtime_error(msg)
    {
    }
};

