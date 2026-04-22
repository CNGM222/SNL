#pragma once

#include "common/shared.hpp"

struct LexResult {
    vector<Token> tokens;
    vector<Diagnostic> diagnostics;
};

LexResult runLexer(const string& src);
string renderTokens(const vector<Token>& tokens);
