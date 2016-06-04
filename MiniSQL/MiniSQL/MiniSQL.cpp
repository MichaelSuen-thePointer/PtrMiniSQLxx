#include "stdafx.h"
#include "RecordManager.h"


int main()
{
    auto& rm = RecordManager::instance();

    //rm.create_table("test_table", 22);

    char entry[] = "helloworld, helloworld";

    rm.insert_entry("test_table", (byte*)entry);
    rm.insert_entry("test_table", (byte*)entry);
    rm.insert_entry("test_table", (byte*)entry);
}