#include "driver.hpp"

#include "semantic/semantic.hpp"
#include "codegen/codegen.hpp"
#include "lexical/lexical.hpp"
#include "syntax/syntax.hpp"

static string symbolKindName(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::TypeName: return "type";
        case SymbolKind::Variable: return "var";
        case SymbolKind::Procedure: return "proc";
        case SymbolKind::Parameter: return "param";
    }
    return "unknown";
}

static string typeToString(const shared_ptr<TypeInfo>& type, int depth = 0) {
    if (!type) return "<null>";
    if (depth > 8) return "<...>";
    switch (type->kind) {
        case TypeKind::Integer:
            return "integer";
        case TypeKind::Char:
            return "char";
        case TypeKind::Error:
            return "<error>";
        case TypeKind::Array: {
            std::ostringstream oss;
            oss << "array[" << type->low << ".." << type->high << "] of "
                << typeToString(type->elementType, depth + 1);
            return oss.str();
        }
        case TypeKind::Record: {
            std::ostringstream oss;
            oss << "record{";
            for (size_t i = 0; i < type->fields.size(); ++i) {
                const auto& f = type->fields[i];
                if (i > 0) oss << ", ";
                oss << f.name << ":" << typeToString(f.type, depth + 1)
                    << "@+" << f.offset;
            }
            oss << "}";
            return oss.str();
        }
    }
    return "<unknown-type>";
}

static void appendScopeSymbolTable(const Scope* scope, int depth, std::ostringstream& oss) {
    if (!scope) return;

    string indent(depth * 2, ' ');
    oss << indent << "[scope level=" << scope->level;
    if (scope->ownerProc && scope->ownerProc->symbol) {
        oss << ", owner=" << scope->ownerProc->symbol->name;
    } else {
        oss << ", owner=<global>";
    }
    oss << "]\n";

    vector<const Symbol*> symbols;
    symbols.reserve(scope->symbols.size());
    for (const auto& kv : scope->symbols) {
        symbols.push_back(kv.second.get());
    }
    std::sort(symbols.begin(), symbols.end(), [](const Symbol* a, const Symbol* b) {
        if (a->name != b->name) return a->name < b->name;
        return static_cast<int>(a->kind) < static_cast<int>(b->kind);
    });

    if (symbols.empty()) {
        oss << indent << "  (empty)\n";
    } else {
        for (const Symbol* sym : symbols) {
            oss << indent
                << "  - name=" << sym->name
                << ", kind=" << symbolKindName(sym->kind)
                << ", type=" << typeToString(sym->type)
                << ", level=" << sym->level;
            if (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter) {
                oss << ", offset=" << sym->offset;
            }
            if (sym->kind == SymbolKind::Parameter) {
                oss << ", byRef=" << (sym->byRef ? "true" : "false");
            }
            if (!sym->label.empty()) {
                oss << ", label=" << sym->label;
            }
            if (sym->kind == SymbolKind::Procedure && sym->proc) {
                oss << ", params=" << sym->proc->params.size()
                    << ", locals=" << sym->proc->localBytes;
            }
            oss << "\n";
        }
    }

    for (const auto& child : scope->children) {
        appendScopeSymbolTable(child.get(), depth + 1, oss);
    }
}

static string renderSymbolTable(const Scope* globalScope) {
    if (!globalScope) {
        return "No symbol table generated.\n";
    }
    std::ostringstream oss;
    oss << "Symbol Table\n";
    oss << "============\n";
    appendScopeSymbolTable(globalScope, 0, oss);
    return oss.str();
}

static bool readFileText(const std::filesystem::path& path, string& content, string& err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        err = "Cannot open file: " + path.string();
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    content = ss.str();
    return true;
}

static bool writeFileText(const std::filesystem::path& path, const string& content, string& err) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        err = "Cannot write file: " + path.string();
        return false;
    }
    out << content;
    return true;
}

static string renderDiagnostics(const vector<Diagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return "No errors.\n";
    }
    std::ostringstream oss;
    for (const auto& d : diagnostics) {
        oss << formatDiagnostic(d) << "\n";
    }
    return oss.str();
}

static void printUsage() {
    std::cout
        << "Usage:\n"
        << "  snlc --input <file.snl> [--output <out.asm>] [--mode <lex|rd|ll1|sem|all>]\n"
        << "  snlc <file.snl> [out.asm]\n";
}

int runCompilerFromArgs(int argc, char** argv) {
    std::filesystem::path inputPath;
    std::filesystem::path outputAsmPath;
    string mode = "all";

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else if ((arg == "--input" || arg == "-i") && i + 1 < argc) {
            inputPath = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            outputAsmPath = argv[++i];
        } else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else if (outputAsmPath.empty()) {
            outputAsmPath = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    if (inputPath.empty()) {
        printUsage();
        return 1;
    }

    if (outputAsmPath.empty()) {
        outputAsmPath = inputPath.parent_path() / (inputPath.stem().string() + ".asm");
    }

    string source;
    string ioErr;
    if (!readFileText(inputPath, source, ioErr)) {
        std::cerr << ioErr << "\n";
        return 1;
    }
    if (source.size() >= 3 &&
        static_cast<unsigned char>(source[0]) == 0xEF &&
        static_cast<unsigned char>(source[1]) == 0xBB &&
        static_cast<unsigned char>(source[2]) == 0xBF) {
        source.erase(0, 3);
    }

    std::filesystem::path tokensPath = inputPath.parent_path() / (inputPath.stem().string() + ".tokens.txt");
    std::filesystem::path rdTreePath = inputPath.parent_path() / (inputPath.stem().string() + ".rd_tree.txt");
    std::filesystem::path ll1TreePath = inputPath.parent_path() / (inputPath.stem().string() + ".ll1_tree.txt");
    std::filesystem::path symbolsPath = inputPath.parent_path() / (inputPath.stem().string() + ".symbols.txt");
    std::filesystem::path errorsPath = inputPath.parent_path() / (inputPath.stem().string() + ".errors.txt");

    vector<Diagnostic> allDiagnostics;

    auto lexResult = runLexer(source);
    bool lexOk = lexResult.diagnostics.empty();
    if (!writeFileText(tokensPath, renderTokens(lexResult.tokens), ioErr)) {
        std::cerr << ioErr << "\n";
        return 1;
    }
    allDiagnostics.insert(allDiagnostics.end(), lexResult.diagnostics.begin(), lexResult.diagnostics.end());

    bool runRD = (mode == "rd" || mode == "sem" || mode == "all");
    bool runLL1 = (mode == "ll1" || mode == "all");
    bool runSem = (mode == "sem" || mode == "all");
    bool runCodeGen = (mode == "all");
    if (mode == "lex") {
        runRD = false;
        runLL1 = false;
        runSem = false;
        runCodeGen = false;
    }

    unique_ptr<ProgramAST> program;
    bool rdOk = false;
    if (runRD) {
        auto rdResult = runRDParser(lexResult.tokens);
        if (!rdResult.treeText.empty()) {
            if (!writeFileText(rdTreePath, rdResult.treeText, ioErr)) {
                std::cerr << ioErr << "\n";
                return 1;
            }
        }
        rdOk = rdResult.diagnostics.empty();
        allDiagnostics.insert(allDiagnostics.end(), rdResult.diagnostics.begin(), rdResult.diagnostics.end());
        program = std::move(rdResult.program);
    }

    if (runLL1) {
        auto ll1Result = runLL1Parser(lexResult.tokens);
        if (ll1Result.tree) {
            if (!writeFileText(ll1TreePath, parseTreeToString(ll1Result.tree.get()), ioErr)) {
                std::cerr << ioErr << "\n";
                return 1;
            }
        }
        allDiagnostics.insert(allDiagnostics.end(), ll1Result.diagnostics.begin(), ll1Result.diagnostics.end());
    }

    bool semanticReady = false;
    bool mipsGenerated = false;
    bool symbolsGenerated = false;
    if (runSem && program && lexOk && rdOk) {
        SemanticAnalyzer semantic;
        auto semResult = semantic.analyze(*program);
        allDiagnostics.insert(allDiagnostics.end(), semResult.diagnostics.begin(), semResult.diagnostics.end());
        semanticReady = semResult.diagnostics.empty();
        if (!writeFileText(symbolsPath, renderSymbolTable(semResult.globalScope), ioErr)) {
            std::cerr << ioErr << "\n";
            return 1;
        }
        symbolsGenerated = true;

        if (runCodeGen && semanticReady) {
            auto cgResult = runCodegen(*program, semantic);
            allDiagnostics.insert(allDiagnostics.end(), cgResult.diagnostics.begin(), cgResult.diagnostics.end());
            mipsGenerated = cgResult.generated;
            if (mipsGenerated) {
                if (!writeFileText(outputAsmPath, cgResult.code, ioErr)) {
                    std::cerr << ioErr << "\n";
                    return 1;
                }
            }
        }
    }

    if (runSem && !symbolsGenerated) {
        std::ostringstream oss;
        oss << "Symbol table not generated.\n";
        if (!lexOk) {
            oss << "- Lexical analysis has errors.\n";
        }
        if (runRD && !rdOk) {
            oss << "- Recursive-descent parsing has errors.\n";
        }
        if (!program) {
            oss << "- AST is unavailable.\n";
        }
        if (!writeFileText(symbolsPath, oss.str(), ioErr)) {
            std::cerr << ioErr << "\n";
            return 1;
        }
        symbolsGenerated = true;
    }

    if (!writeFileText(errorsPath, renderDiagnostics(allDiagnostics), ioErr)) {
        std::cerr << ioErr << "\n";
        return 1;
    }

    std::cout << "Input:       " << inputPath.string() << "\n";
    std::cout << "Tokens:      " << tokensPath.string() << "\n";
    if (runRD) {
        std::cout << "RD Tree:     " << rdTreePath.string() << "\n";
    }
    if (runLL1) {
        std::cout << "LL1 Tree:    " << ll1TreePath.string() << "\n";
    }
    if (runCodeGen) {
        if (mipsGenerated) {
            std::cout << "MIPS Output: " << outputAsmPath.string() << "\n";
        } else {
            std::cout << "MIPS Output: (not generated due to previous errors)\n";
        }
    }
    if (runSem) {
        std::cout << "Symbols:     " << symbolsPath.string() << "\n";
    }
    std::cout << "Errors:      " << errorsPath.string() << "\n";
    std::cout << "Diagnostics: " << allDiagnostics.size() << "\n";

    (void)semanticReady;
    return 0;
}

