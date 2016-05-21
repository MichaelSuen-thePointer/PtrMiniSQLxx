#pragma once

class SyntaxError : public std::runtime_error
{
public:
    int line, column;
    explicit SyntaxError(const char* msg, int line, int column)
        : std::runtime_error(msg)
        , line(line)
        , column(column)
    {
    }
};

class Tokenizer
{
public:
    struct Token
    {
        int line, column;
        std::string content;
    };

    Tokenizer();

    void reset(const std::string& str = std::string());

private:
    enum State
    {
        None,
        InNonSpace,
        InSingleQuote,
        InDoubleQuote
    };
    State _state;
    int _line, _column;
    int _tokenLine, _tokenColumn;
    std::string _current;
    std::string::const_iterator _front;
    std::string::const_iterator _end;
    std::vector<Token> _tokens;
    
    void generate_all()
    {
        while (_front != _end)
        {
            char ch = *_front;
            switch (_state)
            {
            case None:
            {
                if (isspace(ch))
                {
                    if (ch == '\n')
                    {
                        _column++;
                        _line = 0;
                    }
                }
                else if (ch != '\'' && ch != '\"')
                {
                    _tokenLine = _line;
                    _tokenColumn = _column;
                    _state = InNonSpace;
                }
                else
                {
                    _tokenLine = _line;
                    _tokenColumn = _column;
                    if (ch == '\'')
                    {
                        _state = InSingleQuote;
                    }
                    else
                    {
                        _state = InDoubleQuote;
                    }
                }
                break;
            } //None
            case InNonSpace:
            {
                if (isspace(ch))
                {
                    _tokens.push_back(Token{ _tokenLine, _tokenColumn, _current });
                    if (ch == '\n')
                    {
                        _column++;
                        _line = 0;
                    }
                }
                else if (ch != '\'' && ch != '\"')
                {
                    _current.push_back(ch);
                }
                else
                {
                    _tokenLine = _line;
                    _tokenColumn = _column;
                    if (ch == '\'')
                    {
                        _state = InSingleQuote;
                    }
                    else
                    {
                        _state = InDoubleQuote;
                    }
                }
                break;
            } //InNonSpace
            case InSingleQuote: case InDoubleQuote:
            {
                if (ch == '\"' && _state == InDoubleQuote)
                {
                    _state = None;
                    _tokens.push_back(Token{ _line, _column, _current });
                }
                else if (ch == '\'' && _state == InSingleQuote)
                {
                    _state = None;
                    _tokens.push_back(Token{ _line, _column, _current });
                }
                else
                {
                    if (ch == '\n')
                    {
                        _line++;
                        _column = 0;
                    }
                    _current.push_back(ch);
                }
                break;
            } //InSingleQuote InDoubleQuote
            } //switch
            _column++;
            ++_front;
        } //while
        if (_state != None)
        {
            throw SyntaxError("String not closed.", _tokenLine, _tokenColumn);
        }
        if(_current.size())
        {
            _tokens.push_back(Token{ _line, _column, _current });
        }
    }

};

