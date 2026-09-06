#include "hopper/json/parser.hpp"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "hopper/parse/parse_error.hpp"

namespace hopper::json
{
namespace
{
/**
 * @brief One open container on the parser's stack.
 *
 * An array frame collects elements; an object frame collects members and carries the name whose value is being read.
 * The begin position is the opening bracket's, so the finished container's span can be closed at the closing one.
 */
struct Frame
{
    Value container;
    std::string name;
    parse::Source_position begin;
};

/**
 * @brief The numeric value of one hex digit, already known to be one.
 * @param digit The digit character.
 * @return Its value, zero to fifteen.
 */
std::uint32_t hex_value(const char digit)
{
    if (digit >= '0' && digit <= '9')
    {
        return static_cast<std::uint32_t>(digit - '0');
    }

    if (digit >= 'a' && digit <= 'f')
    {
        return static_cast<std::uint32_t>(digit - 'a' + 10);
    }

    return static_cast<std::uint32_t>(digit - 'A' + 10);
}

/**
 * @brief Appends one code point to a string as UTF-8.
 * @param out The string appended to.
 * @param code_point The scalar value, at most U+10FFFF and never a surrogate.
 */
void append_utf8(std::string& out, const std::uint32_t code_point)
{
    if (code_point < 0x80)
    {
        out.push_back(static_cast<char>(code_point));
    }
    else if (code_point < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else if (code_point < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

/**
 * @brief Whether a code unit is a high (leading) surrogate.
 * @param unit The code unit.
 * @return True for U+D800 to U+DBFF.
 */
bool is_high_surrogate(const std::uint32_t unit) noexcept
{
    return unit >= 0xD800 && unit <= 0xDBFF;
}

/**
 * @brief Whether a code unit is a low (trailing) surrogate.
 * @param unit The code unit.
 * @return True for U+DC00 to U+DFFF.
 */
bool is_low_surrogate(const std::uint32_t unit) noexcept
{
    return unit >= 0xDC00 && unit <= 0xDFFF;
}
} // namespace

Parser::Parser(Token_reader_t reader) : parse::Parser_base<Token_kind>{std::move(reader)}
{}

Parser::Parser(const std::string& input) : Parser{Token_reader_t{lexer(), input, is_trivia}}
{}

Parser::Parser(const std::filesystem::path& file) : Parser{Token_reader_t{lexer(), file, is_trivia}}
{}

std::string Parser::unescape(const std::string_view lexeme, const parse::Source_span& span)
{
    std::string out;

    out.reserve(lexeme.size());

    // The interior, between the quotes; the grammar guarantees every backslash starts a complete escape.
    const auto interior{lexeme.substr(1, lexeme.size() - 2)};

    for (std::size_t at{0}; at < interior.size(); ++at)
    {
        if (interior[at] != '\\')
        {
            out.push_back(interior[at]);
            continue;
        }

        ++at;

        switch (interior[at])
        {
        case '"':
            out.push_back('"');
            break;
        case '\\':
            out.push_back('\\');
            break;
        case '/':
            out.push_back('/');
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        default:
        {
            // A \u escape: four hex digits follow the u, and a high surrogate must be followed by a low one in a
            // second \u escape to name a character.
            const auto unit_at{[&](const std::size_t from) {
                return (hex_value(interior[from]) << 12) | (hex_value(interior[from + 1]) << 8) |
                       (hex_value(interior[from + 2]) << 4) | hex_value(interior[from + 3]);
            }};

            const auto unit{unit_at(at + 1)};

            at += 4;

            if (is_low_surrogate(unit))
            {
                throw parse::Parse_error{
                        parse::Parse_error_kind::Invalid_literal, span,
                        "Invalid string: a low surrogate escape with no high surrogate before it"};
            }

            if (!is_high_surrogate(unit))
            {
                append_utf8(out, unit);
                break;
            }

            const bool paired{
                    at + 6 < interior.size() && interior[at + 1] == '\\' && interior[at + 2] == 'u' &&
                    is_low_surrogate(unit_at(at + 3))};

            if (!paired)
            {
                throw parse::Parse_error{
                        parse::Parse_error_kind::Invalid_literal, span,
                        "Invalid string: a high surrogate escape with no low surrogate after it"};
            }

            const auto low{unit_at(at + 3)};

            at += 6;

            append_utf8(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
            break;
        }
        }
    }

    return out;
}

Parser::Token_t Parser::next_or_end(const std::string_view what)
{
    const auto token{next_token()};

    if (!token)
    {
        eof_error("Expected " + std::string(what) + " before end of input");
    }

    return *token;
}

Value Parser::scalar(const Token_t& token, const parse::Source_span& span)
{
    switch (token.kind())
    {
    case Token_kind::String:
        return Value{unescape(token.lexeme(), span), span};
    case Token_kind::Number:
        return Value{Number{.text = std::string{token.lexeme()}}, span};
    case Token_kind::True:
        return Value{true, span};
    case Token_kind::False:
        return Value{false, span};
    default:
        return Value{Null{}, span};
    }
}

Value Parser::parse()
{
    std::vector<Frame> stack;

    std::optional<Value> completed;

    while (true)
    {
        if (!completed)
        {
            // A value is due: either the document's, the next element's, or a member's after its colon.
            const auto begin{mark()};

            const auto token{next_or_end("a value")};

            switch (token.kind())
            {
            case Token_kind::Left_bracket:
                stack.push_back({.container = Value{Array{}, {}}, .name = {}, .begin = begin});

                if (accept(Token_kind::Right_bracket))
                {
                    completed = std::move(stack.back().container);
                    completed->span = span_from(begin);
                    stack.pop_back();
                }

                continue;
            case Token_kind::Left_brace:
                stack.push_back({.container = Value{Object{}, {}}, .name = {}, .begin = begin});

                if (accept(Token_kind::Right_brace))
                {
                    completed = std::move(stack.back().container);
                    completed->span = span_from(begin);
                    stack.pop_back();
                    continue;
                }

                {
                    const auto name_begin{mark()};
                    const auto name{expect(Token_kind::String, "a member name")};
                    stack.back().name = unescape(name.lexeme(), span_from(name_begin));
                    (void)expect(Token_kind::Colon, "':' after the member name");
                }

                continue;
            case Token_kind::String:
            case Token_kind::Number:
            case Token_kind::True:
            case Token_kind::False:
            case Token_kind::Null:
                completed = scalar(token, span_from(begin));
                continue;
            default:
                syntax_error("Expected a value", token);
            }
        }

        // A value is complete: it is the document, or it belongs to the open container on top of the stack.
        if (stack.empty())
        {
            if (const auto trailing{peek_token()})
            {
                syntax_error("Expected end of input after the value", *trailing);
            }

            return std::move(*completed);
        }

        auto& top{stack.back()};

        if (auto* const array{std::get_if<Array>(&top.container.node)})
        {
            array->elements.push_back(std::move(*completed));
            completed.reset();

            const auto token{next_or_end("',' or ']'")};

            if (token.kind() == Token_kind::Comma)
            {
                continue;
            }

            if (token.kind() != Token_kind::Right_bracket)
            {
                syntax_error("Expected ',' or ']'", token);
            }
        }
        else
        {
            auto& object{std::get<Object>(top.container.node)};

            object.members.push_back({.name = std::move(top.name), .value = std::move(*completed)});
            completed.reset();

            const auto token{next_or_end("',' or '}'")};

            if (token.kind() == Token_kind::Comma)
            {
                const auto name_begin{mark()};
                const auto name{expect(Token_kind::String, "a member name")};
                top.name = unescape(name.lexeme(), span_from(name_begin));
                (void)expect(Token_kind::Colon, "':' after the member name");
                continue;
            }

            if (token.kind() != Token_kind::Right_brace)
            {
                syntax_error("Expected ',' or '}'", token);
            }
        }

        // The container closed: it is the completed value for the frame below it.
        completed = std::move(top.container);
        completed->span = span_from(top.begin);
        stack.pop_back();
    }
}
} // namespace hopper::json
