#include "codegen/codegen.hpp"

class RegisterPool {
public:
    RegisterPool() {
        regs_ = {"$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8", "$t9"};
        used_.assign(regs_.size(), false);
    }

    string acquire() {
        for (size_t i = 0; i < regs_.size(); ++i) {
            if (!used_[i]) {
                used_[i] = true;
                return regs_[i];
            }
        }
        throw std::runtime_error("Out of temporary registers");
    }

    void release(const string& reg) {
        for (size_t i = 0; i < regs_.size(); ++i) {
            if (regs_[i] == reg) {
                used_[i] = false;
                return;
            }
        }
    }

private:
    vector<string> regs_;
    vector<bool> used_;
};

class MipsCodeGenerator {
public:
    struct Result {
        string code;
        vector<Diagnostic> diagnostics;
    };

    MipsCodeGenerator(ProgramAST& program, SemanticAnalyzer& semantic)
        : program_(program), semantic_(semantic) {}

    Result generate() {
        data_.clear();
        text_.clear();
        diagnostics_.clear();
        labelCounter_ = 0;
        regs_ = RegisterPool();

        Scope* global = semantic_.globalScope();
        if (!global) {
            diagnostics_.push_back(Diagnostic{
                "CodeGen", SourcePos{1, 1}, "No semantic scope information found"
            });
            return {"", diagnostics_};
        }

        emitData(".data");
        emitData("__newline: .asciiz \"\\n\"");
        emitData(".align 2");
        for (const auto& kv : global->symbols) {
            const Symbol* sym = kv.second.get();
            if (sym->kind == SymbolKind::Variable) {
                int size = align4(sym->type ? sym->type->width : 4);
                if (size <= 0) size = 4;
                emitData(sym->label + ": .space " + std::to_string(size));
            }
        }
        emitData("");

        emitText(".text");
        emitText(".globl main");
        emitText("");

        emitText("main:");
        emitText("  move $fp, $sp");
        GenContext mainCtx;
        mainCtx.proc = nullptr;
        for (auto& stmt : program_.body) {
            generateStmt(*stmt, mainCtx);
        }
        emitText("  li $v0, 10");
        emitText("  syscall");
        emitText("");

        for (auto& proc : program_.declPart->procDecls) {
            generateProcedure(*proc);
        }

        std::ostringstream oss;
        for (const auto& line : data_) {
            oss << line << "\n";
        }
        for (const auto& line : text_) {
            oss << line << "\n";
        }
        return {oss.str(), diagnostics_};
    }

private:
    struct GenContext {
        ProcInfo* proc = nullptr;
    };

    ProgramAST& program_;
    SemanticAnalyzer& semantic_;
    vector<string> data_;
    vector<string> text_;
    vector<Diagnostic> diagnostics_;
    RegisterPool regs_;
    int labelCounter_ = 0;

    void emitData(const string& line) { data_.push_back(line); }
    void emitText(const string& line) { text_.push_back(line); }

    string newLabel(const string& prefix) {
        return prefix + "_" + std::to_string(labelCounter_++);
    }

    void report(const SourcePos& pos, const string& msg) {
        diagnostics_.push_back(Diagnostic{"CodeGen", pos, msg});
    }

    int currentLevel(const GenContext& ctx) const {
        if (!ctx.proc) return 0;
        return ctx.proc->level;
    }

    const FieldInfo* findField(const shared_ptr<TypeInfo>& rec, const string& name) const {
        if (!rec || rec->kind != TypeKind::Record) return nullptr;
        for (const auto& f : rec->fields) {
            if (f.name == name) return &f;
        }
        return nullptr;
    }

    string framePointerForLevel(int targetLevel, const GenContext& ctx, const SourcePos& pos) {
        string reg = regs_.acquire();
        if (!ctx.proc) {
            report(pos, "Cannot access non-global symbol from global context");
            emitText("  move " + reg + ", $zero");
            return reg;
        }
        emitText("  move " + reg + ", $fp");
        int steps = currentLevel(ctx) - targetLevel;
        if (steps < 0) {
            report(pos, "Invalid lexical level access");
            steps = 0;
        }
        for (int i = 0; i < steps; ++i) {
            emitText("  lw " + reg + ", 8(" + reg + ")");
        }
        return reg;
    }

    string generateVarAddress(const VarRef& var, const GenContext& ctx) {
        string addrReg = regs_.acquire();
        if (!var.symbol) {
            report(var.pos, "Missing symbol info for variable '" + var.name + "'");
            emitText("  move " + addrReg + ", $zero");
            return addrReg;
        }

        const Symbol* sym = var.symbol;
        shared_ptr<TypeInfo> curType = sym->type;

        if (sym->level == 0) {
            emitText("  la " + addrReg + ", " + sym->label);
        } else {
            string frameReg = framePointerForLevel(sym->level, ctx, var.pos);
            if (sym->kind == SymbolKind::Parameter && sym->byRef) {
                emitText("  lw " + addrReg + ", " + std::to_string(sym->offset) + "(" + frameReg + ")");
            } else {
                emitText("  addiu " + addrReg + ", " + frameReg + ", " + std::to_string(sym->offset));
            }
            regs_.release(frameReg);
        }

        if (var.access == VarRef::AccessKind::Index) {
            if (!curType || curType->kind != TypeKind::Array) {
                report(var.pos, "Indexing non-array variable '" + var.name + "'");
            } else {
                string idxReg = generateExpr(*var.indexExpr, ctx);
                if (curType->low != 0) {
                    emitText("  addiu " + idxReg + ", " + idxReg + ", " + std::to_string(-curType->low));
                }
                int elemWidth = align4(curType->elementType ? curType->elementType->width : 4);
                if (elemWidth != 1) {
                    string tmp = regs_.acquire();
                    emitText("  li " + tmp + ", " + std::to_string(elemWidth));
                    emitText("  mul " + idxReg + ", " + idxReg + ", " + tmp);
                    regs_.release(tmp);
                }
                emitText("  addu " + addrReg + ", " + addrReg + ", " + idxReg);
                regs_.release(idxReg);
                curType = curType->elementType;
            }
        } else if (var.access == VarRef::AccessKind::Field) {
            if (!curType || curType->kind != TypeKind::Record) {
                report(var.pos, "Field access on non-record variable '" + var.name + "'");
            } else {
                const FieldInfo* field = findField(curType, var.fieldName);
                if (!field) {
                    report(var.pos, "Unknown field '" + var.fieldName + "'");
                } else {
                    if (field->offset != 0) {
                        emitText("  addiu " + addrReg + ", " + addrReg + ", " + std::to_string(field->offset));
                    }
                    curType = field->type;
                    if (var.fieldIndexExpr) {
                        if (!curType || curType->kind != TypeKind::Array) {
                            report(var.pos, "Field '" + var.fieldName + "' is not an array");
                        } else {
                            string idxReg = generateExpr(*var.fieldIndexExpr, ctx);
                            if (curType->low != 0) {
                                emitText("  addiu " + idxReg + ", " + idxReg + ", " + std::to_string(-curType->low));
                            }
                            int elemWidth = align4(curType->elementType ? curType->elementType->width : 4);
                            if (elemWidth != 1) {
                                string tmp = regs_.acquire();
                                emitText("  li " + tmp + ", " + std::to_string(elemWidth));
                                emitText("  mul " + idxReg + ", " + idxReg + ", " + tmp);
                                regs_.release(tmp);
                            }
                            emitText("  addu " + addrReg + ", " + addrReg + ", " + idxReg);
                            regs_.release(idxReg);
                            curType = curType->elementType;
                        }
                    }
                }
            }
        }

        return addrReg;
    }

    string generateExpr(const Expr& expr, const GenContext& ctx) {
        switch (expr.kind) {
            case ExprKind::IntConst: {
                string reg = regs_.acquire();
                emitText("  li " + reg + ", " + std::to_string(expr.intValue));
                return reg;
            }
            case ExprKind::CharConst: {
                string reg = regs_.acquire();
                emitText("  li " + reg + ", " + std::to_string(static_cast<int>(static_cast<unsigned char>(expr.charValue))));
                return reg;
            }
            case ExprKind::Var: {
                string addr = generateVarAddress(*expr.var, ctx);
                string val = regs_.acquire();
                emitText("  lw " + val + ", 0(" + addr + ")");
                regs_.release(addr);
                return val;
            }
            case ExprKind::Binary: {
                string lhs = generateExpr(*expr.lhs, ctx);
                string rhs = generateExpr(*expr.rhs, ctx);
                switch (expr.op) {
                    case BinaryOp::Add:
                        emitText("  addu " + lhs + ", " + lhs + ", " + rhs);
                        break;
                    case BinaryOp::Sub:
                        emitText("  subu " + lhs + ", " + lhs + ", " + rhs);
                        break;
                    case BinaryOp::Mul:
                        emitText("  mul " + lhs + ", " + lhs + ", " + rhs);
                        break;
                    case BinaryOp::Div:
                        emitText("  div " + lhs + ", " + rhs);
                        emitText("  mflo " + lhs);
                        break;
                    case BinaryOp::Lt:
                        emitText("  slt " + lhs + ", " + lhs + ", " + rhs);
                        break;
                    case BinaryOp::Eq:
                        emitText("  xor " + lhs + ", " + lhs + ", " + rhs);
                        emitText("  sltiu " + lhs + ", " + lhs + ", 1");
                        break;
                }
                regs_.release(rhs);
                return lhs;
            }
        }
        string reg = regs_.acquire();
        emitText("  move " + reg + ", $zero");
        return reg;
    }

    void generateCall(const Stmt& stmt, const GenContext& ctx) {
        if (!stmt.callSymbol || !stmt.callSymbol->proc) {
            report(stmt.pos, "Missing procedure symbol for call '" + stmt.callName + "'");
            return;
        }
        ProcInfo* callee = stmt.callSymbol->proc;
        size_t n = std::min(callee->params.size(), stmt.callArgs.size());

        for (size_t i = n; i > 0; --i) {
            size_t idx = i - 1;
            Symbol* param = callee->params[idx];
            string argReg;
            if (param->byRef) {
                if (stmt.callArgs[idx]->kind == ExprKind::Var && stmt.callArgs[idx]->var) {
                    argReg = generateVarAddress(*stmt.callArgs[idx]->var, ctx);
                } else {
                    report(stmt.pos, "Argument for var parameter must be variable");
                    argReg = regs_.acquire();
                    emitText("  move " + argReg + ", $zero");
                }
            } else {
                argReg = generateExpr(*stmt.callArgs[idx], ctx);
            }
            emitText("  addiu $sp, $sp, -4");
            emitText("  sw " + argReg + ", 0($sp)");
            regs_.release(argReg);
        }

        string slReg = regs_.acquire();
        if (callee->parentLevel == 0) {
            emitText("  move " + slReg + ", $zero");
        } else if (!ctx.proc) {
            report(stmt.pos, "Cannot compute static link from global context");
            emitText("  move " + slReg + ", $zero");
        } else {
            emitText("  move " + slReg + ", $fp");
            int steps = ctx.proc->level - callee->parentLevel;
            if (steps < 0) {
                report(stmt.pos, "Invalid static link level");
                steps = 0;
            }
            for (int i = 0; i < steps; ++i) {
                emitText("  lw " + slReg + ", 8(" + slReg + ")");
            }
        }
        emitText("  addiu $sp, $sp, -4");
        emitText("  sw " + slReg + ", 0($sp)");
        regs_.release(slReg);

        emitText("  jal " + callee->label);
        emitText("  addiu $sp, $sp, " + std::to_string(static_cast<int>((n + 1) * 4)));
    }

    void generateStmt(const Stmt& stmt, const GenContext& ctx) {
        switch (stmt.kind) {
            case StmtKind::Assign: {
                string valueReg = generateExpr(*stmt.assignValue, ctx);
                string addrReg = generateVarAddress(*stmt.assignTarget, ctx);
                emitText("  sw " + valueReg + ", 0(" + addrReg + ")");
                regs_.release(valueReg);
                regs_.release(addrReg);
                break;
            }
            case StmtKind::If: {
                string condReg = generateExpr(*stmt.condition, ctx);
                string elseLabel = newLabel("if_else");
                string endLabel = newLabel("if_end");
                emitText("  beq " + condReg + ", $zero, " + elseLabel);
                regs_.release(condReg);
                for (const auto& s : stmt.thenStmts) {
                    generateStmt(*s, ctx);
                }
                emitText("  j " + endLabel);
                emitText(elseLabel + ":");
                for (const auto& s : stmt.elseStmts) {
                    generateStmt(*s, ctx);
                }
                emitText(endLabel + ":");
                break;
            }
            case StmtKind::While: {
                string beginLabel = newLabel("while_begin");
                string endLabel = newLabel("while_end");
                emitText(beginLabel + ":");
                string condReg = generateExpr(*stmt.condition, ctx);
                emitText("  beq " + condReg + ", $zero, " + endLabel);
                regs_.release(condReg);
                for (const auto& s : stmt.loopStmts) {
                    generateStmt(*s, ctx);
                }
                emitText("  j " + beginLabel);
                emitText(endLabel + ":");
                break;
            }
            case StmtKind::Read: {
                string addrReg = generateVarAddress(*stmt.readVar, ctx);
                emitText("  li $v0, 5");
                emitText("  syscall");
                emitText("  sw $v0, 0(" + addrReg + ")");
                regs_.release(addrReg);
                break;
            }
            case StmtKind::Write: {
                string valReg = generateExpr(*stmt.writeExpr, ctx);
                emitText("  move $a0, " + valReg);
                emitText("  li $v0, 1");
                emitText("  syscall");
                emitText("  la $a0, __newline");
                emitText("  li $v0, 4");
                emitText("  syscall");
                regs_.release(valReg);
                break;
            }
            case StmtKind::Call:
                generateCall(stmt, ctx);
                break;
            case StmtKind::Return:
                if (ctx.proc) {
                    emitText("  j " + ctx.proc->endLabel);
                }
                break;
        }
    }

    void generateProcedure(ProcDecl& procDecl) {
        for (auto& nested : procDecl.declPart->procDecls) {
            generateProcedure(*nested);
        }

        if (!procDecl.procInfo) {
            report(procDecl.pos, "Procedure missing semantic info: " + procDecl.name);
            return;
        }
        ProcInfo* info = procDecl.procInfo;

        emitText(info->label + ":");
        emitText("  addiu $sp, $sp, -8");
        emitText("  sw $fp, 0($sp)");
        emitText("  sw $ra, 4($sp)");
        emitText("  move $fp, $sp");
        if (info->localBytes > 0) {
            emitText("  addiu $sp, $sp, -" + std::to_string(align4(info->localBytes)));
        }

        GenContext ctx;
        ctx.proc = info;
        for (auto& stmt : procDecl.body) {
            generateStmt(*stmt, ctx);
        }

        emitText(info->endLabel + ":");
        emitText("  move $sp, $fp");
        emitText("  lw $fp, 0($sp)");
        emitText("  lw $ra, 4($sp)");
        emitText("  addiu $sp, $sp, 8");
        emitText("  jr $ra");
        emitText("");
    }
};

CodegenResult runCodegen(ProgramAST& program, SemanticAnalyzer& semantic) {
    CodegenResult out;
    MipsCodeGenerator codegen(program, semantic);
    auto cgResult = codegen.generate();
    out.code = std::move(cgResult.code);
    out.diagnostics = std::move(cgResult.diagnostics);
    out.generated = out.diagnostics.empty();
    return out;
}
