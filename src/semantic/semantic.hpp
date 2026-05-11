#pragma once

#include "common/shared.hpp"

enum class TypeKind {
    Integer,
    Char,
    Array,
    Record,
    Error
};

struct FieldInfo {
    string name;
    shared_ptr<TypeInfo> type;
    int offset = 0;
};

struct TypeInfo {
    TypeKind kind = TypeKind::Error;
    string displayName;
    int width = 4;
    int low = 0;
    int high = -1;
    shared_ptr<TypeInfo> elementType;
    vector<FieldInfo> fields;
};

enum class SymbolKind {
    TypeName,
    Variable,
    Procedure,
    Parameter
};

struct Scope;

struct Symbol {
    string name;
    SymbolKind kind = SymbolKind::Variable;
    SourcePos pos;
    shared_ptr<TypeInfo> type;
    bool byRef = false;
    int level = 0;
    int offset = 0;
    string label;
    ProcInfo* proc = nullptr;
    Scope* ownerScope = nullptr;
};

struct ProcInfo {
    Symbol* symbol = nullptr;
    Scope* scope = nullptr;
    int level = 0;
    int parentLevel = 0;
    vector<Symbol*> params;
    int localBytes = 0;
    string label;
    string endLabel;
};

struct Scope {
    Scope* parent = nullptr;
    int level = 0;
    ProcInfo* ownerProc = nullptr;
    unordered_map<string, unique_ptr<Symbol>> symbols;
    vector<unique_ptr<Scope>> children;
};

static int align4(int n) {
    if (n <= 0) return 4;
    return ((n + 3) / 4) * 4;
}

class SemanticAnalyzer {
public:
    struct Result {
        vector<Diagnostic> diagnostics;
        Scope* globalScope = nullptr;
    };

    Result analyze(ProgramAST& program) {
        diagnostics_.clear();
        procInfos_.clear();
        uniqueId_ = 0;

        globalScope_ = std::make_unique<Scope>();
        globalScope_->parent = nullptr;
        globalScope_->level = 0;
        globalScope_->ownerProc = nullptr;

        intType_ = std::make_shared<TypeInfo>();
        intType_->kind = TypeKind::Integer;
        intType_->displayName = "integer";
        intType_->width = 4;

        charType_ = std::make_shared<TypeInfo>();
        charType_->kind = TypeKind::Char;
        charType_->displayName = "char";
        charType_->width = 4;

        errorType_ = std::make_shared<TypeInfo>();
        errorType_->kind = TypeKind::Error;
        errorType_->displayName = "<error>";
        errorType_->width = 4;

        addBuiltinType("integer", intType_);
        addBuiltinType("char", charType_);

        analyzeDeclPart(program.declPart.get(), globalScope_.get());
        analyzeStmtList(program.body, globalScope_.get(), nullptr);

        Result result;
        result.diagnostics = diagnostics_;
        result.globalScope = globalScope_.get();
        return result;
    }

    Scope* globalScope() const { return globalScope_.get(); }
    const vector<unique_ptr<ProcInfo>>& procedures() const { return procInfos_; }
    const vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    shared_ptr<TypeInfo> errorType() const { return errorType_; }
    bool hasErrors() const { return !diagnostics_.empty(); }

private:
    vector<Diagnostic> diagnostics_;
    unique_ptr<Scope> globalScope_;
    vector<unique_ptr<ProcInfo>> procInfos_;

    shared_ptr<TypeInfo> intType_;
    shared_ptr<TypeInfo> charType_;
    shared_ptr<TypeInfo> errorType_;
    int uniqueId_ = 0;

    void report(const SourcePos& pos, const string& msg) {
        diagnostics_.push_back(Diagnostic{"Semantic", pos, msg});
    }

    void addBuiltinType(const string& name, const shared_ptr<TypeInfo>& type) {
        auto symbol = std::make_unique<Symbol>();
        symbol->name = name;
        symbol->kind = SymbolKind::TypeName;
        symbol->type = type;
        symbol->level = 0;
        symbol->ownerScope = globalScope_.get();
        globalScope_->symbols[name] = std::move(symbol);
    }

    Symbol* lookup(Scope* scope, const string& name) const {
        for (Scope* s = scope; s != nullptr; s = s->parent) {
            auto it = s->symbols.find(name);
            if (it != s->symbols.end()) {
                return it->second.get();
            }
        }
        return nullptr;
    }

    bool existsInCurrentScope(Scope* scope, const string& name) const {
        return scope->symbols.count(name) > 0;
    }

    Scope* createChildScope(Scope* parent, ProcInfo* ownerProc) {
        parent->children.push_back(std::make_unique<Scope>());
        Scope* child = parent->children.back().get();
        child->parent = parent;
        child->level = parent->level + 1;
        child->ownerProc = ownerProc;
        return child;
    }

    shared_ptr<TypeInfo> resolveTypeExpr(TypeExpr* typeExpr, Scope* scope) {
        if (!typeExpr) {
            return errorType_;
        }
        switch (typeExpr->kind) {
            case TypeExprKind::BaseInteger:
                return intType_;
            case TypeExprKind::BaseChar:
                return charType_;
            case TypeExprKind::Alias: {
                Symbol* sym = lookup(scope, typeExpr->aliasName);
                if (!sym) {
                    report(typeExpr->pos, "Undefined type alias '" + typeExpr->aliasName + "'");
                    return errorType_;
                }
                if (sym->kind != SymbolKind::TypeName) {
                    report(typeExpr->pos, "'" + typeExpr->aliasName + "' is not a type name");
                    return errorType_;
                }
                return sym->type;
            }
            case TypeExprKind::Array: {
                auto arr = std::make_shared<TypeInfo>();
                arr->kind = TypeKind::Array;
                arr->displayName = "array";
                arr->low = typeExpr->low;
                arr->high = typeExpr->high;
                arr->elementType = resolveTypeExpr(typeExpr->elemType.get(), scope);
                if (arr->low > arr->high) {
                    report(typeExpr->pos, "Invalid array bounds: low > high");
                    arr->width = 4;
                } else {
                    int count = arr->high - arr->low + 1;
                    int elemWidth = align4(arr->elementType ? arr->elementType->width : 4);
                    arr->width = std::max(4, count * elemWidth);
                }
                return arr;
            }
            case TypeExprKind::Record: {
                auto rec = std::make_shared<TypeInfo>();
                rec->kind = TypeKind::Record;
                rec->displayName = "record";
                rec->width = 0;
                set<string> used;
                for (auto& field : typeExpr->fields) {
                    auto fieldType = resolveTypeExpr(field.type.get(), scope);
                    for (const auto& name : field.names) {
                        if (used.count(name)) {
                            report(field.pos, "Duplicate record field '" + name + "'");
                            continue;
                        }
                        used.insert(name);
                        FieldInfo fi;
                        fi.name = name;
                        fi.type = fieldType;
                        fi.offset = rec->width;
                        rec->fields.push_back(fi);
                        rec->width += align4(fieldType ? fieldType->width : 4);
                    }
                }
                if (rec->width <= 0) rec->width = 4;
                return rec;
            }
        }
        return errorType_;
    }

    bool sameType(const shared_ptr<TypeInfo>& a, const shared_ptr<TypeInfo>& b) const {
        if (!a || !b) return false;
        if (a->kind == TypeKind::Error || b->kind == TypeKind::Error) return true;
        if (a.get() == b.get()) return true;
        if (a->kind != b->kind) return false;
        if (a->kind == TypeKind::Array) {
            return a->low == b->low &&
                   a->high == b->high &&
                   sameType(a->elementType, b->elementType);
        }
        if (a->kind == TypeKind::Record) {
            if (a->fields.size() != b->fields.size()) return false;
            for (size_t i = 0; i < a->fields.size(); ++i) {
                if (a->fields[i].name != b->fields[i].name) return false;
                if (!sameType(a->fields[i].type, b->fields[i].type)) return false;
            }
            return true;
        }
        return false;
    }

    bool isInteger(const shared_ptr<TypeInfo>& t) const {
        return t && t->kind == TypeKind::Integer;
    }

    bool isScalar(const shared_ptr<TypeInfo>& t) const {
        return t && (t->kind == TypeKind::Integer || t->kind == TypeKind::Char);
    }

    void analyzeDeclPart(DeclPart* declPart, Scope* scope) {
        if (!declPart) return;

        for (auto& td : declPart->typeDecls) {
            if (existsInCurrentScope(scope, td->name)) {
                report(td->pos, "Duplicate declaration: '" + td->name + "'");
                continue;
            }
            auto sym = std::make_unique<Symbol>();
            sym->name = td->name;
            sym->kind = SymbolKind::TypeName;
            sym->pos = td->pos;
            sym->type = resolveTypeExpr(td->type.get(), scope);
            sym->level = scope->level;
            sym->ownerScope = scope;
            scope->symbols[sym->name] = std::move(sym);
        }

        for (auto& vd : declPart->varDecls) {
            auto varType = resolveTypeExpr(vd->type.get(), scope);
            for (const auto& name : vd->names) {
                if (existsInCurrentScope(scope, name)) {
                    report(vd->pos, "Duplicate declaration: '" + name + "'");
                    continue;
                }
                auto sym = std::make_unique<Symbol>();
                sym->name = name;
                sym->kind = SymbolKind::Variable;
                sym->pos = vd->pos;
                sym->type = varType;
                sym->level = scope->level;
                sym->ownerScope = scope;
                if (scope->level == 0) {
                    sym->label = "g_" + name + "_" + std::to_string(uniqueId_++);
                } else if (scope->ownerProc) {
                    int size = align4(varType ? varType->width : 4);
                    scope->ownerProc->localBytes += size;
                    sym->offset = -scope->ownerProc->localBytes;
                }
                scope->symbols[name] = std::move(sym);
            }
        }

        for (auto& pd : declPart->procDecls) {
            if (existsInCurrentScope(scope, pd->name)) {
                report(pd->pos, "Duplicate declaration: '" + pd->name + "'");
                continue;
            }
            auto procInfo = std::make_unique<ProcInfo>();
            procInfo->label = "proc_" + pd->name + "_" + std::to_string(uniqueId_++);
            procInfo->endLabel = procInfo->label + "_end";
            procInfo->parentLevel = scope->level;
            ProcInfo* rawProc = procInfo.get();
            procInfos_.push_back(std::move(procInfo));

            auto sym = std::make_unique<Symbol>();
            sym->name = pd->name;
            sym->kind = SymbolKind::Procedure;
            sym->pos = pd->pos;
            sym->level = scope->level;
            sym->label = rawProc->label;
            sym->proc = rawProc;
            sym->ownerScope = scope;
            rawProc->symbol = sym.get();
            scope->symbols[pd->name] = std::move(sym);

            pd->symbol = scope->symbols[pd->name].get();
            pd->procInfo = rawProc;
        }

        for (auto& pd : declPart->procDecls) {
            if (!pd->procInfo) continue;
            prepareProcedureHeader(*pd, scope);
        }

        for (auto& pd : declPart->procDecls) {
            if (!pd->procInfo) continue;
            analyzeProcedureBody(*pd);
        }
    }

    void prepareProcedureHeader(ProcDecl& procDecl, Scope* parentScope) {
        ProcInfo* info = procDecl.procInfo;
        Scope* procScope = createChildScope(parentScope, info);
        info->scope = procScope;
        info->level = procScope->level;
        info->parentLevel = parentScope->level;

        int nextParamOffset = 12;
        for (auto& p : procDecl.params) {
            auto pType = resolveTypeExpr(p.type.get(), procScope);
            for (const auto& name : p.names) {
                if (existsInCurrentScope(procScope, name)) {
                    report(p.pos, "Duplicate parameter name '" + name + "'");
                    continue;
                }
                auto sym = std::make_unique<Symbol>();
                sym->name = name;
                sym->kind = SymbolKind::Parameter;
                sym->pos = p.pos;
                sym->type = pType;
                sym->byRef = p.byRef;
                sym->level = procScope->level;
                sym->offset = nextParamOffset;
                sym->ownerScope = procScope;
                nextParamOffset += 4;
                if (!p.byRef && pType && !isScalar(pType)) {
                    report(p.pos, "By-value parameter '" + name + "' must be scalar for code generation");
                }
                Symbol* raw = sym.get();
                procScope->symbols[name] = std::move(sym);
                info->params.push_back(raw);
            }
        }
    }

    void analyzeProcedureBody(ProcDecl& procDecl) {
        ProcInfo* info = procDecl.procInfo;
        if (!info || !info->scope) return;
        Scope* procScope = info->scope;
        analyzeDeclPart(procDecl.declPart.get(), procScope);
        analyzeStmtList(procDecl.body, procScope, info);
    }

    const FieldInfo* findField(const shared_ptr<TypeInfo>& type, const string& fieldName) const {
        if (!type || type->kind != TypeKind::Record) {
            return nullptr;
        }
        for (const auto& f : type->fields) {
            if (f.name == fieldName) {
                return &f;
            }
        }
        return nullptr;
    }

    shared_ptr<TypeInfo> analyzeVarRef(VarRef* var, Scope* scope) {
        if (!var) return errorType_;
        Symbol* sym = lookup(scope, var->name);
        if (!sym) {
            report(var->pos, "Undefined identifier '" + var->name + "'");
            var->resolvedType = errorType_;
            return errorType_;
        }
        if (!(sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter)) {
            report(var->pos, "'" + var->name + "' is not a variable");
            var->resolvedType = errorType_;
            return errorType_;
        }

        var->symbol = sym;
        shared_ptr<TypeInfo> curType = sym->type ? sym->type : errorType_;

        if (var->access == VarRef::AccessKind::Index) {
            if (!curType || curType->kind != TypeKind::Array) {
                report(var->pos, "Variable '" + var->name + "' is not an array");
                curType = errorType_;
            } else {
                auto idxType = analyzeExpr(var->indexExpr.get(), scope);
                if (!isInteger(idxType)) {
                    report(var->pos, "Array index must be integer");
                }
                curType = curType->elementType ? curType->elementType : errorType_;
            }
        } else if (var->access == VarRef::AccessKind::Field) {
            if (!curType || curType->kind != TypeKind::Record) {
                report(var->pos, "Variable '" + var->name + "' is not a record");
                curType = errorType_;
            } else {
                const FieldInfo* field = findField(curType, var->fieldName);
                if (!field) {
                    report(var->pos, "Unknown record field '" + var->fieldName + "'");
                    curType = errorType_;
                } else {
                    curType = field->type;
                    if (var->fieldIndexExpr) {
                        if (!curType || curType->kind != TypeKind::Array) {
                            report(var->pos, "Field '" + var->fieldName + "' is not an array");
                            curType = errorType_;
                        } else {
                            auto idxType = analyzeExpr(var->fieldIndexExpr.get(), scope);
                            if (!isInteger(idxType)) {
                                report(var->pos, "Array index must be integer");
                            }
                            curType = curType->elementType ? curType->elementType : errorType_;
                        }
                    }
                }
            }
        }

        var->resolvedType = curType;
        return curType;
    }

    shared_ptr<TypeInfo> analyzeExpr(Expr* expr, Scope* scope) {
        if (!expr) return errorType_;
        switch (expr->kind) {
            case ExprKind::IntConst:
                expr->inferredType = intType_;
                expr->isLValue = false;
                return intType_;
            case ExprKind::CharConst:
                expr->inferredType = charType_;
                expr->isLValue = false;
                return charType_;
            case ExprKind::Var: {
                auto t = analyzeVarRef(expr->var.get(), scope);
                expr->inferredType = t;
                expr->isLValue = true;
                return t;
            }
            case ExprKind::Binary: {
                auto lt = analyzeExpr(expr->lhs.get(), scope);
                auto rt = analyzeExpr(expr->rhs.get(), scope);
                expr->isLValue = false;
                switch (expr->op) {
                    case BinaryOp::Add:
                    case BinaryOp::Sub:
                    case BinaryOp::Mul:
                    case BinaryOp::Div:
                        if (!isInteger(lt) || !isInteger(rt)) {
                            report(expr->pos, "Arithmetic operators require integer operands");
                        }
                        expr->inferredType = intType_;
                        return intType_;
                    case BinaryOp::Lt:
                        if (!isInteger(lt) || !isInteger(rt)) {
                            report(expr->pos, "'<' requires integer operands");
                        }
                        expr->inferredType = intType_;
                        return intType_;
                    case BinaryOp::Eq:
                        if (!sameType(lt, rt)) {
                            report(expr->pos, "'=' requires operands of same type");
                        }
                        expr->inferredType = intType_;
                        return intType_;
                }
                break;
            }
        }
        expr->inferredType = errorType_;
        return errorType_;
    }

    void analyzeStmt(Stmt* stmt, Scope* scope, ProcInfo* currentProc) {
        if (!stmt) return;
        switch (stmt->kind) {
            case StmtKind::Assign: {
                auto lt = analyzeVarRef(stmt->assignTarget.get(), scope);
                auto rt = analyzeExpr(stmt->assignValue.get(), scope);
                if (!sameType(lt, rt)) {
                    report(stmt->pos, "Type mismatch in assignment");
                }
                break;
            }
            case StmtKind::If: {
                auto ct = analyzeExpr(stmt->condition.get(), scope);
                if (!isInteger(ct)) {
                    report(stmt->pos, "Condition of if must be integer/bool");
                }
                analyzeStmtList(stmt->thenStmts, scope, currentProc);
                analyzeStmtList(stmt->elseStmts, scope, currentProc);
                break;
            }
            case StmtKind::While: {
                auto ct = analyzeExpr(stmt->condition.get(), scope);
                if (!isInteger(ct)) {
                    report(stmt->pos, "Condition of while must be integer/bool");
                }
                analyzeStmtList(stmt->loopStmts, scope, currentProc);
                break;
            }
            case StmtKind::Read: {
                auto t = analyzeVarRef(stmt->readVar.get(), scope);
                if (!isScalar(t)) {
                    report(stmt->pos, "read() requires scalar variable");
                }
                break;
            }
            case StmtKind::Write: {
                auto t = analyzeExpr(stmt->writeExpr.get(), scope);
                if (!isScalar(t)) {
                    report(stmt->pos, "write() requires scalar expression");
                }
                break;
            }
            case StmtKind::Call: {
                Symbol* sym = lookup(scope, stmt->callName);
                if (!sym) {
                    report(stmt->pos, "Undefined procedure '" + stmt->callName + "'");
                    for (auto& arg : stmt->callArgs) {
                        analyzeExpr(arg.get(), scope);
                    }
                    break;
                }
                if (sym->kind != SymbolKind::Procedure || !sym->proc) {
                    report(stmt->pos, "'" + stmt->callName + "' is not a procedure");
                    for (auto& arg : stmt->callArgs) {
                        analyzeExpr(arg.get(), scope);
                    }
                    break;
                }
                stmt->callSymbol = sym;
                const auto& params = sym->proc->params;
                if (params.size() != stmt->callArgs.size()) {
                    report(stmt->pos, "Argument count mismatch calling '" + stmt->callName + "'");
                }
                size_t n = std::min(params.size(), stmt->callArgs.size());
                for (size_t i = 0; i < n; ++i) {
                    auto argType = analyzeExpr(stmt->callArgs[i].get(), scope);
                    auto paramType = params[i]->type;
                    if (!sameType(argType, paramType)) {
                        report(stmt->pos, "Argument type mismatch at position " + std::to_string(i + 1));
                    }
                    if (params[i]->byRef) {
                        if (stmt->callArgs[i]->kind != ExprKind::Var) {
                            report(stmt->pos, "var parameter requires variable argument at position " + std::to_string(i + 1));
                        }
                    }
                }
                for (size_t i = n; i < stmt->callArgs.size(); ++i) {
                    analyzeExpr(stmt->callArgs[i].get(), scope);
                }
                break;
            }
            case StmtKind::Return: {
                if (currentProc == nullptr) {
                    report(stmt->pos, "return statement is only allowed inside procedures");
                }
                if (stmt->returnExpr) {
                    analyzeExpr(stmt->returnExpr.get(), scope);
                }
                break;
            }
        }
    }

    void analyzeStmtList(vector<unique_ptr<Stmt>>& stmts, Scope* scope, ProcInfo* currentProc) {
        for (auto& stmt : stmts) {
            analyzeStmt(stmt.get(), scope, currentProc);
        }
    }
};
