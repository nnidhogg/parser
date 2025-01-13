#include "parser/idl/token_reader.hpp"

#include <fstream>

namespace parser::idl
{
Token_reader::Token_reader(lexer::core::Lexer lexer) : tokenizer_{std::move(lexer)}
{}

Token_reader::Token_reader(lexer::core::Lexer lexer, const std::string& input)
    : tokenizer_{std::move(lexer), normalize(input)}
{}

Token_reader::Token_reader(lexer::core::Lexer lexer, const std::filesystem::path& file)
    : tokenizer_{std::move(lexer), normalize(file)}
{}

void Token_reader::load(const std::string& input)
{
    tokenizer_.load(normalize(input));

    lookahead_.reset();
}

void Token_reader::load(const std::filesystem::path& file)
{
    tokenizer_.load(normalize(file));

    lookahead_.reset();
}

void Token_reader::reset() noexcept
{
    tokenizer_.reset();

    lookahead_.reset();
}

Token_reader::Result_t Token_reader::peek()
{
    if (lookahead_.token())
    {
        return lookahead_.token();
    }

    for (;;)
    {
        const auto expected{tokenizer_.next<Token_kind>()};

        if (!expected)
        {
            return expected;
        }

        const auto& optional{expected.value()};

        if (!optional)
        {
            return std::nullopt;
        }

        const auto& token{optional.value()};

        lookahead_.advance(token.kind(), token.lexeme());

        if (skip_token(token.kind()))
        {
            lookahead_.consume();

            continue;
        }

        return lookahead_.token();
    }
}

Token_reader::Result_t Token_reader::next()
{
    const auto expected{peek()};

    if (!expected)
    {
        return expected;
    }

    if (const auto& optional{expected.value()}; !optional)
    {
        return std::nullopt;
    }

    return lookahead_.consume();
}

const Token_location& Token_reader::location() const noexcept
{
    return lookahead_.location();
}

bool Token_reader::skip_token(const Token_kind kind) noexcept
{
    return kind == Token_kind::Whitespace || kind == Token_kind::Newline;
}

std::string Token_reader::normalize(const std::string& input)
{
    std::string output;

    output.reserve(input.size());

    for (auto iterator = input.cbegin(); iterator != input.cend();)
    {
        if (const char c = *iterator++; c == '\r')
        {
            if (iterator != input.cend() && *iterator == '\n')
            {
                ++iterator;
            }

            output.push_back('\n');
        }
        else
        {
            output.push_back(c);
        }
    }

    return output;
}

std::string Token_reader::normalize(const std::filesystem::path& file)
{
    return normalize(read(file));
}

std::string Token_reader::read(const std::filesystem::path& file)
{
    if (std::ifstream stream{file, std::ios::binary}; stream.is_open())
    {
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    throw std::runtime_error("Token_reader: cannot open file: " + file.string());
}

} // namespace parser::idl
