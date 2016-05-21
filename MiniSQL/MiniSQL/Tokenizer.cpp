#include "stdafx.h"
#include "Tokenizer.h"


Tokenizer::Tokenizer()
{
    reset();
}

void Tokenizer::reset(const std::string& str)
{
    _current = str;
    _front = _current.begin();
    _end = _current.end();
    _state = None;
    _line = 1;
    _column = 1;
    _tokenLine = 1;
    _tokenColumn = 1;
    _tokens.clear();
}
