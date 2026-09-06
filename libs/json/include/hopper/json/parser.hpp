#ifndef HOPPER_LIBS_JSON_INCLUDE_HOPPER_JSON_PARSER_HPP
#define HOPPER_LIBS_JSON_INCLUDE_HOPPER_JSON_PARSER_HPP

#include <filesystem>
#include <munch/core/lexer.hpp>
#include <string>
#include <string_view>

#include "hopper/json/tokens.hpp"
#include "hopper/json/value.hpp"
#include "hopper/parse/parser_base.hpp"
#include "hopper/parse/token_reader.hpp"

namespace hopper::json
{
/**
 * @brief Parses one RFC 8259 JSON text into a Value tree.
 *
 * The parser keeps its own stack of open arrays and objects rather than recursing, so a document nested as deep as
 * memory allows parses without touching the call stack, and the stack is exactly the state a JSON parser carries: the
 * open containers, and for an object, the name waiting for its value. Every value carries its source span. Numbers
 * are kept as spelled; strings are unescaped to UTF-8, with surrogate pairs in \u escapes combined and a lone
 * surrogate refused.
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
     * @param reader The reader, whose lexer is expected to be lexer() with whitespace discarded.
     */
    explicit Parser(Token_reader_t reader);

    /**
     * @brief Constructs a parser over a text held in memory, using lexer() and discarding whitespace.
     * @param input The JSON text.
     */
    explicit Parser(const std::string& input);

    /**
     * @brief Constructs a parser over a file's contents, using lexer() and discarding whitespace.
     * @param file The file to read.
     */
    explicit Parser(const std::filesystem::path& file);

    using parse::Parser_base<Token_kind>::load;
    using parse::Parser_base<Token_kind>::reset;
    using parse::Parser_base<Token_kind>::recover;

    /**
     * @brief Parses the whole input as one JSON text: a single value with nothing but whitespace around it.
     * @return The value tree.
     * @throws parse::Parse_error On a lexical error, on a token out of place, on an input that ends inside a value,
     *         on trailing text after the value, and on a string whose \u escapes leave a surrogate unpaired.
     */
    [[nodiscard]] Value parse();

    /**
     * @brief Resolves the escapes of a string token's lexeme.
     * @param lexeme The token text, quotes included, already known to match the string grammar.
     * @param span The token's source span, named in the error a lone surrogate raises.
     * @return The string's characters as UTF-8.
     * @throws parse::Parse_error With kind Invalid_literal when a \u escape spells a surrogate that is not one half
     *         of a pair.
     */
    [[nodiscard]] static std::string unescape(std::string_view lexeme, const parse::Source_span& span);

private:
    /**
     * @brief Reads the next token or raises the end-of-input error naming what was expected.
     * @param what What the grammar expected, for the message.
     * @return The token.
     */
    [[nodiscard]] Token_t next_or_end(std::string_view what);

    /**
     * @brief Builds a scalar value from a token that spells one.
     * @param token The token, of a scalar kind.
     * @param span The token's source span.
     * @return The value.
     */
    [[nodiscard]] Value scalar(const Token_t& token, const parse::Source_span& span);
};
} // namespace hopper::json

#endif // HOPPER_LIBS_JSON_INCLUDE_HOPPER_JSON_PARSER_HPP
