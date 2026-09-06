#ifndef HOPPER_LIBS_CLIKE_INCLUDE_HOPPER_CLIKE_PARSER_HPP
#define HOPPER_LIBS_CLIKE_INCLUDE_HOPPER_CLIKE_PARSER_HPP

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "hopper/clike/ast/expr.hpp"
#include "hopper/clike/ast/stmt.hpp"
#include "hopper/clike/ast/unit.hpp"
#include "hopper/clike/tokens.hpp"
#include "hopper/parse/parser_base.hpp"
#include "hopper/parse/source_span.hpp"
#include "hopper/parse/token_reader.hpp"

namespace hopper::clike
{
/**
 * @brief A recursive-descent, precedence-climbing parser for the C-like study grammar's expressions, statements and
 *        translation units.
 *
 * The lexer delivers the campaign's tokens and nothing finer, so the parser recognizes what a C lexer would have:
 * keywords are identifiers with a reserved spelling, and a multi-byte operator is a run of adjacent operator bytes,
 * fused by the longest spelling the operator table knows, which is maximal munch applied one level up. The language
 * is the study grammar's: decimal integers, strings without escapes, booleans, the C operator set, the fundamental
 * types with const, pointers and references, and the statements a C body is made of.
 */
class Parser : public parse::Parser_base<Token_kind>
{
public:
    /**
     * @brief The token reader type this parser consumes.
     */
    using Token_reader_t = parse::Token_reader<Token_kind>;

    /**
     * @brief Constructs a parser over a prepared reader.
     * @param reader The reader, whose lexer is expected to be lexer() with trivia discarded.
     */
    explicit Parser(Token_reader_t reader);

    /**
     * @brief Constructs a parser over a text held in memory, using lexer() and discarding trivia.
     * @param input The source text.
     */
    explicit Parser(const std::string& input);

    /**
     * @brief Constructs a parser over a file's contents, using lexer() and discarding trivia.
     * @param file The file to read.
     */
    explicit Parser(const std::filesystem::path& file);

    using parse::Parser_base<Token_kind>::load;
    using parse::Parser_base<Token_kind>::reset;
    using parse::Parser_base<Token_kind>::recover;

    /**
     * @brief Parses the whole input as one expression.
     * @return The expression tree.
     * @throws parse::Parse_error On a lexical error, a token out of place, an input ending inside the expression,
     *         or input left after it.
     */
    [[nodiscard]] ast::Expr parse_expression();

    /**
     * @brief Parses the whole input as one statement.
     * @return The statement tree.
     * @throws parse::Parse_error As parse_expression() does.
     */
    [[nodiscard]] ast::Stmt parse_statement();

    /**
     * @brief Parses the whole input as a translation unit: a sequence of declarations and function definitions.
     * @return The unit.
     * @throws parse::Parse_error As parse_expression() does.
     */
    [[nodiscard]] ast::Translation_unit parse_translation_unit();

private:
    /**
     * @brief A multi-byte operator the parser has fused from adjacent operator tokens and not yet consumed.
     *
     * The fused tokens are already taken from the reader, so the parser keeps the operator here as the current token
     * and remembers where the input stood before it, which is where a span that ends before the operator closes.
     */
    struct Operator
    {
        std::string spelling;
        parse::Source_span span;
        parse::Source_position before;
    };

    /**
     * @brief Where the next token begins, the fused operator included.
     * @return The position the next construct's span starts at.
     */
    [[nodiscard]] parse::Source_position here();

    /**
     * @brief The span from a position to the end of what has been consumed, a fused but unconsumed operator excluded.
     * @param begin Where the construct began.
     * @return The construct's span.
     */
    [[nodiscard]] parse::Source_span close(const parse::Source_position& begin) const noexcept;

    /**
     * @brief The operator at the current position, fused to its longest spelling, or nothing when the current token
     *        is not an operator byte.
     *
     * Adjacent operator bytes are joined while the joined spelling is a prefix of an operator the table knows, so
     * `<<=` is one operator and `a+-b` is `a + (-b)`; the result is kept as the current token until consumed.
     * @return The fused operator.
     */
    [[nodiscard]] std::optional<Operator> peek_operator();

    /**
     * @brief Consumes the fused operator peek_operator() produced.
     */
    void take_operator() noexcept;

    /**
     * @brief Whether the current token is the operator with the given spelling.
     */
    [[nodiscard]] bool check_operator(std::string_view spelling);

    /**
     * @brief Consumes the operator with the given spelling if it is the current token.
     * @return True when consumed.
     */
    [[nodiscard]] bool accept_operator(std::string_view spelling);

    /**
     * @brief Consumes the operator with the given spelling or raises the syntax error naming what was expected.
     */
    void expect_operator(std::string_view spelling, std::string_view what);

    /**
     * @brief Whether the current token is the punctuation byte.
     */
    [[nodiscard]] bool check_punctuation(char byte);

    /**
     * @brief Consumes the punctuation byte if it is the current token.
     * @return True when consumed.
     */
    [[nodiscard]] bool accept_punctuation(char byte);

    /**
     * @brief Consumes the punctuation byte or raises the syntax error naming what was expected.
     */
    void expect_punctuation(char byte, std::string_view what);

    /**
     * @brief Whether the current token is the identifier with the given reserved spelling.
     */
    [[nodiscard]] bool check_keyword(std::string_view word);

    /**
     * @brief Consumes the keyword if it is the current token.
     * @return True when consumed.
     */
    [[nodiscard]] bool accept_keyword(std::string_view word);

    /**
     * @brief Consumes the keyword or raises the syntax error naming what was expected.
     */
    void expect_keyword(std::string_view word, std::string_view what);

    /**
     * @brief Consumes an identifier that is not a keyword, if the current token is one.
     * @return The token, or nothing.
     */
    [[nodiscard]] std::optional<Token_t> accept_identifier();

    /**
     * @brief Consumes an identifier that is not a keyword or raises the syntax error naming what was expected.
     * @return The token.
     */
    Token_t expect_identifier(std::string_view what);

    /**
     * @brief Whether input remains, the fused operator included.
     */
    [[nodiscard]] bool more();

    /**
     * @brief Raises the syntax error for the current token, whatever its kind, naming what was expected.
     */
    [[noreturn]] void unexpected(std::string_view what);

    /**
     * @brief Parses an assignment expression: a conditional expression, optionally assigned to with one of the
     * assignment operators, right-associatively.
     * @return The expression.
     */
    [[nodiscard]] ast::Expr parse_assignment();
    /**
     * @brief Parses a conditional expression: a binary expression, optionally followed by `?`, an assignment
     * expression, `:` and another.
     * @return The expression.
     */
    [[nodiscard]] ast::Expr parse_ternary();
    /**
     * @brief Parses a run of binary operators by precedence climbing, left-associatively within a level.
     * @param min_precedence The lowest precedence this call may consume; an operator below it ends the run.
     * @return The expression.
     */
    [[nodiscard]] ast::Expr parse_binary(int min_precedence);
    /**
     * @brief Parses prefix operators applied to a postfix expression.
     * @return The expression.
     */
    [[nodiscard]] ast::Expr parse_unary();
    /**
     * @brief Parses a primary expression followed by calls, subscripts, member accesses and postfix increments.
     * @return The expression.
     */
    [[nodiscard]] ast::Expr parse_postfix();
    /**
     * @brief Parses a literal, a name, a named cast or a parenthesized expression, or raises the error naming what was
     * expected.
     * @return The expression.
     */
    [[nodiscard]] ast::Expr parse_primary();

    /**
     * @brief Parses one statement of any kind, the compound statement and the declaration included.
     * @return The statement.
     */
    [[nodiscard]] ast::Stmt parse_statement_node();
    /**
     * @brief Parses a brace-enclosed block of statements.
     * @return The compound statement.
     */
    [[nodiscard]] ast::Stmt parse_compound_statement();
    /**
     * @brief Parses an if statement, a dangling else binding to the nearest if.
     * @return The statement.
     */
    [[nodiscard]] ast::Stmt parse_if_statement();
    /**
     * @brief Parses a while statement.
     * @return The statement.
     */
    [[nodiscard]] ast::Stmt parse_while_statement();
    /**
     * @brief Parses a for statement whose initializer is a declaration, an expression or empty, with an optional
     * condition and step.
     * @return The statement.
     */
    [[nodiscard]] ast::Stmt parse_for_statement();
    /**
     * @brief Parses a do/while statement.
     * @return The statement.
     */
    [[nodiscard]] ast::Stmt parse_do_statement();
    /**
     * @brief Parses a return statement with or without a value.
     * @return The statement.
     */
    [[nodiscard]] ast::Stmt parse_return_statement();

    /**
     * @brief Whether the current token opens a declaration: `const` or a fundamental type name.
     * @return True when a declaration starts here.
     */
    [[nodiscard]] bool is_declaration_start();
    /**
     * @brief Parses a declaration of one or more declarators over one type, ended by `;`.
     * @return The declaration statement.
     */
    [[nodiscard]] ast::Stmt parse_declaration_statement();
    /**
     * @brief Parses a fundamental type name with `const` before or after it, refusing it twice.
     * @return The type.
     */
    [[nodiscard]] ast::Type parse_type_specifier();
    /**
     * @brief Parses a type specifier followed by pointer stars and an optional reference, as a cast names its type.
     * @return The type id.
     */
    [[nodiscard]] ast::Type_id parse_type_id();
    /**
     * @brief Parses pointer stars, an optional reference, a name and an optional initializer.
     * @return The declarator.
     */
    [[nodiscard]] ast::Declarator parse_declarator();

    /**
     * @brief Parses one item of a translation unit: a declaration, or a function prototype or definition.
     * @return The item's node.
     */
    [[nodiscard]] ast::Translation_unit::Item::Node_t parse_external_declaration();
    /**
     * @brief Parses a function's parameter list and its body or terminating `;`, the return type and name already read.
     * @param type The return type.
     * @param pointers The pointer depth of the return type.
     * @param reference Whether the return type is a reference.
     * @param name The function's name.
     * @return The function.
     */
    [[nodiscard]] ast::Function parse_function(ast::Type type, std::size_t pointers, bool reference, std::string name);
    /**
     * @brief Parses one parameter: a type, pointer stars, an optional reference, an optional name and an optional
     * default.
     * @return The parameter.
     */
    [[nodiscard]] ast::Parameter parse_parameter();

    std::optional<Operator> pending_;
};
} // namespace hopper::clike

#endif // HOPPER_LIBS_CLIKE_INCLUDE_HOPPER_CLIKE_PARSER_HPP
