#include "syntax/syntax.hpp"

class RDParser {
public:
    struct Result {
        unique_ptr<ProgramAST> program;
        vector<Diagnostic> diagnostics;
        string treeText;
    };

    explicit RDParser(const vector<Token>& tokens) : tokens_(tokens) {}

    Result parse() {
        Result result;
        result.program = parseProgram();
        result.diagnostics = diagnostics_;
        if (result.program) {
            result.treeText = astToString(*result.program);
        }
        return result;
    }

private:
    const vector<Token>& tokens_;
    size_t current_ = 0;
    vector<Diagnostic> diagnostics_;

    const Token& peek(int offset = 0) const {
        if (current_ + static_cast<size_t>(offset) < tokens_.size()) {
            return tokens_[current_ + static_cast<size_t>(offset)];
        }
        return tokens_.back();
    }

    const Token& previous() const {
        if (current_ == 0) {
            return tokens_.front();
        }
        return tokens_[current_ - 1];
    }

    bool isAtEnd() const { return peek().type == TokenType::ENDFILE; }
    bool check(TokenType t) const { return peek().type == t; }

    const Token& advance() {
        if (!isAtEnd()) {
            ++current_;
        }
        return previous();
    }

    bool match(std::initializer_list<TokenType> types) {
        for (TokenType t : types) {
            if (check(t)) {
                advance();
                return true;
            }
        }
        return false;
    }

    void reportError(const Token& tk, const string& msg) {
        diagnostics_.push_back(Diagnostic{"RDParser", tk.pos, msg});
    }

    Token consume(TokenType t, const string& msg) {
        if (check(t)) {
            return advance();
        }
        reportError(peek(), msg + ", got " + tokenTypeName(peek().type));
        Token fake;
        fake.type = t;
        fake.pos = peek().pos;
        if (!isAtEnd()) {
            advance();
        }
        return fake;
    }

    static bool isTypeStart(TokenType t) {
        return t == TokenType::INTEGER ||
               t == TokenType::CHAR ||
               t == TokenType::ARRAY ||
               t == TokenType::RECORD ||
               t == TokenType::ID;
    }

    unique_ptr<ProgramAST> parseProgram() {
        auto program = std::make_unique<ProgramAST>();
        consume(TokenType::PROGRAM, "Expected 'program'");
        Token name = consume(TokenType::ID, "Expected program name");
        program->name = name.lexeme;
        program->pos = name.pos;
        program->declPart = parseDeclarePart();
        program->body = parseProgramBody();
        consume(TokenType::DOT, "Expected '.' at end of program");
        return program;
    }

    unique_ptr<DeclPart> parseDeclarePart() {
        auto decl = std::make_unique<DeclPart>();
        parseTypeDec(*decl);
        parseVarDec(*decl);
        parseProcDec(*decl);
        return decl;
    }

    vector<string> parseIdList() {
        vector<string> names;
        Token id = consume(TokenType::ID, "Expected identifier");
        if (!id.lexeme.empty()) {
            names.push_back(id.lexeme);
        }
        while (match({TokenType::COMMA})) {
            Token next = consume(TokenType::ID, "Expected identifier after ','");
            if (!next.lexeme.empty()) {
                names.push_back(next.lexeme);
            }
        }
        return names;
    }

    unique_ptr<TypeExpr> parseBaseTypeOnly() {
        auto type = std::make_unique<TypeExpr>();
        type->pos = peek().pos;
        if (match({TokenType::INTEGER})) {
            type->kind = TypeExprKind::BaseInteger;
        } else if (match({TokenType::CHAR})) {
            type->kind = TypeExprKind::BaseChar;
        } else {
            reportError(peek(), "Expected base type 'integer' or 'char'");
            type->kind = TypeExprKind::BaseInteger;
        }
        return type;
    }

    unique_ptr<TypeExpr> parseTypeName() {
        if (check(TokenType::INTEGER) || check(TokenType::CHAR)) {
            return parseBaseTypeOnly();
        }
        if (check(TokenType::ARRAY)) {
            auto type = std::make_unique<TypeExpr>();
            type->kind = TypeExprKind::Array;
            type->pos = peek().pos;
            consume(TokenType::ARRAY, "Expected 'array'");
            consume(TokenType::LMIDPAREN, "Expected '[' after array");
            Token low = consume(TokenType::INTC, "Expected lower bound integer");
            consume(TokenType::UNDERANGE, "Expected '..'");
            Token high = consume(TokenType::INTC, "Expected upper bound integer");
            consume(TokenType::RMIDPAREN, "Expected ']'");
            consume(TokenType::OF, "Expected 'of'");
            type->elemType = parseBaseTypeOnly();
            try {
                type->low = std::stoi(low.lexeme);
                type->high = std::stoi(high.lexeme);
            } catch (...) {
                reportError(low, "Invalid array bounds");
                type->low = 0;
                type->high = -1;
            }
            return type;
        }
        if (check(TokenType::RECORD)) {
            auto type = std::make_unique<TypeExpr>();
            type->kind = TypeExprKind::Record;
            type->pos = peek().pos;
            consume(TokenType::RECORD, "Expected 'record'");
            while (!check(TokenType::END) && !isAtEnd()) {
                if (!isTypeStart(peek().type)) {
                    reportError(peek(), "Expected field declaration in record");
                    advance();
                    continue;
                }
                RecordFieldDecl field;
                field.pos = peek().pos;
                field.type = parseTypeName();
                field.names = parseIdList();
                consume(TokenType::SEMI, "Expected ';' after record field declaration");
                type->fields.push_back(std::move(field));
            }
            consume(TokenType::END, "Expected 'end' to close record");
            return type;
        }
        if (check(TokenType::ID)) {
            auto type = std::make_unique<TypeExpr>();
            Token id = consume(TokenType::ID, "Expected type name");
            type->kind = TypeExprKind::Alias;
            type->aliasName = id.lexeme;
            type->pos = id.pos;
            return type;
        }

        reportError(peek(), "Expected type name");
        auto fallback = std::make_unique<TypeExpr>();
        fallback->kind = TypeExprKind::BaseInteger;
        fallback->pos = peek().pos;
        if (!isAtEnd()) {
            advance();
        }
        return fallback;
    }

    void parseTypeDec(DeclPart& decl) {
        if (!match({TokenType::TYPE})) {
            return;
        }
        if (!check(TokenType::ID)) {
            reportError(peek(), "Expected type declaration after 'type'");
        }
        while (check(TokenType::ID)) {
            auto td = std::make_unique<TypeDecl>();
            Token id = consume(TokenType::ID, "Expected type identifier");
            td->name = id.lexeme;
            td->pos = id.pos;
            consume(TokenType::EQ, "Expected '=' in type declaration");
            td->type = parseTypeName();
            consume(TokenType::SEMI, "Expected ';' after type declaration");
            decl.typeDecls.push_back(std::move(td));
        }
    }

    void parseVarDec(DeclPart& decl) {
        if (!match({TokenType::VAR})) {
            return;
        }
        if (!isTypeStart(peek().type)) {
            reportError(peek(), "Expected variable declaration after 'var'");
        }
        while (isTypeStart(peek().type)) {
            auto vd = std::make_unique<VarDecl>();
            vd->pos = peek().pos;
            vd->type = parseTypeName();
            vd->names = parseIdList();
            consume(TokenType::SEMI, "Expected ';' after variable declaration");
            decl.varDecls.push_back(std::move(vd));
        }
    }

    ParamDecl parseParam() {
        ParamDecl param;
        param.pos = peek().pos;
        param.byRef = match({TokenType::VAR});
        param.type = parseTypeName();
        param.names = parseIdList();
        return param;
    }

    void parseParamList(vector<ParamDecl>& params) {
        if (check(TokenType::RPAREN)) {
            return;
        }
        params.push_back(parseParam());
        while (match({TokenType::SEMI})) {
            params.push_back(parseParam());
        }
    }

    unique_ptr<ProcDecl> parseProcDeclOne() {
        auto proc = std::make_unique<ProcDecl>();
        consume(TokenType::PROCEDURE, "Expected 'procedure'");
        Token name = consume(TokenType::ID, "Expected procedure name");
        proc->name = name.lexeme;
        proc->pos = name.pos;
        consume(TokenType::LPAREN, "Expected '(' after procedure name");
        parseParamList(proc->params);
        consume(TokenType::RPAREN, "Expected ')'");
        consume(TokenType::SEMI, "Expected ';' after procedure head");
        proc->declPart = parseDeclarePart();
        proc->body = parseProgramBody();
        return proc;
    }

    void parseProcDec(DeclPart& decl) {
        while (check(TokenType::PROCEDURE)) {
            decl.procDecls.push_back(parseProcDeclOne());
        }
    }

    unique_ptr<VarRef> parseVarRefFromIdToken(Token idToken) {
        auto var = std::make_unique<VarRef>();
        var->name = idToken.lexeme;
        var->pos = idToken.pos;
        if (match({TokenType::LMIDPAREN})) {
            var->access = VarRef::AccessKind::Index;
            var->indexExpr = parseExp();
            consume(TokenType::RMIDPAREN, "Expected ']'");
        } else if (match({TokenType::DOT})) {
            var->access = VarRef::AccessKind::Field;
            Token field = consume(TokenType::ID, "Expected field name after '.'");
            var->fieldName = field.lexeme;
            if (match({TokenType::LMIDPAREN})) {
                var->fieldIndexExpr = parseExp();
                consume(TokenType::RMIDPAREN, "Expected ']'");
            }
        }
        return var;
    }

    unique_ptr<Expr> makeIntExpr(int value, const SourcePos& pos) {
        auto expr = std::make_unique<Expr>();
        expr->kind = ExprKind::IntConst;
        expr->intValue = value;
        expr->pos = pos;
        return expr;
    }

    unique_ptr<Expr> parseFactor() {
        if (match({TokenType::LPAREN})) {
            auto expr = parseExp();
            consume(TokenType::RPAREN, "Expected ')'");
            return expr;
        }
        if (check(TokenType::INTC)) {
            Token tk = advance();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::IntConst;
            expr->pos = tk.pos;
            try {
                expr->intValue = std::stoi(tk.lexeme);
            } catch (...) {
                reportError(tk, "Invalid integer constant");
                expr->intValue = 0;
            }
            return expr;
        }
        if (check(TokenType::CHARC)) {
            Token tk = advance();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::CharConst;
            expr->pos = tk.pos;
            expr->charValue = tk.lexeme.empty() ? '\0' : tk.lexeme[0];
            return expr;
        }
        if (check(TokenType::ID)) {
            Token id = advance();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Var;
            expr->pos = id.pos;
            expr->var = parseVarRefFromIdToken(id);
            return expr;
        }
        reportError(peek(), "Expected factor");
        if (!isAtEnd()) {
            advance();
        }
        return makeIntExpr(0, peek().pos);
    }

    unique_ptr<Expr> parseTerm() {
        auto lhs = parseFactor();
        while (true) {
            BinaryOp op;
            if (match({TokenType::TIMES})) {
                op = BinaryOp::Mul;
            } else if (match({TokenType::OVER})) {
                op = BinaryOp::Div;
            } else {
                break;
            }
            auto rhs = parseFactor();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->pos = lhs->pos;
            expr->op = op;
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
            lhs = std::move(expr);
        }
        return lhs;
    }

    unique_ptr<Expr> parseExp() {
        auto lhs = parseTerm();
        while (true) {
            BinaryOp op;
            if (match({TokenType::PLUS})) {
                op = BinaryOp::Add;
            } else if (match({TokenType::MINUS})) {
                op = BinaryOp::Sub;
            } else {
                break;
            }
            auto rhs = parseTerm();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->pos = lhs->pos;
            expr->op = op;
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
            lhs = std::move(expr);
        }
        return lhs;
    }

    unique_ptr<Expr> parseRelExp() {
        auto lhs = parseExp();
        if (match({TokenType::LT})) {
            auto rhs = parseExp();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->pos = lhs->pos;
            expr->op = BinaryOp::Lt;
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
            return expr;
        }
        if (match({TokenType::EQ})) {
            auto rhs = parseExp();
            auto expr = std::make_unique<Expr>();
            expr->kind = ExprKind::Binary;
            expr->pos = lhs->pos;
            expr->op = BinaryOp::Eq;
            expr->lhs = std::move(lhs);
            expr->rhs = std::move(rhs);
            return expr;
        }
        reportError(peek(), "Expected relational operator '<' or '='");
        return lhs;
    }

    unique_ptr<Stmt> parseAssignOrCallStmt() {
        Token id = consume(TokenType::ID, "Expected identifier");
        if (check(TokenType::LPAREN)) {
            auto stmt = std::make_unique<Stmt>();
            stmt->kind = StmtKind::Call;
            stmt->pos = id.pos;
            stmt->callName = id.lexeme;
            consume(TokenType::LPAREN, "Expected '('");
            if (!check(TokenType::RPAREN)) {
                stmt->callArgs.push_back(parseExp());
                while (match({TokenType::COMMA})) {
                    stmt->callArgs.push_back(parseExp());
                }
            }
            consume(TokenType::RPAREN, "Expected ')'");
            return stmt;
        }

        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::Assign;
        stmt->pos = id.pos;
        stmt->assignTarget = parseVarRefFromIdToken(id);
        consume(TokenType::ASSIGN, "Expected ':=' in assignment");
        stmt->assignValue = parseExp();
        return stmt;
    }

    unique_ptr<Stmt> parseReadStmt() {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::Read;
        Token tk = consume(TokenType::READ, "Expected 'read'");
        stmt->pos = tk.pos;
        consume(TokenType::LPAREN, "Expected '(' after read");
        Token id = consume(TokenType::ID, "Expected identifier in read");
        stmt->readVar = parseVarRefFromIdToken(id);
        consume(TokenType::RPAREN, "Expected ')' after read argument");
        return stmt;
    }

    unique_ptr<Stmt> parseWriteStmt() {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::Write;
        Token tk = consume(TokenType::WRITE, "Expected 'write'");
        stmt->pos = tk.pos;
        consume(TokenType::LPAREN, "Expected '(' after write");
        stmt->writeExpr = parseExp();
        consume(TokenType::RPAREN, "Expected ')' after write argument");
        return stmt;
    }

    unique_ptr<Stmt> parseReturnStmt() {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::Return;
        Token tk = consume(TokenType::RETURN, "Expected 'return'");
        stmt->pos = tk.pos;
        if (match({TokenType::LPAREN})) {
            stmt->returnExpr = parseExp();
            consume(TokenType::RPAREN, "Expected ')'");
        }
        return stmt;
    }

    unique_ptr<Stmt> parseIfStmt() {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::If;
        Token tk = consume(TokenType::IF, "Expected 'if'");
        stmt->pos = tk.pos;
        stmt->condition = parseRelExp();
        consume(TokenType::THEN, "Expected 'then'");
        stmt->thenStmts = parseStmtList({TokenType::ELSE, TokenType::FI});
        consume(TokenType::ELSE, "Expected 'else'");
        stmt->elseStmts = parseStmtList({TokenType::FI});
        consume(TokenType::FI, "Expected 'fi'");
        return stmt;
    }

    unique_ptr<Stmt> parseWhileStmt() {
        auto stmt = std::make_unique<Stmt>();
        stmt->kind = StmtKind::While;
        Token tk = consume(TokenType::WHILE, "Expected 'while'");
        stmt->pos = tk.pos;
        stmt->condition = parseRelExp();
        consume(TokenType::DO, "Expected 'do'");
        stmt->loopStmts = parseStmtList({TokenType::ENDWH});
        consume(TokenType::ENDWH, "Expected 'endwh'");
        return stmt;
    }

    unique_ptr<Stmt> parseStmt() {
        if (check(TokenType::IF)) return parseIfStmt();
        if (check(TokenType::WHILE)) return parseWhileStmt();
        if (check(TokenType::READ)) return parseReadStmt();
        if (check(TokenType::WRITE)) return parseWriteStmt();
        if (check(TokenType::RETURN)) return parseReturnStmt();
        if (check(TokenType::ID)) return parseAssignOrCallStmt();

        reportError(peek(), "Unexpected token in statement: " + tokenTypeName(peek().type));
        if (!isAtEnd()) {
            advance();
        }
        return nullptr;
    }

    vector<unique_ptr<Stmt>> parseStmtList(const set<TokenType>& stopTokens) {
        vector<unique_ptr<Stmt>> stmts;
        while (!isAtEnd() && stopTokens.count(peek().type) == 0) {
            auto stmt = parseStmt();
            if (stmt) {
                stmts.push_back(std::move(stmt));
            }
            if (match({TokenType::SEMI})) {
                continue;
            }
            if (stopTokens.count(peek().type) > 0) {
                break;
            }
            reportError(peek(), "Expected ';' between statements");
            while (!isAtEnd() && !check(TokenType::SEMI) && stopTokens.count(peek().type) == 0) {
                advance();
            }
            if (check(TokenType::SEMI)) {
                advance();
            }
        }
        return stmts;
    }

    vector<unique_ptr<Stmt>> parseProgramBody() {
        consume(TokenType::BEGIN, "Expected 'begin'");
        auto stmts = parseStmtList({TokenType::END});
        consume(TokenType::END, "Expected 'end'");
        return stmts;
    }
};

struct Production {
    string lhs;
    vector<string> rhs;
};

class LL1Parser {
public:
    struct Result {
        unique_ptr<ParseNode> tree;
        vector<Diagnostic> diagnostics;
    };

    LL1Parser() {
        buildGrammar();
        computeFirst();
        computeFollow();
        buildTable();
    }

    Result parse(const vector<Token>& tokens) const {
        Result result;
        result.tree = std::make_unique<ParseNode>("Program");

        vector<std::pair<string, ParseNode*>> stack;
        stack.push_back({kEndMarker, nullptr});
        stack.push_back({"Program", result.tree.get()});

        size_t index = 0;
        auto lookaheadSym = [&](size_t i) -> string {
            if (i >= tokens.size()) {
                return kEndMarker;
            }
            return tokenToTerminal(tokens[i].type);
        };

        while (!stack.empty()) {
            string topSym = stack.back().first;
            ParseNode* topNode = stack.back().second;
            stack.pop_back();

            string look = lookaheadSym(index);
            const Token& lookToken = tokens[std::min(index, tokens.size() - 1)];

            if (topSym == kEpsilon) {
                continue;
            }

            if (topSym == kEndMarker) {
                if (look != kEndMarker) {
                    result.diagnostics.push_back(Diagnostic{
                        "LL1Parser",
                        lookToken.pos,
                        "Extra tokens after parse complete"
                    });
                }
                break;
            }

            if (isTerminal(topSym)) {
                if (topSym == look) {
                    if (topNode && index < tokens.size()) {
                        topNode->lexeme = tokens[index].lexeme;
                    }
                    if (index < tokens.size()) {
                        ++index;
                    }
                } else {
                    result.diagnostics.push_back(Diagnostic{
                        "LL1Parser",
                        lookToken.pos,
                        "Terminal mismatch: expected " + topSym + ", found " + look
                    });
                    // Insert missing terminal logically; continue.
                }
                continue;
            }

            auto rowIt = table_.find(topSym);
            bool hasEntry = false;
            int prodIndex = -1;
            if (rowIt != table_.end()) {
                auto colIt = rowIt->second.find(look);
                if (colIt != rowIt->second.end()) {
                    hasEntry = true;
                    prodIndex = colIt->second;
                }
            }

            if (!hasEntry) {
                result.diagnostics.push_back(Diagnostic{
                    "LL1Parser",
                    lookToken.pos,
                    "No production for (" + topSym + ", " + look + ")"
                });
                auto followIt = follow_.find(topSym);
                bool inFollow = (followIt != follow_.end() && followIt->second.count(look) > 0);
                if (inFollow || look == kEndMarker) {
                    continue;
                }
                if (look != kEndMarker && index < tokens.size()) {
                    ++index;
                    stack.push_back({topSym, topNode});
                    continue;
                }
                break;
            }

            const Production& p = productions_[static_cast<size_t>(prodIndex)];
            vector<ParseNode*> childPtrs;
            childPtrs.reserve(p.rhs.size());
            for (const string& sym : p.rhs) {
                if (topNode) {
                    topNode->children.push_back(std::make_unique<ParseNode>(sym));
                    childPtrs.push_back(topNode->children.back().get());
                } else {
                    childPtrs.push_back(nullptr);
                }
            }
            for (size_t i = p.rhs.size(); i > 0; --i) {
                const string& sym = p.rhs[i - 1];
                if (sym == kEpsilon) {
                    continue;
                }
                stack.push_back({sym, childPtrs[i - 1]});
            }
        }

        return result;
    }

private:
    static constexpr const char* kEpsilon = "EPS";
    static constexpr const char* kEndMarker = "$";

    vector<Production> productions_;
    set<string> nonterminals_;
    set<string> terminals_;
    map<string, set<string>> first_;
    map<string, set<string>> follow_;
    map<string, map<string, int>> table_;

    static void addSet(set<string>& target, const set<string>& src, bool* changed = nullptr) {
        for (const auto& s : src) {
            auto inserted = target.insert(s).second;
            if (inserted && changed) {
                *changed = true;
            }
        }
    }

    void addProduction(const string& lhs, const vector<string>& rhs) {
        productions_.push_back(Production{lhs, rhs});
        nonterminals_.insert(lhs);
    }

    bool isNonterminal(const string& sym) const {
        return nonterminals_.count(sym) > 0;
    }

    bool isTerminal(const string& sym) const {
        return terminals_.count(sym) > 0 || sym == kEndMarker;
    }

    string tokenToTerminal(TokenType t) const {
        switch (t) {
            case TokenType::ENDFILE: return kEndMarker;
            case TokenType::PROGRAM: return "PROGRAM";
            case TokenType::PROCEDURE: return "PROCEDURE";
            case TokenType::TYPE: return "TYPE";
            case TokenType::VAR: return "VAR";
            case TokenType::IF: return "IF";
            case TokenType::THEN: return "THEN";
            case TokenType::ELSE: return "ELSE";
            case TokenType::FI: return "FI";
            case TokenType::WHILE: return "WHILE";
            case TokenType::DO: return "DO";
            case TokenType::ENDWH: return "ENDWH";
            case TokenType::BEGIN: return "BEGIN";
            case TokenType::END: return "END";
            case TokenType::READ: return "READ";
            case TokenType::WRITE: return "WRITE";
            case TokenType::ARRAY: return "ARRAY";
            case TokenType::OF: return "OF";
            case TokenType::RECORD: return "RECORD";
            case TokenType::RETURN: return "RETURN";
            case TokenType::INTEGER: return "INTEGER";
            case TokenType::CHAR: return "CHAR";
            case TokenType::ID: return "ID";
            case TokenType::INTC: return "INTC";
            case TokenType::CHARC: return "CHARC";
            case TokenType::ASSIGN: return "ASSIGN";
            case TokenType::EQ: return "EQ";
            case TokenType::LT: return "LT";
            case TokenType::PLUS: return "PLUS";
            case TokenType::MINUS: return "MINUS";
            case TokenType::TIMES: return "TIMES";
            case TokenType::OVER: return "OVER";
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::DOT: return "DOT";
            case TokenType::COLON: return "COLON";
            case TokenType::SEMI: return "SEMI";
            case TokenType::COMMA: return "COMMA";
            case TokenType::LMIDPAREN: return "LMIDPAREN";
            case TokenType::RMIDPAREN: return "RMIDPAREN";
            case TokenType::UNDERANGE: return "UNDERANGE";
            case TokenType::ERROR: return "ERROR";
        }
        return "ERROR";
    }

    void buildGrammar() {
        addProduction("Program", {"ProgramHead", "DeclarePart", "ProgramBody", "DOT"});
        addProduction("ProgramHead", {"PROGRAM", "ProgramName"});
        addProduction("ProgramName", {"ID"});

        addProduction("DeclarePart", {"TypeDec", "VarDec", "ProcDec"});

        addProduction("TypeDec", {kEpsilon});
        addProduction("TypeDec", {"TypeDeclaration"});
        addProduction("TypeDeclaration", {"TYPE", "TypeDecList"});
        addProduction("TypeDecList", {"TypeId", "EQ", "TypeName", "SEMI", "TypeDecMore"});
        addProduction("TypeDecMore", {kEpsilon});
        addProduction("TypeDecMore", {"TypeDecList"});
        addProduction("TypeId", {"ID"});

        addProduction("TypeName", {"BaseType"});
        addProduction("TypeName", {"StructureType"});
        addProduction("TypeName", {"ID"});
        addProduction("BaseType", {"INTEGER"});
        addProduction("BaseType", {"CHAR"});
        addProduction("StructureType", {"ArrayType"});
        addProduction("StructureType", {"RecType"});
        addProduction("ArrayType", {"ARRAY", "LMIDPAREN", "Low", "UNDERANGE", "Top", "RMIDPAREN", "OF", "BaseType"});
        addProduction("Low", {"INTC"});
        addProduction("Top", {"INTC"});
        addProduction("RecType", {"RECORD", "FieldDecList", "END"});
        addProduction("FieldDecList", {"BaseType", "IdList", "SEMI", "FieldDecMore"});
        addProduction("FieldDecList", {"ArrayType", "IdList", "SEMI", "FieldDecMore"});
        addProduction("FieldDecMore", {kEpsilon});
        addProduction("FieldDecMore", {"FieldDecList"});
        addProduction("IdList", {"ID", "IdMore"});
        addProduction("IdMore", {kEpsilon});
        addProduction("IdMore", {"COMMA", "IdList"});

        addProduction("VarDec", {kEpsilon});
        addProduction("VarDec", {"VarDeclaration"});
        addProduction("VarDeclaration", {"VAR", "VarDecList"});
        addProduction("VarDecList", {"TypeName", "VarIdList", "SEMI", "VarDecMore"});
        addProduction("VarDecMore", {kEpsilon});
        addProduction("VarDecMore", {"VarDecList"});
        addProduction("VarIdList", {"ID", "VarIdMore"});
        addProduction("VarIdMore", {kEpsilon});
        addProduction("VarIdMore", {"COMMA", "VarIdList"});

        addProduction("ProcDec", {kEpsilon});
        addProduction("ProcDec", {"ProcDeclaration"});
        addProduction("ProcDeclaration", {"PROCEDURE", "ProcName", "LPAREN", "ParamList", "RPAREN", "SEMI", "ProcDecPart", "ProcBody", "ProcDecMore"});
        addProduction("ProcDecMore", {kEpsilon});
        addProduction("ProcDecMore", {"ProcDeclaration"});
        addProduction("ProcName", {"ID"});

        addProduction("ParamList", {kEpsilon});
        addProduction("ParamList", {"ParamDecList"});
        addProduction("ParamDecList", {"Param", "ParamMore"});
        addProduction("ParamMore", {kEpsilon});
        addProduction("ParamMore", {"SEMI", "ParamDecList"});
        addProduction("Param", {"TypeName", "FormList"});
        addProduction("Param", {"VAR", "TypeName", "FormList"});
        addProduction("FormList", {"ID", "FidMore"});
        addProduction("FidMore", {kEpsilon});
        addProduction("FidMore", {"COMMA", "FormList"});

        addProduction("ProcDecPart", {"DeclarePart"});
        addProduction("ProcBody", {"ProgramBody"});

        addProduction("ProgramBody", {"BEGIN", "StmList", "END"});
        addProduction("StmList", {"Stm", "StmMore"});
        addProduction("StmMore", {kEpsilon});
        addProduction("StmMore", {"SEMI", "StmList"});

        addProduction("Stm", {"ConditionalStm"});
        addProduction("Stm", {"LoopStm"});
        addProduction("Stm", {"InputStm"});
        addProduction("Stm", {"OutputStm"});
        addProduction("Stm", {"ReturnStm"});
        addProduction("Stm", {"ID", "AssCall"});

        addProduction("AssCall", {"AssignmentRest"});
        addProduction("AssCall", {"CallStmRest"});
        addProduction("AssignmentRest", {"VariMore", "ASSIGN", "Exp"});

        addProduction("ConditionalStm", {"IF", "RelExp", "THEN", "StmList", "ELSE", "StmList", "FI"});
        addProduction("LoopStm", {"WHILE", "RelExp", "DO", "StmList", "ENDWH"});
        addProduction("InputStm", {"READ", "LPAREN", "Invar", "RPAREN"});
        addProduction("Invar", {"ID"});
        addProduction("OutputStm", {"WRITE", "LPAREN", "Exp", "RPAREN"});
        addProduction("ReturnStm", {"RETURN", "ReturnTail"});
        addProduction("ReturnTail", {kEpsilon});
        addProduction("ReturnTail", {"LPAREN", "Exp", "RPAREN"});

        addProduction("CallStmRest", {"LPAREN", "ActParamList", "RPAREN"});
        addProduction("ActParamList", {kEpsilon});
        addProduction("ActParamList", {"Exp", "ActParamMore"});
        addProduction("ActParamMore", {kEpsilon});
        addProduction("ActParamMore", {"COMMA", "ActParamList"});

        addProduction("RelExp", {"Exp", "OtherRelE"});
        addProduction("OtherRelE", {"CmpOp", "Exp"});
        addProduction("Exp", {"Term", "OtherTerm"});
        addProduction("OtherTerm", {kEpsilon});
        addProduction("OtherTerm", {"AddOp", "Exp"});
        addProduction("Term", {"Factor", "OtherFactor"});
        addProduction("OtherFactor", {kEpsilon});
        addProduction("OtherFactor", {"MultOp", "Term"});
        addProduction("Factor", {"LPAREN", "Exp", "RPAREN"});
        addProduction("Factor", {"INTC"});
        addProduction("Factor", {"CHARC"});
        addProduction("Factor", {"Variable"});

        addProduction("Variable", {"ID", "VariMore"});
        addProduction("VariMore", {kEpsilon});
        addProduction("VariMore", {"LMIDPAREN", "Exp", "RMIDPAREN"});
        addProduction("VariMore", {"DOT", "FieldVar"});
        addProduction("FieldVar", {"ID", "FieldVarMore"});
        addProduction("FieldVarMore", {kEpsilon});
        addProduction("FieldVarMore", {"LMIDPAREN", "Exp", "RMIDPAREN"});

        addProduction("CmpOp", {"LT"});
        addProduction("CmpOp", {"EQ"});
        addProduction("AddOp", {"PLUS"});
        addProduction("AddOp", {"MINUS"});
        addProduction("MultOp", {"TIMES"});
        addProduction("MultOp", {"OVER"});

        for (const auto& p : productions_) {
            for (const auto& sym : p.rhs) {
                if (sym == kEpsilon) continue;
                if (!isNonterminal(sym)) {
                    terminals_.insert(sym);
                }
            }
        }
        terminals_.insert(kEndMarker);
    }

    set<string> firstOfSequence(const vector<string>& seq) const {
        set<string> result;
        bool allNullable = true;
        if (seq.empty()) {
            result.insert(kEpsilon);
            return result;
        }
        for (const auto& sym : seq) {
            auto it = first_.find(sym);
            set<string> cur;
            if (it != first_.end()) {
                cur = it->second;
            } else {
                cur.insert(sym);
            }
            for (const auto& v : cur) {
                if (v != kEpsilon) {
                    result.insert(v);
                }
            }
            if (cur.count(kEpsilon) == 0) {
                allNullable = false;
                break;
            }
        }
        if (allNullable) {
            result.insert(kEpsilon);
        }
        return result;
    }

    void computeFirst() {
        for (const auto& nt : nonterminals_) {
            first_[nt] = {};
        }
        for (const auto& t : terminals_) {
            first_[t] = {t};
        }
        first_[kEpsilon] = {kEpsilon};

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& p : productions_) {
                auto& firstA = first_[p.lhs];
                auto firstAlpha = firstOfSequence(p.rhs);
                for (const auto& sym : firstAlpha) {
                    if (firstA.insert(sym).second) {
                        changed = true;
                    }
                }
            }
        }
    }

    void computeFollow() {
        for (const auto& nt : nonterminals_) {
            follow_[nt] = {};
        }
        follow_["Program"].insert(kEndMarker);

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& p : productions_) {
                for (size_t i = 0; i < p.rhs.size(); ++i) {
                    const string& B = p.rhs[i];
                    if (!isNonterminal(B)) {
                        continue;
                    }
                    vector<string> beta;
                    for (size_t j = i + 1; j < p.rhs.size(); ++j) {
                        beta.push_back(p.rhs[j]);
                    }
                    auto firstBeta = firstOfSequence(beta);
                    set<string> toAdd;
                    for (const auto& s : firstBeta) {
                        if (s != kEpsilon) {
                            toAdd.insert(s);
                        }
                    }
                    addSet(follow_[B], toAdd, &changed);
                    if (beta.empty() || firstBeta.count(kEpsilon) > 0) {
                        addSet(follow_[B], follow_[p.lhs], &changed);
                    }
                }
            }
        }
    }

    void buildTable() {
        for (size_t i = 0; i < productions_.size(); ++i) {
            const auto& p = productions_[i];
            auto firstAlpha = firstOfSequence(p.rhs);
            for (const auto& t : firstAlpha) {
                if (t == kEpsilon) continue;
                if (table_[p.lhs].count(t) == 0) {
                    table_[p.lhs][t] = static_cast<int>(i);
                }
            }
            if (firstAlpha.count(kEpsilon) > 0) {
                for (const auto& b : follow_[p.lhs]) {
                    if (table_[p.lhs].count(b) == 0) {
                        table_[p.lhs][b] = static_cast<int>(i);
                    }
                }
            }
        }
    }
};



RDParseResult runRDParser(const vector<Token>& tokens) {
    RDParser parser(tokens);
    auto res = parser.parse();
    RDParseResult out;
    out.program = std::move(res.program);
    out.diagnostics = std::move(res.diagnostics);
    out.treeText = std::move(res.treeText);
    return out;
}

LL1ParseResult runLL1Parser(const vector<Token>& tokens) {
    LL1Parser parser;
    auto res = parser.parse(tokens);
    LL1ParseResult out;
    out.tree = std::move(res.tree);
    out.diagnostics = std::move(res.diagnostics);
    return out;
}
