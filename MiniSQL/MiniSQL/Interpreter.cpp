#include "stdafx.h"

#include "Interpreter.h"

Tokenizer Interpreter::_tokenizer;

void Interpreter::create_table()
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
        if (_tokenizer.peek().kind == Kind::RBracket)
        {
            _tokenizer.get();
            break;
        }
    }
    auto tokSemi = _tokenizer.get();
    if (tokSemi.kind != Kind::SemiColon)
    {
        throw SyntaxError("expect ';'", tokSemi.line, tokSemi.column);
    }

    CatalogManager::instance().add_table(creater.create());
    RecordManager::instance().create_table(creater.table_name(), creater.size());
}

void Interpreter::select()
{
    SelectStatementBuilder builder;
    for (;;)
    {
        auto tokFieldName = _tokenizer.get();
        check_assert(tokFieldName, Kind::Identifier, "identifier");
        if (tokFieldName.content != "*")
        {
            builder.add_select_field(tokFieldName.content);
        }
        else
        {
            break;
        }
        auto tokCommaOrFrom = _tokenizer.peek();
        if (tokCommaOrFrom.kind == Kind::From)
        {
            break;
        }
        else if (tokCommaOrFrom.kind == Kind::Comma)
        {
            _tokenizer.get();
            continue;
        }
        else
        {
            throw SyntaxError("expect ',' or end of field list", tokCommaOrFrom.line, tokCommaOrFrom.column);
        }
    }
    EXPECT(Kind::From, "keyword 'from'");
    auto tokTableName = _tokenizer.get();
    check_assert(tokTableName, Kind::Identifier, "table name");
    builder.set_table(tokTableName.content);

    auto tokMaybeWhere = _tokenizer.peek();
    if (tokMaybeWhere.kind == Kind::Where)
    {
        _tokenizer.get();
        for (;;)
        {
            auto tokLhs = _tokenizer.get();
            auto tokOperator = _tokenizer.get();
            auto tokRhs = _tokenizer.get();
            check_operator(tokOperator);
            if (tokLhs.kind == Kind::Identifier)
            {
                check_value(tokRhs);

                builder.add_condition(to_comparison_type(tokOperator), tokLhs.content, tokRhs.content);
            }
            auto tokAndOrSemi = _tokenizer.get();
            if (tokAndOrSemi.kind == Kind::And)
            {
                continue;
            }
            else if (tokAndOrSemi.kind == Kind::SemiColon)
            {
                break;
            }
            else
            {
                throw SyntaxError("expected 'and' or ';'", tokAndOrSemi.line, tokAndOrSemi.column);
            }
        }
    }
    else
    {
        EXPECT(Kind::SemiColon, "';'");
    }
    show_select_result(builder);
}


void Interpreter::insert()
{
    EXPECT(Kind::Into, "keyword 'into'");
    auto tokTableName = _tokenizer.get();
    ASSERT(tokTableName, Kind::Identifier, "table name");
    TupleBuilder builder(tokTableName.content);
    EXPECT(Kind::Values, "keyword 'values'");
    EXPECT(Kind::LBracket, "'('");
    for (;;)
    {
        auto tokValue = _tokenizer.get();
        check_value(tokValue);
        builder.set_value(tokValue.content, to_type(tokValue));
        auto tokCommaOrRBracket = _tokenizer.get();
        if (tokCommaOrRBracket.kind == Kind::Comma)
        {
            continue;
        }
        else if (tokCommaOrRBracket.kind == Kind::RBracket)
        {
            break;
        }
        else
        {
            throw SyntaxError("expect ',' or ')'");
        }
    }
    EXPECT(Kind::SemiColon, "';'");

    UniqueChecker checker{builder.table_name(), builder.get_field()};
    checker.linearly_check();

    RecordManager::instance().insert_entry(builder.table_name(), builder.get_field());
}

void Interpreter::delete_entry()
{
    DeleteStatementBuilder builder;
    EXPECT(Kind::From, "keyword 'from'");
    auto tokTableName = _tokenizer.get();
    check_assert(tokTableName, Kind::Identifier, "table name");
    builder.set_table(tokTableName.content);

    auto tokMaybeWhere = _tokenizer.peek();
    if (tokMaybeWhere.kind == Kind::Where)
    {
        _tokenizer.get();
        for (;;)
        {
            auto tokLhs = _tokenizer.get();
            auto tokOperator = _tokenizer.get();
            auto tokRhs = _tokenizer.get();
            check_operator(tokOperator);
            if (tokLhs.kind == Kind::Identifier)
            {
                check_value(tokRhs);

                builder.add_condition(to_comparison_type(tokOperator), tokLhs.content, tokRhs.content);
            }
            auto tokAndOrSemi = _tokenizer.get();
            if (tokAndOrSemi.kind == Kind::And)
            {
                continue;
            }
            else if (tokAndOrSemi.kind == Kind::SemiColon)
            {
                break;
            }
            else
            {
                throw SyntaxError("expected 'and' or ';'", tokAndOrSemi.line, tokAndOrSemi.column);
            }
        }
    }
    else
    {
        EXPECT(Kind::SemiColon, "';'");
    }
    builder.get_result();
}

void Interpreter::show_select_result(const SelectStatementBuilder& builder)
{
    auto result = builder.get_result();
    size_t totalWidth = 0;
    std::cout << "|";
    for (size_t i = 0; i != result->fields().size(); i++)
    {
        std::cout << std::setw(result->field_width()[i]) << result->fields()[i] << '|';
        totalWidth += result->field_width()[i];
    }
    totalWidth += result->fields().size() + 1;
    std::cout << "\n";
    for (auto i = 0; i != totalWidth; i++)
    {
        std::cout << "-";
    }
    std::cout << "\n";
    for (auto iResult = 0; iResult != result->size(); iResult++)
    {
        std::cout << "|";
        for (size_t i = 0; i < result->fields().size(); i++)
        {
            std::cout << std::setw(result->field_width()[i]) << result->results()[iResult][i] << "|";
        }
        std::cout << std::endl;
    }
}
