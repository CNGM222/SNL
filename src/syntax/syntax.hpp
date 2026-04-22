#pragma once

#include "common/shared.hpp"

struct RDParseResult {
    unique_ptr<ProgramAST> program;
    vector<Diagnostic> diagnostics;
    string treeText;
};

RDParseResult runRDParser(const vector<Token>& tokens);

struct LL1ParseResult {
    unique_ptr<ParseNode> tree;
    vector<Diagnostic> diagnostics;
};

LL1ParseResult runLL1Parser(const vector<Token>& tokens);
