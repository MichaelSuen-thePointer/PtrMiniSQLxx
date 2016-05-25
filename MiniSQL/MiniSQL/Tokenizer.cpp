#include "stdafx.h"
#include "Tokenizer.h"

std::map<std::string, Tokenizer::Kind> Tokenizer::_keywordMap =
{
    { "create", Kind::Create },
    { "delete", Kind::Delete },
    { "values", Kind::Values },
    { "use", Kind::Use },
    { "insert", Kind::Insert },
    { "from", Kind::From },
    { "like", Kind::Like },
    { "select", Kind::Select },
    { "where", Kind::Where },
    { "into", Kind::Into },
    { "exit", Kind::Exit },
    { ",", Kind::Comma },
    { ";", Kind::SemiColon },
    { ".", Kind::Dot },
    { "(", Kind::LBracket },
    { ")", Kind::RBracket },
    { "=", Kind::EQ },
    { "<", Kind::LT },
    { ">", Kind::GT },
};

Tokenizer::Tokenizer(const std::string& str)
{
    reset(str);
}

void Tokenizer::reset(const std::string& str)
{
    _current.clear();
    _content = str;
    _front = 0;
    _end = _content.size();
    _state = None;
    _line = 1;
    _column = 1;
    _tokenLine = 1;
    _tokenColumn = 1;
    _tokens.clear();

    generate_all();
}

void Tokenizer::generate_all()
{
    while (_front != _end)
    {
        char ch = _content[_front];
        switch (_state)
        {
        case None:
        {
            switch (ch)
            {
            case '(': case ')':case ';':case '=':case'.':case',':
            {
                assign_pos();
                push_token(ch);
                break;
            }
            case '<':case '>':
            {
                assign_pos();
                push_char(ch);
                if (ch == '>')
                {
                    _state = InGreaterThanOperator;
                }
                else
                {
                    _state = InLessThanOperator;
                }
                break;
            }
            case '\'':case '\"':
            {
                assign_pos();
                if (ch == '\'')
                {
                    _state = InSingleQuote;
                }
                else
                {
                    _state = InDoubleQuote;
                }
                break;
            }
            case '-':case'0':case'1':case'2':case'3':case'4':
            case'5':case '6':case'7':case'8':case'9':
            {
                push_char(ch);
                assign_pos();
                _state = InInt;
                break;
            }
            default:
            {
                if (isspace(ch))
                {
                    if (ch == '\n')
                    {
                        _line++;
                        _column = 0;
                    }
                }
                else if (ch == '`' || ch == '_' || isalpha(ch))
                {
                    push_char(ch);
                    assign_pos();
                    _state = InWord;
                }
                else
                {
                    assert(0);
                }
                break;
            }
            } //switch ch
            break;
        } //case None
        case InWord:
        {
            if (ch == '_' || isalnum(ch))
            {
                push_char(ch);
            }
            else
            {
                push_token();
                _front -= 1;
                _column -= 1;
                _state = None;
            }
            break;
        } //case InWord
        case InInt:
        {
            switch (ch)
            {
            case'0':case'1':case'2':case'3':
            case'4':case'5':case'7':case'8':case'9':
            {
                push_char(ch);
                break;
            }
            case '.':
            {
                push_char(ch);
                _state = InFloat;
                break;
            }
            default:
            {
                push_token(Kind::Number);
                _front -= 1;
                _column -= 1;
                _state = None;
                break;
            }
            }
            break;
        }
        case InFloat:
        {
            switch (ch)
            {
            case'0':case'1':case'2':case'3':
            case'4':case'5':case'7':case'8':case'9':
            {
                push_char(ch);
                break;
            }
            default:
            {
                push_token(Kind::Number);
                _state = None;
                _front -= 1;
                _column -= 1;
                break;
            }
            }
            break;
        }
        case InLessThanOperator:
        {
            if (ch == '=')
            {
                push_char(ch);
                push_token(Kind::LE);
            }
            else
            {
                push_token(Kind::LT);
                _front -= 1;
                _column -= 1;
            }
            _state = None;
            break;
        } //case InLessThanOperator
        case InGreaterThanOperator:
        {
            if (ch == '=')
            {
                push_char(ch);
                push_token(Kind::GE);
            }
            else
            {
                push_token(Kind::LT);
                _front -= 1;
                _column -= 1;
            }
            _state = None;
            break;
        } //case InGreaterThanOperator
        case InSingleQuote:
        {
            if (ch == '\'')
            {
                push_token(Kind::String);
                _state = None;
            }
            else
            {
                push_char(ch);
                if (ch == '\n')
                {
                    push_char(ch);
                    _line += 1;
                    _column = 0;
                }
            }
            break;
        } //case InSingleQuote:
        case InDoubleQuote:
        {
            if (ch == '\"')
            {
                push_token(Kind::String);
                _state = None;
            }
            else
            {
                push_char(ch);
                if (ch == '\n')
                {
                    _line += 1;
                    _column = 0;
                }
            }
            break;
        } //case InDoubleQuote
        default:
        {
            assert(("unexpected state", 0));
        }
        }
        _column += 1;
        _front += 1;
    }
    if (_current.size())
    {
        switch (_state)
        {
        case InInt:case InFloat:
        {
            push_token(Kind::Number);
            break;
        }
        case InSingleQuote:case InDoubleQuote:
        {
            push_token(Kind::String);
            break;
        }
        default:
        {
            push_token();
            break;
        }
        }
    }
    push_token(Kind::End, '\0');
}