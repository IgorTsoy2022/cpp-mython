#pragma once

#include <iosfwd>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace std::literals;

namespace parse {

    namespace token_type {
        struct Number {         // ??????? ???????
            int value;
        };

        struct Id {             // ??????? ???????????????
            std::string value;  // ??? ??????????????
        };

        struct Char {           // ??????? ????????
            char value;
        };

        struct String {         // ??????? ?????????? ??????????
            std::string value;
        };

        struct Class {};        // ??????? ?class?
        struct Return {};       // ??????? ?return?
        struct If {};           // ??????? ?if?
        struct Else {};         // ??????? ?else?
        struct Def {};          // ??????? ?def?
        struct Newline {};      // ??????? ?????? ???????
        struct Print {};        // ??????? ?print?
        struct Indent {};       // ??????? ??????????? ????????,
        // ????????????? ???? ????????
        struct Dedent {};       // ??????? ??????????? ????????
        struct Eof {};          // ??????? ?????? ??????
        struct And {};          // ??????? ?and?
        struct Or {};           // ??????? ?or?
        struct Not {};          // ??????? ?not?
        struct Eq {};           // ??????? ?==?
        struct NotEq {};        // ??????? ?!=?
        struct LessOrEq {};     // ??????? ?<=?
        struct GreaterOrEq {};  // ??????? ?>=?
        struct None {};         // ??????? ?None?
        struct True {};         // ??????? ?True?
        struct False {};        // ??????? ?False?

    }  // namespace token_type

    using TokenBase =
        std::variant<token_type::Number, token_type::Id,
        token_type::Char, token_type::String,
        token_type::Class, token_type::Return,
        token_type::If, token_type::Else,
        token_type::Def, token_type::Newline,
        token_type::Print, token_type::Indent,
        token_type::Dedent, token_type::And,
        token_type::Or, token_type::Not,
        token_type::Eq, token_type::NotEq,
        token_type::LessOrEq, token_type::GreaterOrEq,
        token_type::None, token_type::True,
        token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhs, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        // ?????????? ?????? ?? ??????? ????? ??? token_type::Eof,
        // ???? ????? ??????? ??????????
        [[nodiscard]] const Token& CurrentToken() const;

        // ?????????? ????????? ?????, ???? token_type::Eof, ????
        // ????? ??????? ??????????
        Token NextToken();

        // ???? ??????? ????? ????? ??? T, ????? ?????????? ??????
        // ?? ????.
        // ? ????????? ?????? ????? ??????????? ??????????
        // LexerError
        template <typename T>
        const T& Expect() const {

            if (tokens_[current_token_index_].Is<T>()) {
                return tokens_[current_token_index_].As<T>();
            }

            using namespace std::literals;
            throw LexerError("Unexpected type!"s);
        }

        // ????? ?????????, ??? ??????? ????? ????? ??? T, ? ???
        // ????? ???????? ???????? value.
        // ? ????????? ?????? ????? ??????????? ??????????
        // LexerError
        template <typename T, typename U>
        void Expect(const U& value) const {

            if (tokens_[current_token_index_].Is<T>()) {
                if (tokens_[current_token_index_].As<T>().value == value) {
                    return;
                }
            }

            using namespace std::literals;
            throw LexerError("Unexpected type!"s);
        }

        // ???? ????????? ????? ????? ??? T, ????? ??????????
        // ?????? ?? ????.
        // ? ????????? ?????? ????? ??????????? ??????????
        // LexerError
        template <typename T>
        const T& ExpectNext() {

            if (NextToken().Is<T>()) {
                return tokens_[current_token_index_].As<T>();
            }

            using namespace std::literals;
            throw LexerError("Unexpected type!"s);
        }

        // ????? ?????????, ??? ????????? ????? ????? ??? T,
        // ? ??? ????? ???????? ???????? value.
        // ? ????????? ?????? ????? ??????????? ??????????
        // LexerError
        template <typename T, typename U>
        void ExpectNext(const U& value) {

            if (NextToken().Is<T>()) {
                if (tokens_[current_token_index_].As<T>().value == value) {
                    return;
                }
            }

            using namespace std::literals;
            throw LexerError("Unexpected type!"s);
        }

    private:
        std::istreambuf_iterator<char> begin_;
        size_t indent_ = 0;
        size_t current_token_index_ = 0;
        std::vector<Token> tokens_;

        void LoadTokens();
        bool AssignIndentTokens(size_t spaces);
        void AssignWordToken(const std::string& word);
        void AssignEofToken();
    };

}  // namespace parse