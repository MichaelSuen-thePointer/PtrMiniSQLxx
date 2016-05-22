#include "../MiniSQL/Tokenizer.h"
#include <string>
int main()
{
    std::string teststring =
        R"__(
use database;
select x
from database
where table.token = 'abse"dasd';

insert into table values(0.121231, -.123, 123, 0.123, 'asdasd', "asd'asd");

select 'from
from 'select
where 'select.'from = "asd'asd'asd";

)__";

    Tokenizer lex(teststring);
}