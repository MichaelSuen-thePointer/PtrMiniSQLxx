// MiniSQL.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "Tokenizer.h"

int main()
{
    std::string teststring =
        R"__(0123123;0.123123;-.123;-0.123;-123)__";

    Tokenizer lex(teststring);
}
