#include "lexical/lexical.hpp"


class Lexer {
public:
    struct Result {
        vector<Token> tokens;
        vector<Diagnostic> diagnostics;
    };

    Result scan(const string& src) const {
        Result res;

        static const unordered_map<string, TokenType> kReservedWords = {
            {"program", TokenType::PROGRAM},
            {"procedure", TokenType::PROCEDURE},
            {"type", TokenType::TYPE},
            {"var", TokenType::VAR},
            {"if", TokenType::IF},
            {"then", TokenType::THEN},
            {"else", TokenType::ELSE},
            {"fi", TokenType::FI},
            {"while", TokenType::WHILE},
            {"do", TokenType::DO},
            {"endwh", TokenType::ENDWH},
            {"begin", TokenType::BEGIN},
            {"end", TokenType::END},
            {"read", TokenType::READ},
            {"write", TokenType::WRITE},
            {"array", TokenType::ARRAY},
            {"of", TokenType::OF},
            {"record", TokenType::RECORD},
            {"return", TokenType::RETURN},
            {"integer", TokenType::INTEGER},
            {"char", TokenType::CHAR},
        };

        size_t i = 0;
        int line = 1;
        int col = 1;

        auto atEnd = [&]() -> bool { return i >= src.size(); };
        auto peek = [&](size_t offset = 0) -> char {
            if (i + offset >= src.size()) {
                return '\0';
            }
            return src[i + offset];
        };
        auto advance = [&]() -> char {
            char c = src[i++];
            if (c == '\n') {
                line += 1;
                col = 1;
            } else {
                col += 1;
            }
            return c;
        };
        auto addToken = [&](TokenType t, const string& lexeme, int tokenLine, int tokenCol) {
            res.tokens.push_back(Token{t, lexeme, SourcePos{tokenLine, tokenCol}});
        };
        auto addError = [&](int errLine, int errCol, const string& message, const string& badLexeme) {
            res.diagnostics.push_back(Diagnostic{"Lexical", SourcePos{errLine, errCol}, message});
            res.tokens.push_back(Token{TokenType::ERROR, badLexeme, SourcePos{errLine, errCol}});
        };

        while (!atEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
                continue;
            }

            int startLine = line;
            int startCol = col;

            if (c == '{') {
                advance();
                bool closed = false;
                while (!atEnd()) {
                    char inner = advance();
                    if (inner == '}') {
                        closed = true;
                        break;
                    }
                }
                if (!closed) {
                    res.diagnostics.push_back(Diagnostic{
                        "Lexical", SourcePos{startLine, startCol}, "Unterminated comment"
                    });
                }
                continue;
            }

            if (std::isalpha(static_cast<unsigned char>(c))) {
                string word;
                while (std::isalnum(static_cast<unsigned char>(peek()))) {
                    word.push_back(advance());
                }
                auto it = kReservedWords.find(word);
                if (it != kReservedWords.end()) {
                    addToken(it->second, word, startLine, startCol);
                } else {
                    addToken(TokenType::ID, word, startLine, startCol);
                }
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(c))) {
                string number;
                while (std::isdigit(static_cast<unsigned char>(peek()))) {
                    number.push_back(advance());
                }
                addToken(TokenType::INTC, number, startLine, startCol);
                continue;
            }

            if (c == '\'') {
                advance();
                if (atEnd() || peek() == '\n') {
                    addError(startLine, startCol, "Unterminated char constant", "'");
                    continue;
                }
                char ch = advance();
                if (peek() == '\'') {
                    advance();
                    string v(1, ch);
                    addToken(TokenType::CHARC, v, startLine, startCol);
                } else {
                    while (!atEnd() && peek() != '\'' && peek() != '\n') {
                        advance();
                    }
                    if (peek() == '\'') {
                        advance();
                    }
                    addError(startLine, startCol, "Invalid char constant", string(1, ch));
                }
                continue;
            }

            if (c == ':' && peek(1) == '=') {
                advance();
                advance();
                addToken(TokenType::ASSIGN, ":=", startLine, startCol);
                continue;
            }
            if (c == '.' && peek(1) == '.') {
                advance();
                advance();
                addToken(TokenType::UNDERANGE, "..", startLine, startCol);
                continue;
            }

            switch (c) {
                case '=':
                    advance();
                    addToken(TokenType::EQ, "=", startLine, startCol);
                    break;
                case '<':
                    advance();
                    addToken(TokenType::LT, "<", startLine, startCol);
                    break;
                case '+':
                    advance();
                    addToken(TokenType::PLUS, "+", startLine, startCol);
                    break;
                case '-':
                    advance();
                    addToken(TokenType::MINUS, "-", startLine, startCol);
                    break;
                case '*':
                    advance();
                    addToken(TokenType::TIMES, "*", startLine, startCol);
                    break;
                case '/':
                    advance();
                    addToken(TokenType::OVER, "/", startLine, startCol);
                    break;
                case '(':
                    advance();
                    addToken(TokenType::LPAREN, "(", startLine, startCol);
                    break;
                case ')':
                    advance();
                    addToken(TokenType::RPAREN, ")", startLine, startCol);
                    break;
                case '[':
                    advance();
                    addToken(TokenType::LMIDPAREN, "[", startLine, startCol);
                    break;
                case ']':
                    advance();
                    addToken(TokenType::RMIDPAREN, "]", startLine, startCol);
                    break;
                case '.':
                    advance();
                    addToken(TokenType::DOT, ".", startLine, startCol);
                    break;
                case ';':
                    advance();
                    addToken(TokenType::SEMI, ";", startLine, startCol);
                    break;
                case ',':
                    advance();
                    addToken(TokenType::COMMA, ",", startLine, startCol);
                    break;
                case ':':
                    advance();
                    addToken(TokenType::COLON, ":", startLine, startCol);
                    break;
                default: {
                    string bad(1, c);
                    addError(startLine, startCol, "Unknown character: '" + bad + "'", bad);
                    advance();
                    break;
                }
            }
        }

        res.tokens.push_back(Token{TokenType::ENDFILE, "EOF", SourcePos{line, col}});
        return res;
    }
};

static bool isReservedWordToken(TokenType t) {
    switch (t) {
        case TokenType::PROGRAM:
        case TokenType::PROCEDURE:
        case TokenType::TYPE:
        case TokenType::VAR:
        case TokenType::IF:
        case TokenType::THEN:
        case TokenType::ELSE:
        case TokenType::FI:
        case TokenType::WHILE:
        case TokenType::DO:
        case TokenType::ENDWH:
        case TokenType::BEGIN:
        case TokenType::END:
        case TokenType::READ:
        case TokenType::WRITE:
        case TokenType::ARRAY:
        case TokenType::OF:
        case TokenType::RECORD:
        case TokenType::RETURN:
        case TokenType::INTEGER:
        case TokenType::CHAR:
            return true;
        default:
            return false;
    }
}

string renderTokens(const vector<Token>& tokens) {
    std::ostringstream oss;
    for (const auto& tk : tokens) {
        if (tk.type == TokenType::ENDFILE) {
            continue;
        }
        oss << tk.pos.line << ": ";
        if (isReservedWordToken(tk.type)) {
            oss << "reserved word: " << tk.lexeme;
        } else if (tk.type == TokenType::ID) {
            oss << "ID, name=" << tk.lexeme;
        } else if (tk.type == TokenType::INTC) {
            oss << "INTC, val=" << tk.lexeme;
        } else if (tk.type == TokenType::CHARC) {
            oss << "CHARC, val='" << tk.lexeme << "'";
        } else if (tk.type == TokenType::ERROR) {
            oss << "ERROR, lexeme=" << tk.lexeme;
        } else {
            oss << tk.lexeme;
        }
        oss << "\n";
    }
    return oss.str();
}



LexResult runLexer(const string& src) {
    Lexer lexer;
    auto res = lexer.scan(src);
    LexResult out;
    out.tokens = std::move(res.tokens);
    out.diagnostics = std::move(res.diagnostics);
    return out;
}
