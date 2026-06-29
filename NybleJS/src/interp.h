#pragma once
#include <chrono>
#include <random>
#include "ast.h"
#include "env.h"
#include "builtins.h"

namespace nyble {

class Interpreter {
public:
    std::shared_ptr<Environment> globalEnv;
    std::shared_ptr<Environment> currentEnv;

    struct ReturnSignal { Value value; };
    struct BreakSignal {};
    struct ContinueSignal {};
    struct ThrowSignal { Value value; };

    Interpreter() {
        globalEnv = std::make_shared<Environment>();
        currentEnv = globalEnv;
        installBuiltins(globalEnv);
    }

    Value callValue(const Value& fn, const std::vector<Value>& args);

    Value evaluate(const Program& prog) {
        Value result;
        for (const auto& stmt : prog.stmts) {
            result = execute(stmt.get());
        }
        return result;
    }

    Value execute(Stmt* stmt) {
        if (!stmt) return Value::makeUndefined();
        switch (stmt->type) {
            case ASTType::Block: return executeBlock(static_cast<BlockStmt*>(stmt));
            case ASTType::ExprStmt: return executeExpr(static_cast<ExprStmtNode*>(stmt));
            case ASTType::VarDecl: return executeVarDecl(static_cast<VarDeclNode*>(stmt));
            case ASTType::FunDecl: return executeFunDecl(static_cast<FunDeclNode*>(stmt));
            case ASTType::If: return executeIf(static_cast<IfNode*>(stmt));
            case ASTType::While: return executeWhile(static_cast<WhileNode*>(stmt));
            case ASTType::DoWhile: return executeDoWhile(static_cast<DoWhileNode*>(stmt));
            case ASTType::For: return executeFor(static_cast<ForNode*>(stmt));
            case ASTType::Return: return executeReturn(static_cast<ReturnNode*>(stmt));
            case ASTType::Break: throw BreakSignal{};
            case ASTType::Continue: throw ContinueSignal{};
            case ASTType::Switch: return executeSwitch(static_cast<SwitchNode*>(stmt));
            case ASTType::Throw: return executeThrow(static_cast<ThrowNode*>(stmt));
            case ASTType::Try: return executeTry(static_cast<TryNode*>(stmt));
            default: return Value::makeUndefined();
        }
    }

    Value evaluateExpr(Expr* expr) {
        if (!expr) return Value::makeUndefined();
        switch (expr->type) {
            case ASTType::Binary: return evalBinary(static_cast<BinaryExprNode*>(expr));
            case ASTType::Unary: return evalUnary(static_cast<UnaryExprNode*>(expr));
            case ASTType::Call: return evalCall(static_cast<CallExprNode*>(expr));
            case ASTType::Member: return evalMember(static_cast<MemberExprNode*>(expr));
            case ASTType::Identifier: return evalIdentifier(static_cast<IdentifierNode*>(expr));
            case ASTType::Literal: return static_cast<LiteralNode*>(expr)->value;
            case ASTType::ArrayLit: return evalArray(static_cast<ArrayLitNode*>(expr));
            case ASTType::ObjectLit: return evalObject(static_cast<ObjectLitNode*>(expr));
            case ASTType::Assignment: return evalAssign(static_cast<AssignNode*>(expr));
            case ASTType::Conditional: return evalConditional(static_cast<ConditionalNode*>(expr));
            case ASTType::ArrowFunc: return evalArrow(static_cast<ArrowFuncNode*>(expr));
            default: return Value::makeUndefined();
        }
    }

private:
    std::mt19937 rng{std::random_device{}()};

    Value executeBlock(BlockStmt* block) {
        auto prev = currentEnv;
        currentEnv = currentEnv->createChild();
        Value result;
        for (const auto& s : block->stmts) {
            result = execute(s.get());
        }
        currentEnv = prev;
        return result;
    }

    Value executeExpr(ExprStmtNode* stmt) {
        return evaluateExpr(stmt->expr.get());
    }

    Value executeVarDecl(VarDeclNode* decl) {
        Value val = Value::makeUndefined();
        if (decl->initializer) val = evaluateExpr(decl->initializer.get());
        currentEnv->define(decl->name, val, decl->isConst);
        return val;
    }

    Value executeFunDecl(FunDeclNode* decl) {
        auto funcData = gHeap.allocate<GCFunction>();
        funcData->params = decl->params;
        funcData->body = decl->body.get();
        funcData->closure = currentEnv;
        currentEnv->define(decl->name, Value::makeFunc(funcData));
        return Value::makeFunc(funcData);
    }

    Value executeIf(IfNode* stmt) {
        Value cond = evaluateExpr(stmt->cond.get());
        if (cond.isTruthy()) return execute(stmt->thenBranch.get());
        if (stmt->elseBranch) return execute(stmt->elseBranch.get());
        return Value::makeUndefined();
    }

    Value executeWhile(WhileNode* stmt) {
        Value result;
        while (true) {
            Value cond = evaluateExpr(stmt->cond.get());
            if (!cond.isTruthy()) break;
            try { result = execute(stmt->body.get()); }
            catch (const BreakSignal&) { break; }
            catch (const ContinueSignal&) { continue; }
        }
        return result;
    }

    Value executeDoWhile(DoWhileNode* stmt) {
        Value result;
        do {
            try { result = execute(stmt->body.get()); }
            catch (const BreakSignal&) { break; }
            catch (const ContinueSignal&) { continue; }
        } while (evaluateExpr(stmt->cond.get()).isTruthy());
        return result;
    }

    Value executeFor(ForNode* stmt) {
        auto prev = currentEnv;
        currentEnv = currentEnv->createChild();
        if (stmt->init) execute(stmt->init.get());
        Value result;
        while (true) {
            if (stmt->cond) {
                Value cond = evaluateExpr(stmt->cond.get());
                if (!cond.isTruthy()) break;
            }
            try { result = execute(stmt->body.get()); }
            catch (const BreakSignal&) { break; }
            catch (const ContinueSignal&) { if (stmt->inc) evaluateExpr(stmt->inc.get()); continue; }
            if (stmt->inc) evaluateExpr(stmt->inc.get());
        }
        currentEnv = prev;
        return result;
    }

    Value executeReturn(ReturnNode* stmt) {
        Value val = Value::makeUndefined();
        if (stmt->value) val = evaluateExpr(stmt->value.get());
        throw ReturnSignal{val};
    }

    Value executeSwitch(SwitchNode* stmt) {
        Value expr = evaluateExpr(stmt->expr.get());
        bool matched = false;
        Value result;
        for (const auto& caseEntry : stmt->cases) {
            if (!matched) {
                Value caseVal = evaluateExpr(caseEntry.first.get());
                Value eq = expr.eq(caseVal, true);
                if (eq.type == ValueType::Boolean && eq.boolVal) matched = true;
                else continue;
            }
            for (const auto& s : caseEntry.second) {
                try { result = execute(s.get()); }
                catch (const BreakSignal&) { return result; }
            }
        }
        if (!matched) {
            for (const auto& s : stmt->defaultCase) {
                try { result = execute(s.get()); }
                catch (const BreakSignal&) { return result; }
            }
        }
        return result;
    }

    Value executeThrow(ThrowNode* stmt) {
        Value val = Value::makeUndefined();
        if (stmt->value) val = evaluateExpr(stmt->value.get());
        throw ThrowSignal{val};
    }

    Value executeTry(TryNode* stmt) {
        Value result;
        bool threw = false;
        Value thrownValue;
        bool hasReturn = false;
        ReturnSignal retSig{Value::makeUndefined()};
        bool hasBreak = false;
        BreakSignal brkSig;
        bool hasContinue = false;
        ContinueSignal contSig;

        {
            auto prev = currentEnv;
            currentEnv = currentEnv->createChild();
            try {
                for (const auto& s : stmt->tryBlock->stmts)
                    result = execute(s.get());
            } catch (const ThrowSignal& ts) {
                threw = true;
                thrownValue = ts.value;
            } catch (const ReturnSignal& rs) {
                hasReturn = true;
                retSig = rs;
            } catch (const BreakSignal& bs) {
                hasBreak = true;
                brkSig = bs;
            } catch (const ContinueSignal& cs) {
                hasContinue = true;
                contSig = cs;
            }
            currentEnv = prev;
        }

        if (threw && stmt->catchBlock) {
            auto prev = currentEnv;
            currentEnv = currentEnv->createChild();
            currentEnv->define(stmt->catchParam, thrownValue);
            try {
                for (const auto& s : stmt->catchBlock->stmts)
                    result = execute(s.get());
                threw = false;
            } catch (const ThrowSignal& ts) {
                threw = true;
                thrownValue = ts.value;
            } catch (const ReturnSignal& rs) {
                hasReturn = true;
                retSig = rs;
            } catch (const BreakSignal& bs) {
                hasBreak = true;
                brkSig = bs;
            } catch (const ContinueSignal& cs) {
                hasContinue = true;
                contSig = cs;
            }
            currentEnv = prev;
        }

        if (stmt->finallyBlock) {
            auto prev = currentEnv;
            currentEnv = currentEnv->createChild();
            try {
                for (const auto& s : stmt->finallyBlock->stmts)
                    result = execute(s.get());
            } catch (const ThrowSignal& ts) {
                threw = true;
                thrownValue = ts.value;
            } catch (const ReturnSignal& rs) {
                hasReturn = true;
                retSig = rs;
            } catch (const BreakSignal& bs) {
                hasBreak = true;
                brkSig = bs;
            } catch (const ContinueSignal& cs) {
                hasContinue = true;
                contSig = cs;
            }
            currentEnv = prev;
        }

        if (hasReturn) throw ReturnSignal{retSig.value};
        if (hasBreak) throw BreakSignal{};
        if (hasContinue) throw ContinueSignal{};
        if (threw) throw ThrowSignal{thrownValue};

        return result;
    }

    Value evalBinary(BinaryExprNode* expr) {
        Value left = evaluateExpr(expr->left.get());
        Value right = evaluateExpr(expr->right.get());
        if (expr->op == "+") return left.add(right);
        if (expr->op == "-") return left.sub(right);
        if (expr->op == "*") return left.mul(right);
        if (expr->op == "/") return left.div(right);
        if (expr->op == "%") return left.mod(right);
        if (expr->op == "**") return left.poww(right);
        if (expr->op == "==") return left.eq(right, false);
        if (expr->op == "===") return left.eq(right, true);
        if (expr->op == "!=") { auto r = left.eq(right, false); return Value::makeBool(!r.boolVal); }
        if (expr->op == "!==") { auto r = left.eq(right, true); return Value::makeBool(!r.boolVal); }
        if (expr->op == "<") return left.cmp(right, "<");
        if (expr->op == ">") return left.cmp(right, ">");
        if (expr->op == "<=") return left.cmp(right, "<=");
        if (expr->op == ">=") return left.cmp(right, ">=");
        if (expr->op == "&&") return Value::makeBool(left.isTruthy() && right.isTruthy());
        if (expr->op == "||") return Value::makeBool(left.isTruthy() || right.isTruthy());
        return Value::makeUndefined();
    }

    Value evalUnary(UnaryExprNode* expr) {
        if (expr->op == "!") return evaluateExpr(expr->operand.get()).logicalNot();
        if (expr->op == "-" && expr->prefix) return evaluateExpr(expr->operand.get()).unaryMinus();
        if (expr->op == "+" && expr->prefix) return evaluateExpr(expr->operand.get()).unaryPlus();
        if (expr->op == "typeof") return evaluateExpr(expr->operand.get()).typeOf();
        if (expr->op == "++") {
            Value oldVal = evaluateExpr(expr->operand.get());
            Value newVal = oldVal.add(Value::makeNum(1));
            if (expr->operand->type == ASTType::Identifier) {
                currentEnv->set(static_cast<IdentifierNode*>(expr->operand.get())->name, newVal);
            }
            return expr->prefix ? newVal : oldVal;
        }
        if (expr->op == "--") {
            Value oldVal = evaluateExpr(expr->operand.get());
            Value newVal = oldVal.sub(Value::makeNum(1));
            if (expr->operand->type == ASTType::Identifier) {
                currentEnv->set(static_cast<IdentifierNode*>(expr->operand.get())->name, newVal);
            }
            return expr->prefix ? newVal : oldVal;
        }
        return Value::makeUndefined();
    }

    Value evalCall(CallExprNode* expr) {
        Value callee = evaluateExpr(expr->callee.get());
        std::vector<Value> args;
        for (const auto& arg : expr->args) args.push_back(evaluateExpr(arg.get()));
        return callValue(callee, args);
    }

    Value evalMember(MemberExprNode* expr) {
        Value obj = evaluateExpr(expr->object.get());
        std::string propName;
        if (!expr->computed && expr->property->type == ASTType::Identifier) {
            propName = static_cast<IdentifierNode*>(expr->property.get())->name;
        } else {
            propName = evaluateExpr(expr->property.get()).toString();
        }

        if (obj.type == ValueType::String) {
            if (propName == "length") return Value::makeNum((double)obj.strVal->str.size());
            if (propName == "charAt")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a) -> Value {
                    int i = a.empty()?0:(int)a[0].toNumber();
                    if(i<0||i>=(int)str.size()) return Value::makeStr("");
                    return Value::makeStr(std::string(1,str[i]));
                });
            if (propName == "indexOf")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a) -> Value {
                    auto p = str.find(a.empty()?"":a[0].toString());
                    return Value::makeNum(p==std::string::npos?-1.0:(double)p);
                });
            if (propName == "slice")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a) -> Value {
                    int s = a.empty()?0:(int)a[0].toNumber();
                    int e = a.size()<2?(int)str.size():(int)a[1].toNumber();
                    if(s<0)s=std::max(0,(int)str.size()+s);
                    if(e<0)e=std::max(0,(int)str.size()+e);
                    if(s>=e||s>=(int)str.size())return Value::makeStr("");
                    return Value::makeStr(str.substr(s,e-s));
                });
            if (propName == "toUpperCase")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&)->Value{
                    std::string r=str; std::transform(r.begin(),r.end(),r.begin(),::toupper); return Value::makeStr(r);});
            if (propName == "toLowerCase")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&)->Value{
                    std::string r=str; std::transform(r.begin(),r.end(),r.begin(),::tolower); return Value::makeStr(r);});
            if (propName == "trim")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>&)->Value{
                    std::string s=str; s.erase(0,s.find_first_not_of(" \t\n\r")); s.erase(s.find_last_not_of(" \t\n\r")+1); return Value::makeStr(s);});
            if (propName == "startsWith")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    return Value::makeBool(str.find(a.empty()?"":a[0].toString())==0);});
            if (propName == "endsWith")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    std::string s=a.empty()?"":a[0].toString(); return Value::makeBool(str.size()>=s.size()&&str.substr(str.size()-s.size())==s);});
            if (propName == "includes")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    return Value::makeBool(str.find(a.empty()?"":a[0].toString())!=std::string::npos);});
            if (propName == "repeat")
                return Value::makeNative([str=obj.strVal->str](const std::vector<Value>& a)->Value{
                    int c=a.empty()?0:(int)a[0].toNumber(); if(c<=0)return Value::makeStr("");
                    std::string r; r.reserve(str.size()*c); for(int i=0;i<c;i++)r+=str; return Value::makeStr(r);});
        }

        if (obj.type == ValueType::Array) {
            if (propName == "length") return Value::makeNum((double)obj.arrVal->elements.size());
            if (propName == "push")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a) mutable->Value{
                    for(const auto& v:a)arrPtr->elements.push_back(v); return Value::makeNum((double)arrPtr->elements.size());});
            if (propName == "pop")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&) mutable->Value{
                    if(arrPtr->elements.empty())return Value::makeUndefined(); Value v=arrPtr->elements.back(); arrPtr->elements.pop_back(); return v;});
            if (propName == "shift")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&) mutable->Value{
                    if(arrPtr->elements.empty())return Value::makeUndefined(); Value v=arrPtr->elements.front(); arrPtr->elements.erase(arrPtr->elements.begin()); return v;});
            if (propName == "unshift")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a) mutable->Value{
                    arrPtr->elements.insert(arrPtr->elements.begin(),a.begin(),a.end()); return Value::makeNum((double)arrPtr->elements.size());});
            if (propName == "indexOf")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty())return Value::makeNum(-1);
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value eq=arrPtr->elements[i].eq(a[0],true);if(eq.boolVal)return Value::makeNum((double)i);}
                    return Value::makeNum(-1);});
            if (propName == "includes")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty())return Value::makeBool(false);
                    for(const auto& e:arrPtr->elements){Value eq=e.eq(a[0],true);if(eq.boolVal)return Value::makeBool(true);}
                    return Value::makeBool(false);});
            if (propName == "join")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    std::string sep=a.empty()?",":a[0].toString(); std::string r;
                    for(size_t i=0;i<arrPtr->elements.size();i++){if(i>0)r+=sep;r+=arrPtr->elements[i].toString();}
                    return Value::makeStr(r);});
            if (propName == "slice")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    int s=a.empty()?0:(int)a[0].toNumber(); int e=a.size()<2?(int)arrPtr->elements.size():(int)a[1].toNumber();
                    if(s<0)s=std::max(0,(int)arrPtr->elements.size()+s); if(e<0)e=std::max(0,(int)arrPtr->elements.size()+e);
                    Value r=Value::makeArr(); for(int i=s;i<e&&i<(int)arrPtr->elements.size();i++)r.arrVal->elements.push_back(arrPtr->elements[i]); return r;});
            if (propName == "splice")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a) mutable->Value{
                    int s=a.empty()?0:(int)a[0].toNumber(); int dc=a.size()<2?(int)arrPtr->elements.size():(int)a[1].toNumber();
                    if(s<0)s=std::max(0,(int)arrPtr->elements.size()+s); dc=std::min(dc,(int)arrPtr->elements.size()-s);
                    Value rem=Value::makeArr(); auto it=arrPtr->elements.begin()+s;
                    for(int i=0;i<dc;i++)rem.arrVal->elements.push_back(*(it+i));
                    arrPtr->elements.erase(it,it+dc);
                    for(size_t i=2;i<a.size();i++)arrPtr->elements.insert(arrPtr->elements.begin()+s+(i-2),a[i]);
                    return rem;});
            if (propName == "forEach")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                    for(size_t i=0;i<arrPtr->elements.size();i++){
                        std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                        for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                        callValue(a[0], ca);}
                    return Value::makeUndefined();});
            if (propName == "map")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    Value r=Value::makeArr(); if(a.empty()||!a[0].isFunction())return r;
                    for(size_t i=0;i<arrPtr->elements.size();i++){
                        std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                        for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                        r.arrVal->elements.push_back(callValue(a[0], ca));}
                    return r;});
            if (propName == "filter")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    Value r=Value::makeArr(); if(a.empty()||!a[0].isFunction())return r;
                    for(size_t i=0;i<arrPtr->elements.size();i++){
                        std::vector<Value> ca={arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()};
                        for(const auto& e:arrPtr->elements)ca[2].arrVal->elements.push_back(e);
                        Value rv=callValue(a[0], ca); if(rv.isTruthy())r.arrVal->elements.push_back(arrPtr->elements[i]);}
                    return r;});
            if (propName == "reduce")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                    bool hasInit=a.size()>1; Value acc=hasInit?a[1]:Value::makeUndefined();
                    size_t si=hasInit?0:1; if(!hasInit&&!arrPtr->elements.empty())acc=arrPtr->elements[0];
                    for(size_t i=si;i<arrPtr->elements.size();i++)
                        acc=callValue(a[0], {acc,arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});
                    return acc;});
            if (propName == "find")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeUndefined();
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(r.isTruthy())return arrPtr->elements[i];}
                    return Value::makeUndefined();});
            if (propName == "some")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeBool(false);
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(r.isTruthy())return Value::makeBool(true);}
                    return Value::makeBool(false);});
            if (propName == "every")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())return Value::makeBool(false);
                    for(size_t i=0;i<arrPtr->elements.size();i++){Value r=callValue(a[0],{arrPtr->elements[i],Value::makeNum((double)i),Value::makeArr()});if(!r.isTruthy())return Value::makeBool(false);}
                    return Value::makeBool(true);});
            if (propName == "sort")
                return Value::makeNative([this, arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    if(a.empty()||!a[0].isFunction())std::sort(arrPtr->elements.begin(),arrPtr->elements.end(),[](const Value& x,const Value& y){return x.toNumber()<y.toNumber();});
                    else std::sort(arrPtr->elements.begin(),arrPtr->elements.end(),[this,&a](const Value& x,const Value& y){return callValue(a[0],{x,y}).toNumber()<0;});
                    Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements; return r;});
            if (propName == "reverse")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>&)->Value{
                    std::reverse(arrPtr->elements.begin(),arrPtr->elements.end()); Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements; return r;});
            if (propName == "concat")
                return Value::makeNative([arrPtr=obj.arrVal](const std::vector<Value>& a)->Value{
                    Value r=Value::makeArr(); r.arrVal->elements=arrPtr->elements;
                    for(const auto& v:a){if(v.isArray())r.arrVal->elements.insert(r.arrVal->elements.end(),v.arrVal->elements.begin(),v.arrVal->elements.end());else r.arrVal->elements.push_back(v);}
                    return r;});
        }

        if (obj.type == ValueType::Object) return obj.getProperty(propName);
        if (obj.type == ValueType::Array) {
            char* end=nullptr; double idx=std::strtod(propName.c_str(),&end);
            if(end==propName.c_str()+propName.length())return obj.getIndex((size_t)idx);
            return obj.getProperty(propName);
        }
        return Value::makeUndefined();
    }

    Value evalIdentifier(IdentifierNode* expr) {
        return currentEnv->get(expr->name);
    }

    Value evalArray(ArrayLitNode* expr) {
        Value arr = Value::makeArr();
        for (const auto& elem : expr->elements) {
            if (elem) arr.arrVal->elements.push_back(evaluateExpr(elem.get()));
            else arr.arrVal->elements.push_back(Value::makeUndefined());
        }
        return arr;
    }

    Value evalObject(ObjectLitNode* expr) {
        Value obj = Value::makeObj();
        for (const auto& [key, valExpr] : expr->properties) {
            obj.setProperty(key, evaluateExpr(valExpr.get()));
        }
        return obj;
    }

    Value evalAssign(AssignNode* expr) {
        Value val = evaluateExpr(expr->value.get());
        if (expr->target->type == ASTType::Identifier) {
            auto id = static_cast<IdentifierNode*>(expr->target.get()); // fix: target.get()
            if (expr->op == "=") return currentEnv->set(id->name, val);
            Value existing = currentEnv->get(id->name);
            if (expr->op == "+=") return currentEnv->set(id->name, existing.add(val));
            if (expr->op == "-=") return currentEnv->set(id->name, existing.sub(val));
            if (expr->op == "*=") return currentEnv->set(id->name, existing.mul(val));
            if (expr->op == "/=") return currentEnv->set(id->name, existing.div(val));
            if (expr->op == "%=") return currentEnv->set(id->name, existing.mod(val));
        }
        if (expr->target->type == ASTType::Member) {
            auto mem = static_cast<MemberExprNode*>(expr->target.get());
            Value obj = evaluateExpr(mem->object.get());
            std::string propName = evaluateExpr(mem->property.get()).toString();
            if (expr->op == "=") { obj.setProperty(propName, val); return val; }
            Value existing = obj.getProperty(propName);
            if (expr->op == "+=") { obj.setProperty(propName, existing.add(val)); return val; }
            if (expr->op == "-=") { obj.setProperty(propName, existing.sub(val)); return val; }
        }
        return val;
    }

    Value evalConditional(ConditionalNode* expr) {
        Value cond = evaluateExpr(expr->cond.get());
        if (cond.isTruthy()) return evaluateExpr(expr->thenExpr.get());
        return evaluateExpr(expr->elseExpr.get());
    }

    Value evalArrow(ArrowFuncNode* expr) {
        auto funcData = gHeap.allocate<GCFunction>();
        funcData->params = expr->params;
        funcData->closure = currentEnv;

        if (expr->isExprBody && expr->exprBody) {
            funcData->exprBody = expr->exprBody.get();
            funcData->body = nullptr;
        } else if (expr->body && expr->body->type == ASTType::Block) {
            funcData->body = static_cast<BlockStmt*>(expr->body.get());
            funcData->exprBody = nullptr;
        }
        return Value::makeFunc(funcData);
    }
};

inline Value Interpreter::callValue(const Value& fn, const std::vector<Value>& args) {
    if (fn.type == ValueType::NativeFunction) return fn.nativeVal(args);
    if (fn.type == ValueType::Function) {
        auto funcData = fn.funcVal;
        auto prev = currentEnv;
        currentEnv = funcData->closure->createChild();
        for (size_t i = 0; i < funcData->params.size(); i++) {
            currentEnv->define(funcData->params[i], i < args.size() ? args[i] : Value::makeUndefined());
        }
        Value result;
        if (funcData->exprBody) {
            result = evaluateExpr(funcData->exprBody);
            currentEnv = prev;
            return result;
        }
        if (funcData->body) {
            try {
                for (const auto& s : funcData->body->stmts) execute(s.get());
            } catch (const ReturnSignal& ret) {
                currentEnv = prev;
                return ret.value;
            }
        }
        currentEnv = prev;
        return Value::makeUndefined();
    }
    return Value::makeUndefined();
}

}
