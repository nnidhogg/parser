#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hopper/clike/parser.hpp"

namespace hopper::clike
{
namespace
{
/**
 * @brief The fundamental type a keyword names.
 * @param word The identifier's spelling.
 * @return The type kind, or std::nullopt when the word names no type.
 */
std::optional<ast::Type_kind> type_kind_for(const std::string_view word)
{
    if (word == "bool")
    {
        return ast::Type_kind::Bool;
    }

    if (word == "char")
    {
        return ast::Type_kind::Char;
    }

    if (word == "int")
    {
        return ast::Type_kind::Int;
    }

    if (word == "float")
    {
        return ast::Type_kind::Float;
    }

    if (word == "double")
    {
        return ast::Type_kind::Double;
    }

    if (word == "void")
    {
        return ast::Type_kind::Void;
    }

    return std::nullopt;
}
} // namespace

bool Parser::is_declaration_start()
{
    if (pending_)
    {
        return false;
    }

    const auto token{peek_token()};

    return token && token->kind() == Token_kind::Identifier &&
           (token->lexeme() == "const" || type_kind_for(token->lexeme()).has_value());
}

ast::Stmt Parser::parse_declaration_statement()
{
    const auto begin{here()};

    auto type{parse_type_specifier()};

    std::vector<ast::Declarator> declarators;

    declarators.push_back(parse_declarator());

    while (accept_punctuation(','))
    {
        declarators.push_back(parse_declarator());
    }

    expect_punctuation(';', "';' after the declaration");

    return {.node = ast::Declaration{.type = type, .declarators = std::move(declarators)}, .span = close(begin)};
}

ast::Type Parser::parse_type_specifier()
{
    auto is_const{accept_keyword("const")};

    if (pending_)
    {
        unexpected("a type name");
    }

    const auto token{next_token()};

    if (!token)
    {
        eof_error("Expected a type name before end of input");
    }

    const auto kind{token->kind() == Token_kind::Identifier ? type_kind_for(token->lexeme()) : std::nullopt};

    if (!kind)
    {
        syntax_error("Expected a type name", *token);
    }

    if (check_keyword("const"))
    {
        if (is_const)
        {
            unexpected("a declarator, not a second 'const'");
        }

        (void)accept_keyword("const");

        is_const = true;
    }

    return {.is_const = is_const, .kind = *kind};
}

ast::Type_id Parser::parse_type_id()
{
    const auto type{parse_type_specifier()};

    std::size_t pointers{0};

    while (accept_operator("*"))
    {
        ++pointers;
    }

    const bool reference{accept_operator("&")};

    return {.type = type, .pointers = pointers, .reference = reference};
}

ast::Declarator Parser::parse_declarator()
{
    std::size_t pointers{0};

    while (accept_operator("*"))
    {
        ++pointers;
    }

    const auto reference{accept_operator("&")};

    const auto name{expect_identifier("a declarator name")};

    std::optional<ast::Expr> initializer;

    if (accept_operator("="))
    {
        initializer = parse_assignment();
    }

    return {.pointers = pointers,
            .reference = reference,
            .name = std::string{name.lexeme()},
            .initializer = std::move(initializer)};
}
} // namespace hopper::clike
