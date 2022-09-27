#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <unordered_set>

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value ==
                rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value ==
                rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os,
        const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type)                                     \
            if (auto p = rhs.TryAs<type>())                     \
        return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type)                                   \
            if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    // ---------------------- Lexer: public ----------------------- //

    Lexer::Lexer(std::istream& input)
        : begin_(std::istreambuf_iterator<char>(input))
    {
        LoadTokens();
    }

    const Token& Lexer::CurrentToken() const {
        return tokens_[current_token_index_];
    }

    Token Lexer::NextToken() {

        if (tokens_.back() != token_type::Eof()) {
            LoadTokens();
        }

        if (tokens_.size() > current_token_index_ + 1) {
            ++current_token_index_;
        }

        return tokens_[current_token_index_];
    }

    // ---------------------- Lexer: private ---------------------- //

    void Lexer::LoadTokens() {

        auto it = begin_;
        auto end = std::istreambuf_iterator<char>();

        while (true) {

            if (it == end) {
                AssignEofToken();
                begin_ = it;
                break;
            }

            size_t spaces = 0;
            while (*it == ' ') {
                ++spaces;
                ++it;
            }

            char c = *it;
            if (tokens_.empty() ||
                tokens_.back().Is<token_type::Newline>()) {
                if ((c != '\n' && c != '#') || it == end) {
                    if (AssignIndentTokens(spaces)) {
                        begin_ = it;
                        break;
                    }
                }
            }

            std::string word = "";
            if (c >= '0' && c <= '9') {
                do {
                    word += *it;
                    ++it;
                } while (it != end && *it >= '0' && *it <= '9');

                tokens_.push_back(
                    token_type::Number{ std::stoi(word) });
                begin_ = it;
                break;
            }

            if (c == '\'' || c == '"') {
                ++it;
                if (it == end) {
                    AssignEofToken();
                    begin_ = it;
                    break;
                }

                while (it != end && *it != c) {
                    if (*it == '\\') {
                        ++it;
                        if (it == end) {
                            break;
                        }
                        switch (*it) {
                        case 'n':
                            word += '\n';
                            break;
                        case 't':
                            word += '\t';
                            break;
                        case 'r':
                            word += '\r';
                            break;
                        case '"':
                            word += '"';
                            break;
                        case '\\':
                            word += '\\';
                            break;
                        case '\'':
                            word += '\'';
                            break;
                        default:
                            break;
                        }
                    }
                    else {
                        word += *it;
                    }

                    ++it;
                }

                if (it != end) {
                    ++it;
                }
                else {
                    AssignEofToken();
                    begin_ = it;
                    break;
                }

                tokens_.push_back(token_type::String{ word });
                begin_ = it;
                break;
            }

            if (c == '=' || c == '!' || c == '<' || c == '>') {
                ++it;
                if (it == end) {
                    tokens_.push_back(token_type::Char{ c });
                    AssignEofToken();
                    begin_ = it;
                    break;
                }

                if (*it == '=') {
                    if (c == '=') {
                        tokens_.push_back(token_type::Eq());
                    }
                    else if (c == '!') {
                        tokens_.push_back(token_type::NotEq());
                    }
                    else if (c == '<') {
                        tokens_.push_back(token_type::LessOrEq());
                    }
                    else if (c == '>') {
                        tokens_.push_back(token_type::GreaterOrEq());
                    }
                    ++it;
                    begin_ = it;
                    break;
                }

                tokens_.push_back(token_type::Char{ c });
                begin_ = it;
                break;
            }

            if (c == '.' || c == ',' || c == ':' ||
                c == '(' || c == ')' || c == '+' ||
                c == '-' || c == '*' || c == '/') {
                tokens_.push_back(token_type::Char{ c });
                ++it;
                begin_ = it;
                break;
            }

            if ((c >= 'A' && c <= 'Z') ||
                c == '_' ||
                (c >= 'a' && c <= 'z')) {

                while ((*it >= '0' && *it <= '9') ||
                    (*it >= 'A' && *it <= 'Z') ||
                    *it == '_' ||
                    (*it >= 'a' && *it <= 'z')) {

                    word += *it;
                    ++it;
                    if (it == end) {
                        break;
                    }
                }

                AssignWordToken(word);
                begin_ = it;
                break;
            }

            if (c == '#') {
                do {
                    ++it;
                } while (it != end && *it != '\n');
            }
            else if (c == '\n') {
                if (!tokens_.empty() &&
                    !tokens_.back().Is<token_type::Newline>()) {

                    tokens_.push_back(token_type::Newline());
                    begin_ = it;
                    break;
                }
                ++it;
            }
        }
    }

    bool Lexer::AssignIndentTokens(size_t spaces) {
        auto indent = spaces / 2;
        if (indent == indent_) {
            return false;
        }

        if (indent > indent_) {
            while (indent > indent_) {
                ++indent_;
                tokens_.push_back(token_type::Indent());
            }
        }
        else {
            while (indent < indent_) {
                --indent_;
                tokens_.push_back(token_type::Dedent());
            }
        }

        return true;
    }

    void Lexer::AssignWordToken(const std::string& word) {

        if (word == "class"s) {
            tokens_.push_back(token_type::Class());
            return;
        }

        if (word == "return"s) {
            tokens_.push_back(token_type::Return());
            return;
        }

        if (word == "if"s) {
            tokens_.push_back(token_type::If());
            return;
        }

        if (word == "else"s) {
            tokens_.push_back(token_type::Else());
            return;
        }

        if (word == "def"s) {
            tokens_.push_back(token_type::Def());
            return;
        }

        if (word == "print"s) {
            tokens_.push_back(token_type::Print());
            return;
        }

        if (word == "and"s) {
            tokens_.push_back(token_type::And());
            return;
        }

        if (word == "or"s) {
            tokens_.push_back(token_type::Or());
            return;
        }

        if (word == "not"s) {
            tokens_.push_back(token_type::Not());
            return;
        }

        if (word == "None"s) {
            tokens_.push_back(token_type::None());
            return;
        }

        if (word == "True"s) {
            tokens_.push_back(token_type::True());
            return;
        }

        if (word == "False"s) {
            tokens_.push_back(token_type::False());
            return;
        }

        tokens_.push_back(token_type::Id{ word });
    }

    void Lexer::AssignEofToken() {

        if (!tokens_.empty() &&
            !tokens_.back().Is<token_type::Newline>()) {
            tokens_.push_back(token_type::Newline());
        }

        while (indent_ > 0) {
            --indent_;
            tokens_.push_back(token_type::Dedent());
        }

        tokens_.push_back(token_type::Eof());
    }

}  // namespace parse