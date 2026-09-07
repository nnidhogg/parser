# **Hopper**

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://github.com/nnidhogg/hopper/actions/workflows/ci.yml/badge.svg" alt="CI">
  <img src="https://github.com/nnidhogg/hopper/actions/workflows/codeql.yml/badge.svg" alt="CodeQL">
  <img src="https://codecov.io/gh/nnidhogg/hopper/branch/master/graph/badge.svg" alt="Coverage">
  <img src="https://img.shields.io/github/license/nnidhogg/hopper" alt="License">
</p>

`hopper` is a **C++23 library** for building **recursive-descent parsers** on top of
**[`munch`](https://github.com/nnidhogg/munch)** lexers. It supplies the parts every hand-written parser repeats and
nothing else: a **token stream with one token of lookahead** that discards trivia, **source tracking** so every node
carries the span it was parsed from with offsets indexing the original bytes, **structured errors** with a kind and
the span they point at, and **certified recovery** after a lexical error, inherited from munch under munch's own
contract. A grammar is a class that derives from the kit and writes its productions as methods; there is no grammar
language, no generated code and no runtime table.

Two grammars ship on the kit. **JSON**, complete to RFC 8259 and held to every accepted and rejected case of the
JSONTestSuite, parsed with an explicit stack so nesting depth is bounded by memory and not by the call stack. And the
**C-like study grammar**, the token set of one row of munch's recovery-quality campaign, with a parser that reads
keywords and multi-byte operators out of the campaign's coarse tokens the way a C lexer would have, so a parser over the
measured grammar exists beside the measurements.

## **Status: pre-1.0**

The kit is stable in shape and used by both grammars; the public names may still change before 1.0, after which the
versioning rule is munch's, additive within a major version. What is not here yet is the parser-level half of
certified resumption: after a lexical error the kit moves the stream to munch's next certified token start, and what
a parser may assume about its own state at that point is the open question the JSON grammar was chosen to ask; see
[docs/design.md](docs/design.md).

## **Features**

- **A token reader over any munch lexer.** `parse::Token_reader<Kind>` wraps a `munch::core::Lexer` with one token of
  lookahead, a skip predicate for trivia, and locations that count `"\r\n"` and a lone `'\r'` as one newline each while
  offsets index the original bytes.
- **A parser base with the operations a recursive-descent parser repeats.** `parse::Parser_base<Kind>` gives peek,
  check, accept and expect over token kinds, `mark()` and `span_from()` to close a node's span, and three error raisers
  whose messages name what was expected.
- **Structured errors.** `parse::Parse_error` carries a kind, lexical, an unexpected token, an unexpected end, or an
  invalid literal, and the source span it points at, with line, column and byte offset.
- **Certified recovery.** `Parser_base::recover()` moves the stream past a lexical error to the next token start munch
  certifies, under complete-repair invariance: in every completely tokenizable repair of the text before the returned
  evidence, the answer begins a token. No repair is promised to exist, the next read may error again, and a call with a
  token buffered throws rather than drop it.
- **JSON.** `json::Parser` parses one RFC 8259 text into a `json::Value` tree: null, booleans, numbers kept as spelled
  with a conversion to double on request, strings unescaped to UTF-8 with surrogate pairs combined, arrays, and objects
  that keep members in document order with duplicates and answer a lookup with the last member of a name. The lexer's
  string interior is built from munch's UTF-8 code point ranges, so a string that is not well-formed UTF-8 never
  tokenizes.
- **Edits that relex only what they reach.** `json::Document` keeps a text and its token stream and brings the stream
  current after an edit by rescanning from the last certified token start before it to the first boundary after it that
  the old stream shared, the edit theorem of the certified-splitting report as a type; every edit reports how many bytes
  it reread.
- **The C-like study grammar.** `clike::Parser` parses expressions, statements and translation units over the campaign's
  seven token kinds; its language is decimal integers, strings without escapes, booleans, the C operator ladder with
  assignment and the ternary, calls, subscripts, member access, the four named casts, the fundamental types with
  `const`, pointers and references, and `if`, `while`, `for`, `do`, `return`, blocks and declarations.

## **Architecture Overview**

```
munch::core::Lexer            the automaton; every token hopper sees comes from here
        |
parse::Token_reader<Kind>     one-token lookahead, trivia discarded, locations tracked
        |
parse::Parser_base<Kind>      peek / accept / expect, spans, errors, recover()
        |
json::Parser   clike::Parser  the grammars, each a class of productions
        |
json::Value    clike::ast     the trees, every node with its span
```

## **Usage Overview**

### **Parsing JSON**

```cpp
#include <iostream>
#include <string>

#include <hopper/json/parser.hpp>
#include <hopper/parse/parse_error.hpp>

int main()
{
    const std::string text{R"({"name": "hopper", "tags": ["json", "clike"], "stable": false})"};

    try
    {
        hopper::json::Parser parser{text};

        const auto document{parser.parse()};

        const auto& object{document.as_object()};

        std::cout << object.find("name")->as_string() << " has "
                  << object.find("tags")->as_array().elements.size() << " tags\n";

        // Every value knows where it came from: byte offsets into the original text, and a line and column.
        const auto& stable{*object.find("stable")};

        std::cout << "stable spans bytes " << stable.span.begin.offset << " to " << stable.span.end.offset << '\n';
    }
    catch (const hopper::parse::Parse_error& error)
    {
        std::cerr << error.what() << " at " << error.span().begin.line << ':' << error.span().begin.column << '\n';
    }
}
```

`parse()` accepts exactly one JSON text with nothing but whitespace around it, and raises a `Parse_error` whose kind
says what went wrong: `Lexical` for bytes no token covers, a control character inside a string or an ill-formed UTF-8
sequence; `Unexpected_token` for a token out of place, trailing text included; `Unexpected_end` for an input that
ends inside a value; `Invalid_literal` for a `\u` escape that leaves a surrogate unpaired. Numbers are kept as the
document spelled them; `Number::to_double()` gives the nearest double, an infinity past the double range.

### **Parsing the C-like grammar**

```cpp
#include <hopper/clike/parser.hpp>

hopper::clike::Parser parser{std::string{"int total = (a << 2) + b[i] * -c;"}};

const auto statement{parser.parse_statement()};
```

`parse_expression()`, `parse_statement()` and `parse_translation_unit()` each parse the whole input as one construct
and refuse anything left over. The trees are plain structs under `hopper::clike::ast`, one `std::variant` per node
family, each node carrying its span.

### **Writing a grammar on the kit**

A parser derives from `parse::Parser_base<Kind>` over its own token kind, builds a `parse::Token_reader<Kind>` from a
munch lexer and a trivia predicate, and writes its productions as methods:

```cpp
class Parser : public hopper::parse::Parser_base<Token_kind>
{
public:
    explicit Parser(const std::string& input)
        : Parser_base{hopper::parse::Token_reader<Token_kind>{lexer(), input, is_trivia}}
    {}

    Node parse_pair()
    {
        const auto begin{mark()};                     // where this node starts
        const auto key{expect(Token_kind::Name, "a name")};
        expect(Token_kind::Equals, "'=' after the name");
        const auto value{expect(Token_kind::Number, "a value")};
        return {.key = key.lexeme(), .value = value.lexeme(), .span = span_from(begin)};
    }
};
```

The base's `check(kind)`, `accept(kind)` and `expect(kind, what)` look at the next token; `syntax_error(message,
token)`, `eof_error(message)` and `lexical_error(message)` raise the three error kinds with the right span. Both
shipped grammars are written this way and are the reference for the style.

### **Editing a JSON text**

`json::Document` holds a text and its token stream, whitespace included, and keeps the stream current across edits by
relexing between certified positions rather than from the start:

```cpp
#include <hopper/json/document.hpp>

hopper::json::Document document{R"({"a": [1, 2, 3], "b": "text"})"};

const auto relex{document.edit(10, 1, "22")};    // replace one byte at offset 10 with "22"

// relex.rescanned is the bytes reread, a handful here; relex.whole is false unless the document had to start over.
// document.tokens() now equals the stream of the edited text tokenized whole.
```

The scan restarts at the last certified token start whose evidence the edit left untouched, since munch's certificate
promises a boundary there in every completely tokenizable text agreeing on that evidence, and stops at the first
boundary after the edit that the old stream also had. An edit that leaves the text incompletely tokenizable relexes
the whole text, and so does every edit through the one that repairs it. The saving on a real document is measured by
`tools/probes/hopper_edit_relex` and quoted in [docs/design.md](docs/design.md).

### **Error Recovery**

After a `Parse_error` of kind `Lexical`, the stream stands at the failing byte with nothing buffered, and
`recover()` asks munch for the next certified token start:

```cpp
try
{
    document = parser.parse();
}
catch (const hopper::parse::Parse_error& error)
{
    if (error.kind() == hopper::parse::Parse_error_kind::Lexical)
    {
        if (const auto start{parser.recover()})
        {
            // The stream now stands at start->start, a token start in every completely tokenizable repair of the
            // text before start->evidence_begin; what to parse from here is the grammar's decision.
        }
    }
}
```

The answer is munch's `Certified_start`, position and evidence interval, and the guarantee is exactly munch's; hopper
adds the location bookkeeping so spans after the skip stay right, and refuses the call when a token is buffered.

## **Getting Started**

### **Requirements**

- A C++23 compiler; GCC and Clang on Linux are the toolchains built and tested (GCC 13.3 and Clang 19 in CI), on
  x86-64 and 64-bit ARM. Clang 18 and older cannot compile munch's tokenizer, which every hopper parser reads through.
- CMake 3.20+.
- munch as the git submodule under `external/munch`, checked out at the release hopper builds against; googletest
  beside it for the tests. Everything else is the standard library.

### **Building the Project**

```bash
git clone --recurse-submodules https://github.com/nnidhogg/hopper
cd hopper
cmake -S . -B build
cmake --build build -j 8
```

The default `CMAKE_BUILD_TYPE` is `Release` when unset.

## **Testing**

Each library under `libs/` has a GoogleTest suite in a `tests/` subdirectory, registered with CTest:

```bash
cd build
ctest --output-on-failure
```

The JSON suite includes the 318 parsing cases of the JSONTestSuite, vendored under
`libs/json/tests/data/JSONTestSuite/` with their licence and provenance: every `y_` case must parse, every `n_` case
must be refused, and what the parser does on the `i_` cases is asserted rather than left to drift. Tests and
warnings-as-errors are enabled by default only when hopper is the top-level project; a build consuming hopper through
`add_subdirectory` opts in with `-DHOPPER_BUILD_TESTS=ON` or `-DHOPPER_WERROR=ON`. The probe under `tools/probes/`
is a self-checking executable registered with CTest as well; given a JSON file it prints the edit figures instead.

## **Directory Structure**

```
docs/                     design.md, the decisions behind the kit and the two grammars.
libs/
  parse/                  The kit: Token_reader, Token_lookahead, Token_location, Source_span, Parse_error,
                          Parser_base.
  json/                   The JSON grammar: tokens over munch, the explicit-stack Parser, the Value tree, the
                          edit-relexing Document; the conformance suite under tests/data.
  clike/                  The C-like study grammar: the campaign's tokens, the Parser with its operator fusion, the
                          ast structs.
tools/
  probes/                 hopper_edit_relex, the edit theorem run as a program over a generated corpus or a file.
external/
  munch/                  The lexer library, as a submodule pinned to a release.
  googletest/             The test framework.
```

## **Example CMake Integration**

```cmake
add_subdirectory(external/hopper)

target_link_libraries(your_target PRIVATE hopper::json)   # or hopper::clike, or hopper::parse for the kit alone
```

`hopper::hopper` carries all three. To install hopper, build it against an installed munch rather than the submodule,
`-DHOPPER_SYSTEM_MUNCH=ON -DHOPPER_INSTALL=ON`, so the exported targets refer to munch's own installed package; the
libraries, headers and a package config then install under the usual prefix, and a consumer writes
`find_package(hopper)` and links the same `hopper::` names.

## **Versioning and Stability**

hopper is pre-1.0: the kit's public names, `parse::Token_reader`, `parse::Parser_base`, `parse::Parse_error`,
`parse::Source_span` and their members, and the two grammars' `Parser` and tree types may still change before 1.0.
From 1.0 the rule is munch's: a minor release adds and never removes or renames on the stable surface named here,
and a major release is the only place a name disappears. The munch submodule is pinned to a release, and a hopper
release names the munch release it was built and tested against.

## **License**

MIT, see [LICENSE](LICENSE). The vendored JSONTestSuite cases are MIT as well; their notice is in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## **Author**

Developed and maintained by **Nicklas Nidhögg** GitHub: [nnidhogg](https://github.com/nnidhogg)
