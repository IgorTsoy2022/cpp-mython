#include "parse.h"

#include "lexer.h"
#include "statement.h"

using namespace std::string_literals;

namespace TokenType = parse::token_type;

namespace {

    bool operator==(const parse::Token& token, char c) {
        const auto* p = token.TryAs<TokenType::Char>();
        return p != nullptr && p->value == c;
    }

    bool operator!=(const parse::Token& token, char c) {
        return !(token == c);
    }

    class Parser {
    public:
        explicit Parser(parse::Lexer& lexer)
            : lexer_(lexer)
        {}

        // Program -> eps
        //          | Statement \n Program
        std::unique_ptr<ast::Statement> ParseProgram() {
            auto result = std::make_unique<ast::Compound>();
            while (!lexer_.CurrentToken().Is<TokenType::Eof>()) {
                result->AddStatement(ParseStatement());
            }

            return result;
        }

    private:
        // Suite -> NEWLINE INDENT (Statement)+ DEDENT
        std::unique_ptr<ast::Statement> ParseSuite()  // NOLINT
        {
            lexer_.Expect<TokenType::Newline>();
            lexer_.ExpectNext<TokenType::Indent>();

            lexer_.NextToken();

            auto result = std::make_unique<ast::Compound>();
            while (!lexer_.CurrentToken().Is<TokenType::Dedent>()) {
                result->AddStatement(ParseStatement());  // NOLINT
            }

            lexer_.Expect<TokenType::Dedent>();
            lexer_.NextToken();

            return result;
        }

        // Methods -> [def id(Params) : Suite]*
        std::vector<runtime::Method> ParseMethods()  // NOLINT
        {
            std::vector<runtime::Method> result;

            while (lexer_.CurrentToken().Is<TokenType::Def>()) {
                runtime::Method m;

                m.name = lexer_.ExpectNext<TokenType::Id>().value;
                lexer_.ExpectNext<TokenType::Char>('(');

                if (lexer_.NextToken().Is<TokenType::Id>()) {
                    m.formal_params.push_back(
                        lexer_.Expect<TokenType::Id>().value);
                    while (lexer_.NextToken() == ',') {
                        m.formal_params.push_back(
                            lexer_.ExpectNext<TokenType::Id>().value);
                    }
                }

                lexer_.Expect<TokenType::Char>(')');
                lexer_.ExpectNext<TokenType::Char>(':');
                lexer_.NextToken();

                // NOLINT
                m.body = 
                    std::make_unique<ast::MethodBody>(ParseSuite());

                result.push_back(std::move(m));
            }
            return result;
        }

        // ClassDefinition -> Id ['(' Id ')'] : new_line indent
        // MethodList dedent
        // NOLINT
        std::unique_ptr<ast::Statement> ParseClassDefinition()
        {
            std::string 
            class_name = lexer_.Expect<TokenType::Id>().value;

            lexer_.NextToken();

            const runtime::Class* base_class = nullptr;
            if (lexer_.CurrentToken() == '(') {
                auto name = lexer_.ExpectNext<TokenType::Id>().value;
                lexer_.ExpectNext<TokenType::Char>(')');
                lexer_.NextToken();

                auto it = declared_classes_.find(name);
                if (it == declared_classes_.end()) {
                    throw ParseError("Base class "s + name + 
                                     " not found for class "s +
                                     class_name);
                }
                // NOLINT
                base_class = 
                    static_cast<const runtime::Class*>(it->second.Get());
            }

            lexer_.Expect<TokenType::Char>(':');
            lexer_.ExpectNext<TokenType::Newline>();
            lexer_.ExpectNext<TokenType::Indent>();
            lexer_.ExpectNext<TokenType::Def>();
            // NOLINT
            std::vector<runtime::Method> methods = ParseMethods();

            lexer_.Expect<TokenType::Dedent>();
            lexer_.NextToken();

            auto [it, inserted] = declared_classes_.insert({
                class_name,
                runtime::ObjectHolder::Own(
                    runtime::Class(class_name, std::move(methods),
                                   base_class)),
                });

            if (!inserted) {
                throw ParseError("Class "s + class_name +
                                 " already exists"s);
            }

            return std::make_unique<ast::ClassDefinition>(it->second);
        }

        std::vector<std::string> ParseDottedIds() {
            std::vector<std::string> 
            result(1, lexer_.Expect<TokenType::Id>().value);

            while (lexer_.NextToken() == '.') {
                result.push_back(
                    lexer_.ExpectNext<TokenType::Id>().value);
            }

            return result;
        }

        //  AssgnOrCall -> DottedIds = Expr
        //               | DottedIds '(' ExprList ')'
        std::unique_ptr<ast::Statement> ParseAssignmentOrCall() {
            lexer_.Expect<TokenType::Id>();

            std::vector<std::string> id_list = ParseDottedIds();
            std::string last_name = id_list.back();
            id_list.pop_back();

            if (lexer_.CurrentToken() == '=') {
                lexer_.NextToken();

                if (id_list.empty()) {
                    return std::make_unique<ast::Assignment>(
                        std::move(last_name), ParseTest());
                }
                return std::make_unique<ast::FieldAssignment>(
                    ast::VariableValue{ std::move(id_list) },
                    std::move(last_name), ParseTest());
            }
            lexer_.Expect<TokenType::Char>('(');
            lexer_.NextToken();

            if (id_list.empty()) {
                throw ParseError("Mython doesn't support " +
                                 "functions, only methods : "s +
                                 last_name);
            }

            std::vector<std::unique_ptr<ast::Statement>> args;
            if (lexer_.CurrentToken() != ')') {
                args = ParseTestList();
            }
            lexer_.Expect<TokenType::Char>(')');
            lexer_.NextToken();

            return std::make_unique<ast::MethodCall>(
                std::make_unique<ast::VariableValue>(std::move(id_list)),
                std::move(last_name), std::move(args));
        }

        // Expr -> Adder ['+'/'-' Adder]*
        std::unique_ptr<ast::Statement> ParseExpression()  // NOLINT
        {
            std::unique_ptr<ast::Statement> result = ParseAdder();
            while (lexer_.CurrentToken() == '+' ||
                   lexer_.CurrentToken() == '-') {
                char op = lexer_.CurrentToken()
                                .As<TokenType::Char>()
                                .value;
                lexer_.NextToken();

                if (op == '+') {
                    result = std::make_unique<ast::Add>(
                        std::move(result), ParseAdder());
                }
                else {
                    result = std::make_unique<ast::Sub>(
                        std::move(result), ParseAdder());
                }
            }
            return result;
        }

        // Adder -> Mult ['*'/'/' Mult]*
        std::unique_ptr<ast::Statement> ParseAdder()  // NOLINT
        {
            std::unique_ptr<ast::Statement> result = ParseMult();
            while (lexer_.CurrentToken() == '*' ||
                   lexer_.CurrentToken() == '/') {
                char op = lexer_.CurrentToken()
                                .As<TokenType::Char>()
                                .value;
                lexer_.NextToken();

                if (op == '*') {
                    result = std::make_unique<ast::Mult>(
                        std::move(result), ParseMult());
                }
                else {
                    result = std::make_unique<ast::Div>(
                        std::move(result), ParseMult());
                }
            }
            return result;
        }

        // Mult -> '(' Expr ')'
        //       | NUMBER
        //       | '-' Mult
        //       | STRING
        //       | NONE
        //       | TRUE
        //       | FALSE
        //       | DottedIds '(' ExprList ')'
        //       | DottedIds
        std::unique_ptr<ast::Statement> ParseMult()  // NOLINT
        {
            if (lexer_.CurrentToken() == '(') {
                lexer_.NextToken();
                auto result = ParseTest();
                lexer_.Expect<TokenType::Char>(')');
                lexer_.NextToken();
                return result;
            }

            if (lexer_.CurrentToken() == '-') {
                lexer_.NextToken();
                return std::make_unique<ast::Mult>(
                    ParseMult(),
                    std::make_unique<ast::NumericConst>(-1));
            }
            if (const auto* num = lexer_.CurrentToken()
                                        .TryAs<TokenType::Number>()) {
                int result = num->value;
                lexer_.NextToken();
                return std::make_unique<ast::NumericConst>(result);
            }
            if (const auto* str = lexer_.CurrentToken()
                                        .TryAs<TokenType::String>()) {
                std::string result = str->value;
                lexer_.NextToken();
                return std::make_unique<ast::StringConst>(
                    std::move(result));
            }
            if (lexer_.CurrentToken().Is<TokenType::True>()) {
                lexer_.NextToken();
                return std::make_unique<ast::BoolConst>(
                    runtime::Bool(true));
            }
            if (lexer_.CurrentToken().Is<TokenType::False>()) {
                lexer_.NextToken();
                return std::make_unique<ast::BoolConst>(
                    runtime::Bool(false));
            }
            if (lexer_.CurrentToken().Is<TokenType::None>()) {
                lexer_.NextToken();
                return std::make_unique<ast::None>();
            }

            return ParseDottedIdsInMultExpr();
        }

        std::unique_ptr<ast::Statement> ParseDottedIdsInMultExpr() {
            std::vector<std::string> names = ParseDottedIds();

            if (lexer_.CurrentToken() == '(') {
                // various calls
                std::vector<std::unique_ptr<ast::Statement>> args;
                if (lexer_.NextToken() != ')') {
                    args = ParseTestList();
                }
                lexer_.Expect<TokenType::Char>(')');
                lexer_.NextToken();

                auto method_name = names.back();
                names.pop_back();

                if (!names.empty()) {
                    return std::make_unique<ast::MethodCall>(
                        std::make_unique<ast::VariableValue>(
                            std::move(names)),
                            std::move(method_name),
                            std::move(args));
                }
                if (auto it = declared_classes_.find(method_name);
                    it != declared_classes_.end()) {
                    return std::make_unique<ast::NewInstance>(
                        static_cast<const runtime::Class&>(
                            *it->second), std::move(args));  // NOLINT
                }
                if (method_name == "str"sv) {
                    if (args.size() != 1) {
                        throw ParseError("Function str takes " +
                                         "exactly one argument"s);
                    }
                    return std::make_unique<ast::Stringify>(
                        std::move(args.front()));
                }
                throw ParseError("Unknown call to "s + 
                                 method_name + "()"s);
            }
            return std::make_unique<ast::VariableValue>(
                std::move(names));
        }
 
        std::vector<std::unique_ptr<ast::Statement>>
        ParseTestList()  // NOLINT
        {
            std::vector<std::unique_ptr<ast::Statement>> result;
            result.push_back(ParseTest());

            while (lexer_.CurrentToken() == ',') {
                lexer_.NextToken();
                result.push_back(ParseTest());
            }
            return result;
        }

        // Condition -> if LogicalExpr: Suite [else: Suite]
        std::unique_ptr<ast::Statement> ParseCondition()  // NOLINT
        {
            lexer_.Expect<TokenType::If>();
            lexer_.NextToken();

            auto condition = ParseTest();

            lexer_.Expect<TokenType::Char>(':');
            lexer_.NextToken();

            auto if_body = ParseSuite();

            std::unique_ptr<ast::Statement> else_body;
            if (lexer_.CurrentToken().Is<TokenType::Else>()) {
                lexer_.ExpectNext<TokenType::Char>(':');
                lexer_.NextToken();
                else_body = ParseSuite();
            }

            return std::make_unique<ast::IfElse>(
                std::move(condition), std::move(if_body),
                std::move(else_body));
        }

        // LogicalExpr -> AndTest [OR AndTest]
        // AndTest -> NotTest [AND NotTest]
        // NotTest -> [NOT] NotTest
        //          | Comparison
        std::unique_ptr<ast::Statement> ParseTest()  // NOLINT
        {
            auto result = ParseAndTest();
            while (lexer_.CurrentToken().Is<TokenType::Or>()) {
                lexer_.NextToken();
                result = std::make_unique<ast::Or>(
                    std::move(result), ParseAndTest());
            }
            return result;
        }

        std::unique_ptr<ast::Statement> ParseAndTest()  // NOLINT
        {
            auto result = ParseNotTest();
            while (lexer_.CurrentToken().Is<TokenType::And>()) {
                lexer_.NextToken();
                result = std::make_unique<ast::And>(
                    std::move(result), ParseNotTest());
            }
            return result;
        }

        std::unique_ptr<ast::Statement> ParseNotTest()  // NOLINT
        {
            if (lexer_.CurrentToken().Is<TokenType::Not>()) {
                lexer_.NextToken();
                // NOLINT
                return std::make_unique<ast::Not>(ParseNotTest());
            }
            return ParseComparison();
        }

        // Comparison -> Expr [COMP_OP Expr]
        std::unique_ptr<ast::Statement> ParseComparison()  // NOLINT
        {
            auto result = ParseExpression();

            const auto tok = lexer_.CurrentToken();

            if (tok == '<') {
                lexer_.NextToken();
                return std::make_unique<ast::Comparison>(
                    runtime::Less, std::move(result),
                    ParseExpression());
            }
            if (tok == '>') {
                lexer_.NextToken();
                return std::make_unique<ast::Comparison>(
                    runtime::Greater, std::move(result),
                    ParseExpression());
            }
            if (tok.Is<TokenType::Eq>()) {
                lexer_.NextToken();
                return std::make_unique<ast::Comparison>(
                    runtime::Equal, std::move(result),
                    ParseExpression());
            }
            if (tok.Is<TokenType::NotEq>()) {
                lexer_.NextToken();
                return std::make_unique<ast::Comparison>(
                    runtime::NotEqual, std::move(result),
                    ParseExpression());
            }
            if (tok.Is<TokenType::LessOrEq>()) {
                lexer_.NextToken();
                return std::make_unique<ast::Comparison>(
                    runtime::LessOrEqual, std::move(result),
                    ParseExpression());
            }
            if (tok.Is<TokenType::GreaterOrEq>()) {
                lexer_.NextToken();
                return std::make_unique<ast::Comparison>(
                    runtime::GreaterOrEqual, std::move(result),
                    ParseExpression());
            }
            return result;
        }

        // Statement -> SimpleStatement Newline
        //           | class ClassDefinition
        //           | if Condition
        std::unique_ptr<ast::Statement> ParseStatement()  // NOLINT
        {
            const auto& tok = lexer_.CurrentToken();

            if (tok.Is<TokenType::Class>()) {
                lexer_.NextToken();
                return ParseClassDefinition();  // NOLINT
            }
            if (tok.Is<TokenType::If>()) {
                return ParseCondition();
            }
            auto result = ParseSimpleStatement();
            lexer_.Expect<TokenType::Newline>();
            lexer_.NextToken();
            return result;
        }

        // StatementBody -> return Expression
        //               | print ExpressionList
        //               | AssignmentOrCall
        std::unique_ptr<ast::Statement> ParseSimpleStatement() {
            const auto& tok = lexer_.CurrentToken();

            if (tok.Is<TokenType::Return>()) {
                lexer_.NextToken();
                return std::make_unique<ast::Return>(ParseTest());
            }
            if (tok.Is<TokenType::Print>()) {
                lexer_.NextToken();
                std::vector<std::unique_ptr<ast::Statement>> args;
                if (!lexer_.CurrentToken().Is<TokenType::Newline>()) {
                    args = ParseTestList();
                }
                return std::make_unique<ast::Print>(std::move(args));
            }
            return ParseAssignmentOrCall();
        }

        parse::Lexer& lexer_;
        runtime::Closure declared_classes_;
    };

}  // namespace

std::unique_ptr<runtime::Executable> 
ParseProgram(parse::Lexer& lexer) {
    return Parser{ lexer }.ParseProgram();
}