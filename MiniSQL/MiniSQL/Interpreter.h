#pragma once
#include <string>
#include <iostream>
#include "Tokenizer.h"
#include "API.h"


class Interpreter
{
private:
    static Tokenizer _tokenizer;
public:
    using Kind = Tokenizer::Kind;
    using Token = Tokenizer::Token;

    static void main_loop()
    {
        std::string command;
        while (std::cin.peek() != -1)
        {
            command.push_back(std::cin.get());
            if (command.back() == ';')
            {
                _tokenizer.reset(command);
                try
                {
                    if (!execute_command())
                    {
                        return;
                    }
                }
                catch (SyntaxError e)
                {
                    std::cout << e.what();
                    if (e.line != -1)
                    {
                        std::cout << "\n\tat line: " << e.line << " column: " << e.column << "\n";
                    }
                    else
                    {
                        std::cout << "\n";
                    }
                }
                catch(SQLError e)
                {
                    std::cout << e.what();
                }
                catch(...)
                {
                    std::cout << "FATAL ERROR: UNEXPECTED ERROR";
                }
                command.clear();
            }
        }
    }

    static bool execute_command()
    {
        Tokenizer::Token token = _tokenizer.get();
        switch (token.kind)
        {
        case Kind::Create:
        {
            create_table();
            break;
        }
        case Kind::Delete: break;
        case Kind::Use: break;
        case Kind::Insert: break;
        case Kind::From: break;
        case Kind::Like: break;
        case Kind::Select: break;
        case Kind::Where: break;
        case Kind::Into: break;
        case Kind::Exit:
            return false;
            break;
        default:
            throw SyntaxError("Syntax error: invalid instruction");
            break;
        }
        return true;
    }

    static void create_table()
    {
        auto token = _tokenizer.get();
        check_assert(token, Kind::Table, "keyword 'table'");
        token = _tokenizer.get();
        check_assert(token, Kind::Identifier, "identifier");
        TableCreater creater(token.content);
        check_assert(_tokenizer.get(), Kind::LBracket, "'('");
        while (_tokenizer.peek().kind != Kind::SemiColon &&
               _tokenizer.peek().kind != Kind::End)
        {
            auto fieldName = _tokenizer.get();
            check_assert(token, Kind::Identifier, "identifier");
            auto typeTok = _tokenizer.get();
            Type type;
            size_t size = 1;
            switch (typeTok.kind) //type
            {
            case Kind::Int:
                type = Int;
                size = 4;
                break;
            case Kind::Float:
                type = Float;
                size = 4;
                break;
            case Kind::Char:
                type = Chars;
                if (_tokenizer.peek().kind == Kind::LBracket)
                {
                    _tokenizer.get();

                    auto opSize = _tokenizer.get();
                    int uncheckedSize;
                    check_assert(opSize, Kind::Integer, "number");
                    std::istringstream(opSize.content) >> uncheckedSize;
                    if (size <= 0 || size >= 256)
                    {
                        throw SyntaxError("Syntax error: expected a integer range from 1 to 255", opSize.line, opSize.column);
                    }
                    size = static_cast<size_t>(uncheckedSize);
                    check_assert(_tokenizer.get(), Kind::RBracket, "')'");
                }
                break;
            default:
                throw SyntaxError("Syntax error: expected a type", typeTok.line, typeTok.column);
            }
            bool isUnique = false;
            if (_tokenizer.peek().kind == Kind::Unique) //unique
            {
                _tokenizer.get();
                isUnique = true;
            }
            switch (_tokenizer.peek().kind) //, or )
            {
            case Kind::Comma:
                _tokenizer.get();
                //fall through
            case Kind::RBracket:
                if (type == Chars)
                {
                    creater.add_field(fieldName.content, TypeInfo{type, size}, isUnique);
                }
                else
                {
                    creater.add_field(fieldName.content, TypeInfo{type}, isUnique);
                }
                break;
            default:
                throw SyntaxError("Syntax error: unexpected content", _tokenizer.peek().line, _tokenizer.peek().column);
            }
            if (_tokenizer.peek().kind == Kind::Primary)
            {
                _tokenizer.get();
                check_assert(_tokenizer.get(), Kind::Key, "keyword 'key'");
                check_assert(_tokenizer.get(), Kind::LBracket, "'('");
                auto primKeyTok = _tokenizer.get();
                if (creater.locate_field(primKeyTok.content) == -1)
                {
                    throw SyntaxError("Syntax error: invalid primary key", primKeyTok.line, primKeyTok.column);
                }
                creater.set_primary(primKeyTok.content);
                check_assert(_tokenizer.get(), Kind::RBracket, "')'");
            }
            if(_tokenizer.peek().kind == Kind::RBracket)
            {
                _tokenizer.get();
                break;
            }
        }
        if (_tokenizer.get().kind != Kind::SemiColon)
        {
            throw SyntaxError("Syntax error: invalid instruction");
        }

        CatalogManager::instance().add_table(creater.create());
    }

    static void check_assert(const Tokenizer::Token& token, Kind kind, const std::string& str)
    {
        if (token.kind != kind)
        {
            throw SyntaxError(("Syntax error: expected " + str + " but received " + token.content).c_str(), token.line, token.column);
        }
    }
};
