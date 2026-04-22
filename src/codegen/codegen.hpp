#pragma once

#include "semantic/semantic.hpp"

struct CodegenResult {
    string code;
    vector<Diagnostic> diagnostics;
    bool generated = false;
};

CodegenResult runCodegen(ProgramAST& program, SemanticAnalyzer& semantic);

