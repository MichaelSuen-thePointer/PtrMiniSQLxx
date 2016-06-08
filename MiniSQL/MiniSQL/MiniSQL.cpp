#include "stdafx.h"
#include "Interpreter.h"

int main()
{
    auto& rm = RecordManager::instance();

    char entry[33] = "++++----++++----++++----++++----";

    auto& table = rm.find_table("test_table");

    for(size_t i = 0; i != table.size(); i++)
    {
        if(strncmp(table[i]->as<char>(), entry, 32) != 0)
        {
            
        }
    }
}