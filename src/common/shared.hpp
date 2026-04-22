#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using std::map;
using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;

struct SourcePos {
    int line = 1;
    int column = 1;
};

struct Diagnostic {
    string stage;
    SourcePos pos;
    string message;
};

static string formatDiagnostic(const Diagnostic& d) {
    std::ostringstream oss;
    oss << "[" << d.stage << "] "
        << "line " << d.pos.line << ", col " << d.pos.column << ": "
        << d.message;
    return oss.str();
}

enum class TokenType {
    ENDFILE,
    ERROR,
    PROGRAM,
    PROCEDURE,
    TYPE,
    VAR,
    IF,
    THEN,
    ELSE,
    FI,
    WHILE,
    DO,
    ENDWH,
    BEGIN,
    END,
    READ,
    WRITE,
    ARRAY,
    OF,
    RECORD,
    RETURN,
    INTEGER,
    CHAR,
    ID,
    INTC,
    CHARC,
    ASSIGN,
    EQ,
    LT,
    PLUS,
    MINUS,
    TIMES,
    OVER,
    LPAREN,
    RPAREN,
    DOT,
    COLON,
    SEMI,
    COMMA,
    LMIDPAREN,
    RMIDPAREN,
    UNDERANGE
};

static string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::ENDFILE: return "ENDFILE";
        case TokenType::ERROR: return "ERROR";
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
    }
    return "UNKNOWN";
}

struct Token {
    TokenType type = TokenType::ERROR;
    string lexeme;
    SourcePos pos;
};


struct ParseNode {
    explicit ParseNode(string s) : symbol(std::move(s)) {}
    string symbol;
    string lexeme;
    vector<unique_ptr<ParseNode>> children;
};

static void dumpParseNode(const ParseNode* node, std::ostream& out, int indentLevel) {
    for (int i = 0; i < indentLevel; ++i) {
        out << "  ";
    }
    out << node->symbol;
    if (!node->lexeme.empty()) {
        out << " " << node->lexeme;
    }
    out << "\n";
    for (const auto& child : node->children) {
        dumpParseNode(child.get(), out, indentLevel + 1);
    }
}

static string parseTreeToString(const ParseNode* root) {
    if (!root) {
        return "";
    }
    std::ostringstream oss;
    dumpParseNode(root, oss, 0);
    return oss.str();
}

struct TypeInfo;
struct Symbol;
struct ProcInfo;

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Lt,
    Eq
};

struct Expr;

struct VarRef {
    enum class AccessKind {
        None,
        Index,
        Field
    };

    string name;
    SourcePos pos;
    AccessKind access = AccessKind::None;
    unique_ptr<Expr> indexExpr;
    string fieldName;
    unique_ptr<Expr> fieldIndexExpr;

    Symbol* symbol = nullptr;
    shared_ptr<TypeInfo> resolvedType;
};

enum class ExprKind {
    IntConst,
    CharConst,
    Var,
    Binary
};

struct Expr {
    ExprKind kind = ExprKind::IntConst;
    SourcePos pos;
    int intValue = 0;
    char charValue = '\0';
    unique_ptr<VarRef> var;
    BinaryOp op = BinaryOp::Add;
    unique_ptr<Expr> lhs;
    unique_ptr<Expr> rhs;

    shared_ptr<TypeInfo> inferredType;
    bool isLValue = false;
};

enum class TypeExprKind {
    BaseInteger,
    BaseChar,
    Alias,
    Array,
    Record
};

struct TypeExpr;

struct RecordFieldDecl {
    unique_ptr<TypeExpr> type;
    vector<string> names;
    SourcePos pos;
};

struct TypeExpr {
    TypeExprKind kind = TypeExprKind::BaseInteger;
    SourcePos pos;
    string aliasName;
    int low = 0;
    int high = 0;
    unique_ptr<TypeExpr> elemType;
    vector<RecordFieldDecl> fields;
};

struct TypeDecl {
    string name;
    unique_ptr<TypeExpr> type;
    SourcePos pos;
};

struct VarDecl {
    unique_ptr<TypeExpr> type;
    vector<string> names;
    SourcePos pos;
};

struct ParamDecl {
    bool byRef = false;
    unique_ptr<TypeExpr> type;
    vector<string> names;
    SourcePos pos;
};

struct ProcDecl;

struct DeclPart {
    vector<unique_ptr<TypeDecl>> typeDecls;
    vector<unique_ptr<VarDecl>> varDecls;
    vector<unique_ptr<ProcDecl>> procDecls;
};

enum class StmtKind {
    Assign,
    If,
    While,
    Read,
    Write,
    Call,
    Return
};

struct Stmt {
    StmtKind kind = StmtKind::Assign;
    SourcePos pos;

    unique_ptr<VarRef> assignTarget;
    unique_ptr<Expr> assignValue;

    unique_ptr<Expr> condition;
    vector<unique_ptr<Stmt>> thenStmts;
    vector<unique_ptr<Stmt>> elseStmts;
    vector<unique_ptr<Stmt>> loopStmts;

    unique_ptr<VarRef> readVar;
    unique_ptr<Expr> writeExpr;

    string callName;
    vector<unique_ptr<Expr>> callArgs;
    Symbol* callSymbol = nullptr;

    unique_ptr<Expr> returnExpr;
};

struct ProcDecl {
    string name;
    SourcePos pos;
    vector<ParamDecl> params;
    unique_ptr<DeclPart> declPart;
    vector<unique_ptr<Stmt>> body;

    Symbol* symbol = nullptr;
    ProcInfo* procInfo = nullptr;
};

struct ProgramAST {
    string name;
    SourcePos pos;
    unique_ptr<DeclPart> declPart;
    vector<unique_ptr<Stmt>> body;
};

static string binaryOpName(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Eq: return "=";
    }
    return "?";
}

static void printIndent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) {
        out << ' ';
    }
}

static void dumpTypeExpr(const TypeExpr* type, std::ostream& out, int indent) {
    if (!type) {
        printIndent(out, indent);
        out << "(null type)\n";
        return;
    }
    printIndent(out, indent);
    switch (type->kind) {
        case TypeExprKind::BaseInteger:
            out << "Type: integer\n";
            break;
        case TypeExprKind::BaseChar:
            out << "Type: char\n";
            break;
        case TypeExprKind::Alias:
            out << "Type: alias(" << type->aliasName << ")\n";
            break;
        case TypeExprKind::Array:
            out << "Type: array[" << type->low << ".." << type->high << "] of\n";
            dumpTypeExpr(type->elemType.get(), out, indent + 2);
            break;
        case TypeExprKind::Record:
            out << "Type: record\n";
            for (const auto& field : type->fields) {
                printIndent(out, indent + 2);
                out << "Field: ";
                for (size_t i = 0; i < field.names.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << field.names[i];
                }
                out << "\n";
                dumpTypeExpr(field.type.get(), out, indent + 4);
            }
            break;
    }
}

static void dumpVarRef(const VarRef* var, std::ostream& out, int indent);
static void dumpExpr(const Expr* expr, std::ostream& out, int indent);
static void dumpStmt(const Stmt* stmt, std::ostream& out, int indent);
static void dumpDeclPart(const DeclPart* decl, std::ostream& out, int indent);

static void dumpVarRef(const VarRef* var, std::ostream& out, int indent) {
    if (!var) {
        printIndent(out, indent);
        out << "(null var)\n";
        return;
    }
    printIndent(out, indent);
    out << "VarRef: " << var->name << "\n";
    if (var->access == VarRef::AccessKind::Index) {
        printIndent(out, indent + 2);
        out << "Index:\n";
        dumpExpr(var->indexExpr.get(), out, indent + 4);
    } else if (var->access == VarRef::AccessKind::Field) {
        printIndent(out, indent + 2);
        out << "Field: " << var->fieldName << "\n";
        if (var->fieldIndexExpr) {
            printIndent(out, indent + 2);
            out << "FieldIndex:\n";
            dumpExpr(var->fieldIndexExpr.get(), out, indent + 4);
        }
    }
}

static void dumpExpr(const Expr* expr, std::ostream& out, int indent) {
    if (!expr) {
        printIndent(out, indent);
        out << "(null expr)\n";
        return;
    }
    printIndent(out, indent);
    switch (expr->kind) {
        case ExprKind::IntConst:
            out << "IntConst: " << expr->intValue << "\n";
            return;
        case ExprKind::CharConst:
            out << "CharConst: '" << expr->charValue << "'\n";
            return;
        case ExprKind::Var:
            out << "VarExpr:\n";
            dumpVarRef(expr->var.get(), out, indent + 2);
            return;
        case ExprKind::Binary:
            out << "BinaryExpr: " << binaryOpName(expr->op) << "\n";
            printIndent(out, indent + 2);
            out << "LHS:\n";
            dumpExpr(expr->lhs.get(), out, indent + 4);
            printIndent(out, indent + 2);
            out << "RHS:\n";
            dumpExpr(expr->rhs.get(), out, indent + 4);
            return;
    }
}

static void dumpStmt(const Stmt* stmt, std::ostream& out, int indent) {
    if (!stmt) {
        printIndent(out, indent);
        out << "(null stmt)\n";
        return;
    }
    printIndent(out, indent);
    switch (stmt->kind) {
        case StmtKind::Assign:
            out << "AssignStmt\n";
            printIndent(out, indent + 2);
            out << "Target:\n";
            dumpVarRef(stmt->assignTarget.get(), out, indent + 4);
            printIndent(out, indent + 2);
            out << "Value:\n";
            dumpExpr(stmt->assignValue.get(), out, indent + 4);
            break;
        case StmtKind::If:
            out << "IfStmt\n";
            printIndent(out, indent + 2);
            out << "Condition:\n";
            dumpExpr(stmt->condition.get(), out, indent + 4);
            printIndent(out, indent + 2);
            out << "Then:\n";
            for (const auto& s : stmt->thenStmts) {
                dumpStmt(s.get(), out, indent + 4);
            }
            printIndent(out, indent + 2);
            out << "Else:\n";
            for (const auto& s : stmt->elseStmts) {
                dumpStmt(s.get(), out, indent + 4);
            }
            break;
        case StmtKind::While:
            out << "WhileStmt\n";
            printIndent(out, indent + 2);
            out << "Condition:\n";
            dumpExpr(stmt->condition.get(), out, indent + 4);
            printIndent(out, indent + 2);
            out << "Body:\n";
            for (const auto& s : stmt->loopStmts) {
                dumpStmt(s.get(), out, indent + 4);
            }
            break;
        case StmtKind::Read:
            out << "ReadStmt\n";
            dumpVarRef(stmt->readVar.get(), out, indent + 2);
            break;
        case StmtKind::Write:
            out << "WriteStmt\n";
            dumpExpr(stmt->writeExpr.get(), out, indent + 2);
            break;
        case StmtKind::Call:
            out << "CallStmt: " << stmt->callName << "\n";
            for (size_t i = 0; i < stmt->callArgs.size(); ++i) {
                printIndent(out, indent + 2);
                out << "Arg[" << i << "]:\n";
                dumpExpr(stmt->callArgs[i].get(), out, indent + 4);
            }
            break;
        case StmtKind::Return:
            out << "ReturnStmt\n";
            if (stmt->returnExpr) {
                dumpExpr(stmt->returnExpr.get(), out, indent + 2);
            }
            break;
    }
}

static void dumpDeclPart(const DeclPart* decl, std::ostream& out, int indent) {
    if (!decl) {
        printIndent(out, indent);
        out << "(null declarations)\n";
        return;
    }
    printIndent(out, indent);
    out << "Declarations\n";

    if (!decl->typeDecls.empty()) {
        printIndent(out, indent + 2);
        out << "TypeDecls:\n";
        for (const auto& td : decl->typeDecls) {
            printIndent(out, indent + 4);
            out << td->name << " =\n";
            dumpTypeExpr(td->type.get(), out, indent + 6);
        }
    }

    if (!decl->varDecls.empty()) {
        printIndent(out, indent + 2);
        out << "VarDecls:\n";
        for (const auto& vd : decl->varDecls) {
            printIndent(out, indent + 4);
            out << "Names: ";
            for (size_t i = 0; i < vd->names.size(); ++i) {
                if (i > 0) out << ", ";
                out << vd->names[i];
            }
            out << "\n";
            dumpTypeExpr(vd->type.get(), out, indent + 6);
        }
    }

    if (!decl->procDecls.empty()) {
        printIndent(out, indent + 2);
        out << "ProcDecls:\n";
        for (const auto& pd : decl->procDecls) {
            printIndent(out, indent + 4);
            out << "Procedure " << pd->name << "\n";
            if (!pd->params.empty()) {
                printIndent(out, indent + 6);
                out << "Params:\n";
                for (const auto& p : pd->params) {
                    printIndent(out, indent + 8);
                    out << (p.byRef ? "var " : "");
                    for (size_t i = 0; i < p.names.size(); ++i) {
                        if (i > 0) out << ", ";
                        out << p.names[i];
                    }
                    out << "\n";
                    dumpTypeExpr(p.type.get(), out, indent + 10);
                }
            }
            dumpDeclPart(pd->declPart.get(), out, indent + 6);
            printIndent(out, indent + 6);
            out << "Body:\n";
            for (const auto& st : pd->body) {
                dumpStmt(st.get(), out, indent + 8);
            }
        }
    }
}

static void textbookIndent(std::ostream& out, int indent) {
    for (int i = 0; i < indent; ++i) {
        out << "  ";
    }
}

static string joinNames(const vector<string>& names) {
    std::ostringstream oss;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            oss << " ";
        }
        oss << names[i];
    }
    return oss.str();
}

static string typeToTextbookK(const TypeExpr* type) {
    if (!type) {
        return "ErrorK";
    }
    switch (type->kind) {
        case TypeExprKind::BaseInteger:
            return "IntegerK";
        case TypeExprKind::BaseChar:
            return "CharK";
        case TypeExprKind::Alias:
            return "IdK(" + type->aliasName + ")";
        case TypeExprKind::Array: {
            std::ostringstream oss;
            oss << "ArrayK[" << type->low << ".." << type->high << "] of " << typeToTextbookK(type->elemType.get());
            return oss.str();
        }
        case TypeExprKind::Record:
            return "RecordK";
    }
    return "ErrorK";
}

static string varRefToTextbook(const VarRef* var) {
    if (!var) {
        return "<null>";
    }
    std::ostringstream oss;
    oss << var->name;
    if (var->access == VarRef::AccessKind::Index) {
        oss << "[...]";
    } else if (var->access == VarRef::AccessKind::Field) {
        oss << "." << var->fieldName;
        if (var->fieldIndexExpr) {
            oss << "[...]";
        }
    }
    return oss.str();
}

static void dumpTextbookExpr(const Expr* expr, std::ostream& out, int indent) {
    if (!expr) {
        textbookIndent(out, indent);
        out << "ExpK <null>\n";
        return;
    }
    switch (expr->kind) {
        case ExprKind::IntConst:
            textbookIndent(out, indent);
            out << "ExpK Const " << expr->intValue << "\n";
            return;
        case ExprKind::CharConst:
            textbookIndent(out, indent);
            out << "ExpK Const '" << expr->charValue << "'\n";
            return;
        case ExprKind::Var:
            textbookIndent(out, indent);
            out << "ExpK " << varRefToTextbook(expr->var.get()) << " IdV\n";
            return;
        case ExprKind::Binary:
            textbookIndent(out, indent);
            out << "ExpK Op " << binaryOpName(expr->op) << "\n";
            dumpTextbookExpr(expr->lhs.get(), out, indent + 1);
            dumpTextbookExpr(expr->rhs.get(), out, indent + 1);
            return;
    }
}

static void dumpTextbookStmtList(const vector<unique_ptr<Stmt>>& stmts, std::ostream& out, int indent);

static void dumpTextbookStmt(const Stmt* stmt, std::ostream& out, int indent) {
    if (!stmt) {
        return;
    }
    switch (stmt->kind) {
        case StmtKind::Assign:
            textbookIndent(out, indent);
            out << "StmtK Assign\n";
            textbookIndent(out, indent + 1);
            out << "ExpK " << varRefToTextbook(stmt->assignTarget.get()) << " IdV\n";
            dumpTextbookExpr(stmt->assignValue.get(), out, indent + 1);
            break;
        case StmtKind::If:
            textbookIndent(out, indent);
            out << "StmtK If\n";
            dumpTextbookExpr(stmt->condition.get(), out, indent + 1);
            textbookIndent(out, indent + 1);
            out << "ThenK\n";
            dumpTextbookStmtList(stmt->thenStmts, out, indent + 2);
            textbookIndent(out, indent + 1);
            out << "ElseK\n";
            dumpTextbookStmtList(stmt->elseStmts, out, indent + 2);
            break;
        case StmtKind::While:
            textbookIndent(out, indent);
            out << "StmtK While\n";
            dumpTextbookExpr(stmt->condition.get(), out, indent + 1);
            dumpTextbookStmtList(stmt->loopStmts, out, indent + 1);
            break;
        case StmtKind::Read:
            textbookIndent(out, indent);
            out << "StmtK Read " << varRefToTextbook(stmt->readVar.get()) << "\n";
            break;
        case StmtKind::Write:
            textbookIndent(out, indent);
            out << "StmtK Write\n";
            dumpTextbookExpr(stmt->writeExpr.get(), out, indent + 1);
            break;
        case StmtKind::Call:
            textbookIndent(out, indent);
            out << "StmtK Call " << stmt->callName << "\n";
            for (const auto& arg : stmt->callArgs) {
                dumpTextbookExpr(arg.get(), out, indent + 1);
            }
            break;
        case StmtKind::Return:
            textbookIndent(out, indent);
            out << "StmtK Return\n";
            if (stmt->returnExpr) {
                dumpTextbookExpr(stmt->returnExpr.get(), out, indent + 1);
            }
            break;
    }
}

static void dumpTextbookStmtList(const vector<unique_ptr<Stmt>>& stmts, std::ostream& out, int indent) {
    textbookIndent(out, indent);
    out << "StmLK\n";
    for (const auto& stmt : stmts) {
        dumpTextbookStmt(stmt.get(), out, indent + 1);
    }
}

static void dumpTextbookDeclPart(const DeclPart* decl, std::ostream& out, int indent) {
    if (!decl) {
        return;
    }

    if (!decl->typeDecls.empty()) {
        textbookIndent(out, indent);
        out << "TypeK\n";
        for (const auto& td : decl->typeDecls) {
            textbookIndent(out, indent + 1);
            out << "DecK " << typeToTextbookK(td->type.get()) << " " << td->name << "\n";
        }
    }

    if (!decl->varDecls.empty()) {
        textbookIndent(out, indent);
        out << "VarK\n";
        for (const auto& vd : decl->varDecls) {
            textbookIndent(out, indent + 1);
            out << "DecK " << typeToTextbookK(vd->type.get()) << " " << joinNames(vd->names) << "\n";
        }
    }

    for (const auto& pd : decl->procDecls) {
        textbookIndent(out, indent);
        out << "ProcDecK " << pd->name << "\n";
        if (!pd->params.empty()) {
            textbookIndent(out, indent + 1);
            out << "ParamK\n";
            for (const auto& param : pd->params) {
                textbookIndent(out, indent + 2);
                out << "DecK " << (param.byRef ? "var" : "value") << " param: "
                    << typeToTextbookK(param.type.get()) << " " << joinNames(param.names) << "\n";
            }
        }
        dumpTextbookDeclPart(pd->declPart.get(), out, indent + 1);
        dumpTextbookStmtList(pd->body, out, indent + 1);
    }
}

static string astToString(const ProgramAST& program) {
    std::ostringstream oss;
    oss << "ProK\n";
    textbookIndent(oss, 1);
    oss << "PheadK " << program.name << "\n";
    dumpTextbookDeclPart(program.declPart.get(), oss, 1);
    dumpTextbookStmtList(program.body, oss, 1);
    return oss.str();
}

