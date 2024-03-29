// lexer.h

#ifndef COMPILER_LEXER_H
#define COMPILER_LEXER_H

#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include "errorHandling.h"

enum class TokenType {
    NO_TOKEN,
    NEW_LINE,
    plus,
    minus,
    mult,
    div,
    equal,
    openParenthesis,
    closeParenthesis,
    immediateInteger,
    identifier,
    intKeyword,
    charKeyword,
    voidKeyword,
    ifKeyword,
    elseKeyword,
    whileKeyword,
    doKeyword,
    returnKeyword,
    exit,
    semiColon,
    coma,
    openCurly,
    closeCurly,
    openSquare,
    closeSquare,
    exclamation,
    ampersand,
    pipe,
    logicalOr,
    logicalAnd,
    doubleEqual,
    notEqual,
    relationalG,
    relationalGE,
    relationalL,
    relationalLE,
};

std::string getTokenName(TokenType tokenType);

struct Token {
    TokenType type;
    std::string val;
    int line;

    /*Token(TokenType type, int line) {
        this->type = type;
        this->line = line;
    }

    Token(TokenType type, std::string value, int line) : val(std::move(value)) {
        this->type = type;
        this->line = line;
    }*/
};

class Lexer {
public:
    int currentLine = 1;

    explicit Lexer(std::string& src) :
    srcCode(std::move(src)) {

    }

    std::vector<Token> analyseSource();
    bool hasNextToken();
    Token currentToken();
    Token currentAndProceedToken();

private:
    void throwLexerError(const std::string &errorMsg) const;

    int nCurIndex = 0;
    int nTokenIndex = 0;
    std::string srcCode;
    std::vector<Token> tokens;

    char current();
    char currentAndProceed();
    bool hasBuffer();

    void countLines();
};


#endif //COMPILER_LEXER_H
