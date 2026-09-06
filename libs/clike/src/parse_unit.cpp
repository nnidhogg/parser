#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "hopper/clike/parser.hpp"

namespace hopper::clike
{
ast::Translation_unit Parser::parse_translation_unit()
{
    ast::Translation_unit unit;

    while (more())
    {
        const auto begin{here()};

        auto node{parse_external_declaration()};

        unit.items.push_back({.node = std::move(node), .span = close(begin)});
    }

    return unit;
}

ast::Translation_unit::Item::Node_t Parser::parse_external_declaration()
{
    auto type{parse_type_specifier()};

    std::size_t pointers{0};

    while (accept_operator("*"))
    {
        ++pointers;
    }

    const auto reference{accept_operator("&")};

    const auto name{expect_identifier("a declarator name")};

    if (check_punctuation('('))
    {
        return parse_function(type, pointers, reference, std::string{name.lexeme()});
    }

    std::optional<ast::Expr> initializer;

    if (accept_operator("="))
    {
        initializer = parse_assignment();
    }

    std::vector<ast::Declarator> declarators;

    declarators.push_back(
            {.pointers = pointers,
             .reference = reference,
             .name = std::string{name.lexeme()},
             .initializer = std::move(initializer)});

    while (accept_punctuation(','))
    {
        declarators.push_back(parse_declarator());
    }

    expect_punctuation(';', "';' after the declaration");

    return ast::Declaration{.type = type, .declarators = std::move(declarators)};
}

ast::Function Parser::parse_function(
        const ast::Type type, const std::size_t pointers, const bool reference, std::string name)
{
    expect_punctuation('(', "'(' to open the parameter list");

    std::vector<ast::Parameter> parameters;

    if (!check_punctuation(')'))
    {
        parameters.push_back(parse_parameter());

        while (accept_punctuation(','))
        {
            parameters.push_back(parse_parameter());
        }
    }

    expect_punctuation(')', "')' to close the parameter list");

    if (accept_punctuation(';'))
    {
        return {.return_type = type,
                .pointers = pointers,
                .reference = reference,
                .name = std::move(name),
                .parameters = std::move(parameters),
                .body = nullptr};
    }

    auto body{parse_compound_statement()};

    return {.return_type = type,
            .pointers = pointers,
            .reference = reference,
            .name = std::move(name),
            .parameters = std::move(parameters),
            .body = std::make_unique<ast::Stmt>(std::move(body))};
}

ast::Parameter Parser::parse_parameter()
{
    auto type{parse_type_specifier()};

    std::size_t pointers{0};

    while (accept_operator("*"))
    {
        ++pointers;
    }

    const auto reference{accept_operator("&")};

    std::string name;

    if (const auto token{accept_identifier()})
    {
        name = std::string{token->lexeme()};
    }

    std::optional<ast::Expr> default_value;

    if (accept_operator("="))
    {
        default_value = parse_assignment();
    }

    return {.type = type,
            .pointers = pointers,
            .reference = reference,
            .name = std::move(name),
            .default_value = std::move(default_value)};
}
} // namespace hopper::clike
