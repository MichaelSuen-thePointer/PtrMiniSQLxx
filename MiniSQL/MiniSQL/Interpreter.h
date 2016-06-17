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

    static void main_loop(std::istream& is)
    {
        std::string command;
        std::cout << "> ";
        while (is.peek() != -1)
        {
            command.push_back(is.get());
            if (command.back() == ';')
            {
                try
                {
                    _tokenizer.reset(command);
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
                catch (LexicalError e)
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
                catch (std::exception e)
                {
                    std::cout << e.what() << "\n";
                }
                command.clear();
                std::cout << "> ";
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
            create();
            break;
        }
        case Kind::Delete:
        {
            delete_entry();
            break;
        }
        case Kind::Drop:
        {
            drop();
            break;
        }
        case Kind::Insert:
        {
            insert();
            break;
        }
        case Kind::Select:
        {
            select();
            break;
        }
        case Kind::Desc:
        {
            desc_table();
            break;
        }
        case Kind::Show:
        {
            show_table();
            break;
        }
        case Kind::Exec:
        {
            exec();
            break;
        }
        case Kind::Exit:
            return false;
            break;
        default:
            throw SyntaxError("Syntax error: invalid instruction");
            break;
        }
        std::cout << "done.\n";
        return true;
    }

#define EXPECT(type, msg) check_assert(_tokenizer.get(), type, msg)
#define ASSERT(token, type, msg) check_assert(token, type, msg)

    static void create();

    static void drop();

    static void create_index();

    static void create_table();

    static void select();

    static void insert();

    static void delete_entry();

    static void show_select_result(const SelectStatementBuilder& builder);

    static void drop_table();

    static void drop_index();

    static void exec();

    static void desc_table()
    {
        auto tokTableName = _tokenizer.get();
        ASSERT(tokTableName, Kind::Identifier, "table name");
        EXPECT(Kind::SemiColon, "';'");

        auto& table = CatalogManager::instance().find_table(tokTableName.content);

        std::cout << table.name() << std::endl;

        for (auto& field : table.fields())
        {
            std::cout << field.name() << " : " << field.type_info().name() << (field.is_unique() ? " unique" : "") << std::endl;
        }

        if (table.primary_pos() != -1)
        {
            std::cout << "primary key: " << table.fields()[table.primary_pos()].name() << std::endl;
        }

    }

    static void show_table()
    {
        EXPECT(Kind::Tables, "keyword 'tables'");
        EXPECT(Kind::SemiColon, "';'");
        auto& cm = CatalogManager::instance();
        for (auto& tables : cm.tables())
        {
            std::cout << tables.name() << "\n";
        }
    }

    static Type to_type(const Token& token)
    {
        switch (token.kind)
        {
        case Kind::Integer:
            return Int;
        case Kind::Single:
            return Float;
        case Kind::String:
            return Chars;
        default:
            throw SyntaxError("expect a value");
        }
    }

    static Comparison::ComparisonType to_comparison_type(const Token& token)
    {
        switch (token.kind)
        {
        case Kind::NE:
            return Comparison::ComparisonType::Ne;
        case Kind::LT:
            return Comparison::ComparisonType::Lt;
        case Kind::GT:
            return Comparison::ComparisonType::Gt;
        case Kind::LE:
            return Comparison::ComparisonType::Le;
        case Kind::GE:
            return Comparison::ComparisonType::Ge;
        case Kind::EQ:
            return Comparison::ComparisonType::Eq;
        default:
            throw SyntaxError("expect a comparison operator");
        }
    }

    static bool is_value(const Token& token)
    {
        switch (token.kind)
        {
        case Kind::Integer:
        case Kind::Single:
        case Kind::String:
        {
            return true;
            break;
        }
        default:
        {
            return false;
        }
        }
    }

    static void check_value(const Token& token)
    {
        if (!is_value(token))
        {
            throw SyntaxError("expect an error", token.line, token.column);
        }
    }

    static bool is_operator(const Token& token)
    {
        switch (token.kind)
        {
        case Kind::LT:
        case Kind::GT:
        case Kind::LE:
        case Kind::GE:
        case Kind::EQ:
        case Kind::NE:
        {
            return true;
        }
        default:
        {
            return false;
        }
        }
    }

    static void check_operator(const Token& token)
    {
        if (!is_operator(token))
        {
            throw SyntaxError("expected an operator", token.line, token.column);
        }
    }

    static void check_assert(const Tokenizer::Token& token, Kind kind, const std::string& str)
    {
        if (token.kind != kind)
        {
            throw SyntaxError(("Syntax error: expected " + str + " but received " + token.content).c_str(), token.line, token.column);
        }
    }
};
