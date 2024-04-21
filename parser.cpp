//
// Created by idang on 13/12/2023.
//

#include "parser.h"

NodeExpr *Parser::parseArrayBrackets() {
    this->lexer->currentAndProceedToken(); // Remove the open square bracket lexeme

    NodeExprP indexExpr = this->parseExpr();

    this->checkPointerUsage(indexExpr);

    if (!this->checkForTokenTypeAndConsume(TokenType::closeSquare)) {
        this->throwSyntaxError("']' expected");
    }

    return indexExpr;
}

NodeExpr *Parser::parseParenthesisExpr() {
    if (!checkForTokenTypeAndConsume(TokenType::openParenthesis)) {
        this->throwSyntaxError("'(' expected");
    }

    NodeExprP innerExpr = this->parseExpr();

    this->checkPointerUsage(innerExpr);

    if (!this->checkForTokenTypeAndConsume(TokenType::closeParenthesis)) {
        this->throwSyntaxError("')' expected");
    }

    return innerExpr;
}

std::vector<Variable> Parser::parseParenthesisVariableList() {
    if (!checkForTokenTypeAndConsume(TokenType::openParenthesis)) {
        this->throwSyntaxError("'(' expected");
    }

    std::vector<Variable> varsList;

    while (!this->checkForTokenTypeAndConsumeIfYes(TokenType::closeParenthesis)) {
        TokenType varTypeKeyword = this->lexer->currentAndProceedToken().type;
        if (varTypeKeyword == TokenType::voidKeyword || !this->typeMap.contains(varTypeKeyword)) {
            this->throwSyntaxError("Valid parameter type expected");
        }

        bool varPtr = checkForTokenType(TokenType::mult);
        if (varPtr) this->lexer->currentAndProceedToken();

        identifierTokenExists();

        varsList.push_back(Variable(this->lexer->currentAndProceedToken().val,
                                    this->typeMap[varTypeKeyword], varPtr));

        if (this->checkForTokenTypeAndConsumeIfYes(TokenType::coma) &&
            this->checkForTokenType(TokenType::closeParenthesis)) {
            this->throwSyntaxError("Expected parameter declaration");
        }
    }

    return varsList;
}

std::vector<NodeExpr *> Parser::parseParenthesisExprList() {
    if (!checkForTokenTypeAndConsume(TokenType::openParenthesis)) {
        this->throwSyntaxError("'(' expected");
    }

    std::vector<NodeExpr *> exprList;

    while (!this->checkForTokenTypeAndConsumeIfYes(TokenType::closeParenthesis)) {
        exprList.push_back(parseExpr());

        if (this->checkForTokenTypeAndConsumeIfYes(TokenType::coma) &&
            this->checkForTokenType(TokenType::closeParenthesis)) {
            this->throwSyntaxError("Expression expected");
        }
    }

    return exprList;
}

void Parser::validateFunctionCallParams(std::vector<NodeExprP> params, NodeFunctionP func) {
    if (params.size() != func->params.size()) {
        for (auto &expr: params) {
            delete expr;
        }

        this->throwSemanticError(
                "Function '" + func->name + "' expected " + std::to_string(func->params.size()) + " parameters");
    }

    for (int i = 0; i < params.size(); ++i) {
        auto exprAddr = dynamic_cast<AddrNodeExpr *>(params[i]);
        auto paramFunc = dynamic_cast<NodeFunctionCall *>(params[i]);
        bool ptr = exprAddr || paramFunc;

        if (func->params[i].ptrType != ptr ||
            (exprAddr && exprAddr->target->variable.type != func->params[i].type) ||
            (paramFunc && paramFunc->function->returnPtr && paramFunc->function->returnType != func->params[i].type)) {
            for (auto &expr: params) {
                delete expr;
            }

            this->throwSemanticError("Function call with incompatible type");
        }
    }
}

std::tuple<NodeStmt *, bool> Parser::tryParseStmt() {
    if (!this->lexer->hasNextToken() || this->lexer->currentToken().type == TokenType::closeCurly) {
        return {nullptr, false};
    }

    NodeStmt *stmt = nullptr;
    bool tryAgain = true;
    bool delimiterIgnore = false;

    Token firstToken = this->lexer->currentToken();

    if (firstToken.type == TokenType::identifier) {
        stmt = this->stmtByIdentifier(this->lexer->currentAndProceedToken());
    } else if (firstToken.type == TokenType::intKeyword || firstToken.type == TokenType::charKeyword) {
        stmt = this->stmtVariableDeclaration(this->typeMap[firstToken.type]);
    } else if (firstToken.type == TokenType::ifKeyword) {
        stmt = this->stmtIf();
        delimiterIgnore = true;
    } else if (firstToken.type == TokenType::whileKeyword) {
        stmt = this->stmtWhile();
        delimiterIgnore = true;
    } else if (firstToken.type == TokenType::doKeyword) {
        stmt = this->stmtWhile(true);
    } else if (firstToken.type == TokenType::returnKeyword) {
        this->lexer->currentAndProceedToken();

        if (this->programTree->functions.back()->returnType == VariableType::voidType) {
            stmt = new NodeReturnStmt(nullptr);
        } else {
            auto innerExpr = parseExpr();

            if (this->programTree->functions.back()->returnPtr != this->ptrUsedInExpr) {
                delete innerExpr;
                this->throwSemanticError("Invalid return type");
            }

            stmt = new NodeReturnStmt(innerExpr);
        }
    } else if (firstToken.type == TokenType::openCurly) {
        stmt = parseScope();
        delimiterIgnore = true;
    } else if (firstToken.type == TokenType::semiColon) {
        // Ignore
    } else {
        tryAgain = false;
    }

    if (!delimiterIgnore && !checkForTokenTypeAndConsume(TokenType::semiColon)) {
        this->throwSyntaxError("';' expected");
    };

    return {stmt, tryAgain};
}

NodeScope *Parser::parseScope() {
    auto scope = new NodeScope();

    if (!checkForTokenTypeAndConsume(TokenType::openCurly)) {
        this->throwSyntaxError("'{' expected");
    }

    this->scopes.push(scope);

    std::tuple<NodeStmt *, bool> stmtTuple = this->tryParseStmt();

    while (get<0>(stmtTuple) || get<1>(stmtTuple)) {
        if (get<0>(stmtTuple)) {
            scope->stmts.push_back(get<0>(stmtTuple));
        }
        stmtTuple = this->tryParseStmt();
    }

    this->scopes.pop();

    if (!checkForTokenTypeAndConsume(TokenType::closeCurly)) {
        this->throwSyntaxError("'}' expected");
    }

    return scope;
}

NodeFunction *Parser::tryParseFunction() {
    if (!this->lexer->hasNextToken()) {
        return nullptr;
    }

    TokenType typeKeyword = this->lexer->currentAndProceedToken().type;

    if (!this->typeMap.contains(typeKeyword)) {
        this->throwSyntaxError("Function type expected");
    }

    VariableType funcType = this->typeMap[typeKeyword];

    bool ptr = checkForTokenType(TokenType::mult);
    if (ptr) this->lexer->currentAndProceedToken();

    if (ptr && funcType == VariableType::voidType) {
        this->throwSemanticError("Cannot return pointer of type void");
    }

    identifierTokenExists();
    std::string funcName = this->lexer->currentAndProceedToken().val;

    if (getFunction(funcName)) this->throwSemanticError("Redeclaration of the function '" + funcName + "'");

    auto funcParams = parseParenthesisVariableList();

    auto function = new NodeFunction(funcType, ptr, funcName, funcParams);

    this->programTree->functions.push_back(function);

    function->scope = this->parseScope();

    return function;
}

ProgramTree *Parser::parseProgram() {
    this->programTree = new ProgramTree();

    while (this->tryParseFunction());

    return this->programTree;
}

void Parser::throwSyntaxError(const std::string &errorMsg) {
    delete this->programTree;
    throw CompilationException(
            "[Parser - Syntax Error] " + errorMsg + " On line " + std::to_string(this->lexer->currentLine));
}

void Parser::throwSemanticError(const std::string &errorMsg) {
    delete this->programTree;
    throw CompilationException(
            "[Parser - Semantic Error] " + errorMsg + " On line " + std::to_string(this->lexer->currentLine));
}

bool Parser::checkForTokenType(TokenType type) {
    return this->lexer->hasNextToken() && this->lexer->currentToken().type == type;
}

bool Parser::checkForTokenTypeAndConsume(TokenType type) {
    return this->lexer->hasNextToken() && this->lexer->currentAndProceedToken().type == type;
}

bool Parser::checkForTokenTypeAndConsumeIfYes(TokenType type) {
    if (this->lexer->hasNextToken() && this->lexer->currentToken().type == type) {
        this->lexer->currentAndProceedToken();
        return true;
    }
    return false;
}

void Parser::identifierTokenExists() {
    if (!checkForTokenType(TokenType::identifier)) {
        this->throwSyntaxError("Identifier expected");
    }
}

std::optional<Variable> Parser::varExistsScopeStack(const std::string &varName) {
    std::stack<NodeScopeP> scopesCopy = this->scopes;
    while (!scopesCopy.empty()) {
        for (auto var: scopesCopy.top()->vars) {
            if (var.name == varName) return var;
        }

        scopesCopy.pop();
    }

    return {};
}

std::optional<Variable> Parser::varExistsCurrentScope(const std::string &varName) {
    for (auto var: this->scopes.top()->vars) {
        if (var.name == varName) return var;
    }

    return {};
}

void Parser::addVarToCurrentScope(const Variable &var) {
//    this->scopes.top()->vars.insert(var);
    this->scopes.top()->vars.push_back(var);
}

Variable Parser::getVarCurrentScope(const std::string &varName) {
    std::optional<Variable> var = varExistsCurrentScope(varName);

    if (var.has_value()) return var.value();

    this->throwSemanticError("Use of undeclared identifier " + varName);
}

Variable Parser::getVarScopeStack(const std::string &varName) {
    std::optional<Variable> var = varExistsScopeStack(varName);

    if (var.has_value()) return var.value();

    for (auto param : this->programTree->functions.back()->params) {
        if (param.name == varName) return param;
    }

    this->throwSemanticError("Use of undeclared identifier " + varName);
}

NodeFunction *Parser::getFunction(const std::string &funcName) {
    for (auto funcPtr: this->programTree->functions) {
        if (funcPtr->name == funcName) return funcPtr;
    }

    return nullptr;
}
