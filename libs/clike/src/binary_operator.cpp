#include "hopper/clike/binary_operator.hpp"

#include <array>

namespace hopper::clike
{
namespace
{
/**
 * @brief Every operator spelling the language knows, the assignment and unary forms included, for prefix fusion.
 */
constexpr std::array<std::string_view, 34> operators{
        "+",  "-",  "*",  "/",  "%",  "<",  ">",  "=",  "!",  "&",  "|",  "^",  "~",  "++", "--", "==",  "!=",
        "<=", ">=", "<<", ">>", "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "->", "<<=", ">>="};
} // namespace

std::optional<Binary_operator> binary_operator_for(const std::string_view spelling)
{
    if (spelling == "||")
    {
        return Binary_operator{1, ast::Binary_op::Logical_or};
    }

    if (spelling == "&&")
    {
        return Binary_operator{2, ast::Binary_op::Logical_and};
    }

    if (spelling == "|")
    {
        return Binary_operator{3, ast::Binary_op::Bitwise_or};
    }

    if (spelling == "^")
    {
        return Binary_operator{4, ast::Binary_op::Bitwise_xor};
    }

    if (spelling == "&")
    {
        return Binary_operator{5, ast::Binary_op::Bitwise_and};
    }

    if (spelling == "==")
    {
        return Binary_operator{6, ast::Binary_op::Equal};
    }

    if (spelling == "!=")
    {
        return Binary_operator{6, ast::Binary_op::Not_equal};
    }

    if (spelling == "<")
    {
        return Binary_operator{7, ast::Binary_op::Less};
    }

    if (spelling == ">")
    {
        return Binary_operator{7, ast::Binary_op::Greater};
    }

    if (spelling == "<=")
    {
        return Binary_operator{7, ast::Binary_op::Less_equal};
    }

    if (spelling == ">=")
    {
        return Binary_operator{7, ast::Binary_op::Greater_equal};
    }

    if (spelling == "<<")
    {
        return Binary_operator{8, ast::Binary_op::Shift_left};
    }

    if (spelling == ">>")
    {
        return Binary_operator{8, ast::Binary_op::Shift_right};
    }

    if (spelling == "+")
    {
        return Binary_operator{9, ast::Binary_op::Add};
    }

    if (spelling == "-")
    {
        return Binary_operator{9, ast::Binary_op::Subtract};
    }

    if (spelling == "*")
    {
        return Binary_operator{10, ast::Binary_op::Multiply};
    }

    if (spelling == "/")
    {
        return Binary_operator{10, ast::Binary_op::Divide};
    }

    if (spelling == "%")
    {
        return Binary_operator{10, ast::Binary_op::Modulo};
    }

    return std::nullopt;
}

bool is_operator_prefix(const std::string_view spelling) noexcept
{
    for (const auto candidate : operators)
    {
        if (candidate.starts_with(spelling))
        {
            return true;
        }
    }

    return false;
}
} // namespace hopper::clike
