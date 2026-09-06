#include <charconv>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "hopper/clike/binary_operator.hpp"
#include "hopper/clike/parser.hpp"

namespace hopper::clike
{
namespace
{
/**
 * @brief Wraps an operand in a unary node.
 * @param op The operator applied.
 * @param operand The operand.
 * @return The node, its span unset.
 */
ast::Expr make_unary(const ast::Unary_op op, ast::Expr operand)
{
    return {.node = ast::Unary{.op = op, .operand = std::make_unique<ast::Expr>(std::move(operand))}};
}

/**
 * @brief Joins two operands in a binary node.
 * @param op The operator applied.
 * @param lhs The left operand.
 * @param rhs The right operand.
 * @return The node, its span unset.
 */
ast::Expr make_binary(const ast::Binary_op op, ast::Expr lhs, ast::Expr rhs)
{
    return {.node = ast::Binary{
                    .op = op,
                    .lhs = std::make_unique<ast::Expr>(std::move(lhs)),
                    .rhs = std::make_unique<ast::Expr>(std::move(rhs)),
            }};
}

/**
 * @brief Joins a target and a value in an assignment node.
 * @param op The assignment performed.
 * @param target The assigned-to expression.
 * @param value The assigned value.
 * @return The node, its span unset.
 */
ast::Expr make_assign(const ast::Assign_op op, ast::Expr target, ast::Expr value)
{
    return {.node = ast::Assign{
                    .op = op,
                    .target = std::make_unique<ast::Expr>(std::move(target)),
                    .value = std::make_unique<ast::Expr>(std::move(value)),
            }};
}

/**
 * @brief The assignment an operator spelling performs.
 * @param spelling The fused operator spelling.
 * @return The assignment, or std::nullopt when the spelling is not an assignment operator.
 */
std::optional<ast::Assign_op> assign_operator_for(const std::string_view spelling)
{
    if (spelling == "=")
    {
        return ast::Assign_op::Assign;
    }

    if (spelling == "+=")
    {
        return ast::Assign_op::Add;
    }

    if (spelling == "-=")
    {
        return ast::Assign_op::Subtract;
    }

    if (spelling == "*=")
    {
        return ast::Assign_op::Multiply;
    }

    if (spelling == "/=")
    {
        return ast::Assign_op::Divide;
    }

    if (spelling == "%=")
    {
        return ast::Assign_op::Modulo;
    }

    if (spelling == "&=")
    {
        return ast::Assign_op::Bitwise_and;
    }

    if (spelling == "|=")
    {
        return ast::Assign_op::Bitwise_or;
    }

    if (spelling == "^=")
    {
        return ast::Assign_op::Bitwise_xor;
    }

    if (spelling == "<<=")
    {
        return ast::Assign_op::Shift_left;
    }

    if (spelling == ">>=")
    {
        return ast::Assign_op::Shift_right;
    }

    return std::nullopt;
}

/**
 * @brief The cast a keyword selects.
 * @param word The identifier's spelling.
 * @return The cast kind, or std::nullopt when the word is not one of the four cast keywords.
 */
std::optional<ast::Cast_kind> cast_kind_for(const std::string_view word)
{
    if (word == "static_cast")
    {
        return ast::Cast_kind::Static;
    }

    if (word == "dynamic_cast")
    {
        return ast::Cast_kind::Dynamic;
    }

    if (word == "const_cast")
    {
        return ast::Cast_kind::Const;
    }

    if (word == "reinterpret_cast")
    {
        return ast::Cast_kind::Reinterpret;
    }

    return std::nullopt;
}
} // namespace

ast::Expr Parser::parse_assignment()
{
    const auto begin{here()};

    auto expr{parse_ternary()};

    const auto op{peek_operator()};

    const auto assign{op ? assign_operator_for(op->spelling) : std::nullopt};

    if (!assign)
    {
        return expr;
    }

    take_operator();

    auto node{make_assign(*assign, std::move(expr), parse_assignment())};

    node.span = close(begin);

    return node;
}

ast::Expr Parser::parse_ternary()
{
    const auto begin{here()};

    auto condition{parse_binary(1)};

    if (!accept_punctuation('?'))
    {
        return condition;
    }

    auto then_branch{parse_assignment()};

    expect_punctuation(':', "':' in the conditional expression");

    auto else_branch{parse_assignment()};

    return {.node =
                    ast::Ternary{
                            .condition = std::make_unique<ast::Expr>(std::move(condition)),
                            .then_branch = std::make_unique<ast::Expr>(std::move(then_branch)),
                            .else_branch = std::make_unique<ast::Expr>(std::move(else_branch)),
                    },
            .span = close(begin)};
}

ast::Expr Parser::parse_binary(const int min_precedence)
{
    const auto begin{here()};

    auto expr{parse_unary()};

    for (;;)
    {
        const auto op{peek_operator()};

        const auto info{op ? binary_operator_for(op->spelling) : std::nullopt};

        if (!info || info->precedence < min_precedence)
        {
            return expr;
        }

        take_operator();

        expr = make_binary(info->op, std::move(expr), parse_binary(info->precedence + 1));

        expr.span = close(begin);
    }
}

ast::Expr Parser::parse_unary()
{
    const auto begin{here()};

    const auto prefixed{[this, &begin](const ast::Unary_op op) {
        auto expr{make_unary(op, parse_unary())};

        expr.span = close(begin);

        return expr;
    }};

    if (accept_operator("+"))
    {
        return prefixed(ast::Unary_op::Plus);
    }

    if (accept_operator("-"))
    {
        return prefixed(ast::Unary_op::Minus);
    }

    if (accept_operator("!"))
    {
        return prefixed(ast::Unary_op::Not);
    }

    if (accept_operator("~"))
    {
        return prefixed(ast::Unary_op::Bitwise_not);
    }

    if (accept_operator("++"))
    {
        return prefixed(ast::Unary_op::Pre_increment);
    }

    if (accept_operator("--"))
    {
        return prefixed(ast::Unary_op::Pre_decrement);
    }

    if (accept_operator("&"))
    {
        return prefixed(ast::Unary_op::Address_of);
    }

    if (accept_operator("*"))
    {
        return prefixed(ast::Unary_op::Dereference);
    }

    return parse_postfix();
}

ast::Expr Parser::parse_postfix()
{
    const auto begin{here()};

    auto expr{parse_primary()};

    for (;;)
    {
        if (accept_punctuation('('))
        {
            std::vector<ast::Expr> arguments;

            if (!check_punctuation(')'))
            {
                arguments.push_back(parse_assignment());

                while (accept_punctuation(','))
                {
                    arguments.push_back(parse_assignment());
                }
            }

            expect_punctuation(')', "')' to close the argument list");

            expr = {.node = ast::Call{
                            .callee = std::make_unique<ast::Expr>(std::move(expr)),
                            .arguments = std::move(arguments),
                    }};

            expr.span = close(begin);
        }
        else if (accept_punctuation('['))
        {
            auto index{parse_assignment()};

            expect_punctuation(']', "']' to close the subscript");

            expr = {.node = ast::Subscript{
                            .object = std::make_unique<ast::Expr>(std::move(expr)),
                            .index = std::make_unique<ast::Expr>(std::move(index)),
                    }};

            expr.span = close(begin);
        }
        else if (accept_punctuation('.'))
        {
            const auto member{expect_identifier("a member name after '.'")};

            expr = {.node = ast::Member{
                            .op = ast::Member_op::Dot,
                            .object = std::make_unique<ast::Expr>(std::move(expr)),
                            .member = std::string{member.lexeme()},
                    }};

            expr.span = close(begin);
        }
        else if (accept_operator("->"))
        {
            const auto member{expect_identifier("a member name after '->'")};

            expr = {.node = ast::Member{
                            .op = ast::Member_op::Arrow,
                            .object = std::make_unique<ast::Expr>(std::move(expr)),
                            .member = std::string{member.lexeme()},
                    }};

            expr.span = close(begin);
        }
        else if (accept_operator("++"))
        {
            expr = {.node = ast::Postfix{
                            .op = ast::Postfix_op::Increment,
                            .operand = std::make_unique<ast::Expr>(std::move(expr))}};

            expr.span = close(begin);
        }
        else if (accept_operator("--"))
        {
            expr = {.node = ast::Postfix{
                            .op = ast::Postfix_op::Decrement,
                            .operand = std::make_unique<ast::Expr>(std::move(expr))}};

            expr.span = close(begin);
        }
        else
        {
            return expr;
        }
    }
}

ast::Expr Parser::parse_primary()
{
    const auto begin{here()};

    if (!pending_)
    {
        if (const auto token{accept(Token_kind::Number)})
        {
            const auto lexeme{token->lexeme()};

            long long value{};

            const auto [end, error]{std::from_chars(lexeme.data(), lexeme.data() + lexeme.size(), value)};

            if (error != std::errc{} || end != lexeme.data() + lexeme.size())
            {
                syntax_error("Integer literal is out of range", *token);
            }

            return {.node = ast::Int_literal{.value = value}, .span = close(begin)};
        }

        if (const auto token{accept(Token_kind::String)})
        {
            const auto lexeme{token->lexeme()};

            return {.node = ast::String_literal{.value = std::string{lexeme.substr(1, lexeme.size() - 2)}},
                    .span = close(begin)};
        }
    }

    if (accept_keyword("true"))
    {
        return {.node = ast::Bool_literal{.value = true}, .span = close(begin)};
    }

    if (accept_keyword("false"))
    {
        return {.node = ast::Bool_literal{.value = false}, .span = close(begin)};
    }

    if (const auto token{accept_identifier()})
    {
        return {.node = ast::Name{.identifier = std::string{token->lexeme()}}, .span = close(begin)};
    }

    if (!pending_)
    {
        if (const auto token{peek_token()}; token && token->kind() == Token_kind::Identifier)
        {
            if (const auto kind{cast_kind_for(token->lexeme())})
            {
                (void)next_token();

                expect_operator("<", "'<' after the cast keyword");

                auto type{parse_type_id()};

                expect_operator(">", "'>' to close the cast type");
                expect_punctuation('(', "'(' to open the cast operand");

                auto operand{parse_assignment()};

                expect_punctuation(')', "')' to close the cast operand");

                return {.node =
                                ast::Cast{
                                        .kind = *kind,
                                        .type = type,
                                        .operand = std::make_unique<ast::Expr>(std::move(operand)),
                                },
                        .span = close(begin)};
            }
        }
    }

    if (accept_punctuation('('))
    {
        auto expr{parse_assignment()};

        expect_punctuation(')', "')' to close '('");

        expr.span = close(begin);

        return expr;
    }

    unexpected("an expression");
}
} // namespace hopper::clike
