#include "hopper/json/value.hpp"

#include <charconv>
#include <cstddef>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace hopper::json
{
namespace
{
/**
 * @brief Whether a spelling from_chars refused as out of range lies above the double range rather than below it.
 *
 * from_chars refuses exactly the spellings whose magnitude exceeds the largest double or falls under the smallest
 * subnormal, and leaves the value untouched, so the side is decided here from the decimal exponent the spelling
 * carries: the count of integer digits, or minus the count of leading fraction zeros when the integer part is zero,
 * plus the written exponent, saturated so an exponent of any length is read.
 * @param text The spelling, already known to match the RFC grammar.
 * @return True when the magnitude is above the range, false when it is below.
 */
bool above_range(const std::string_view text)
{
    std::size_t at{text.front() == '-' ? 1U : 0U};

    long long decimal_exponent{0};

    if (text[at] == '0')
    {
        ++at;

        if (at < text.size() && text[at] == '.')
        {
            ++at;

            while (at < text.size() && text[at] == '0')
            {
                --decimal_exponent;
                ++at;
            }
        }
    }
    else
    {
        while (at < text.size() && text[at] >= '0' && text[at] <= '9')
        {
            ++decimal_exponent;
            ++at;
        }
    }

    const auto exponent_at{text.find_first_of("eE")};

    if (exponent_at != std::string_view::npos)
    {
        auto digit{exponent_at + 1};

        const bool negative{text[digit] == '-'};

        if (text[digit] == '-' || text[digit] == '+')
        {
            ++digit;
        }

        long long written{0};

        for (; digit < text.size(); ++digit)
        {
            written = written > 1'000'000'000 ? written : written * 10 + (text[digit] - '0');
        }

        decimal_exponent += negative ? -written : written;
    }

    return decimal_exponent > 0;
}

/**
 * @brief Moves the children of a node onto the worklist and leaves the node childless.
 * @param node The node whose children are detached.
 * @param pending The worklist the children's nodes are moved onto.
 */
void detach_children(Value::Node_t& node, std::vector<Value::Node_t>& pending)
{
    if (auto* const array{std::get_if<Array>(&node)})
    {
        for (auto& element : array->elements)
        {
            pending.push_back(std::move(element.node));
        }

        array->elements.clear();
    }
    else if (auto* const object{std::get_if<Object>(&node)})
    {
        for (auto& member : object->members)
        {
            pending.push_back(std::move(member.value.node));
        }

        object->members.clear();
    }
}
} // namespace

double Number::to_double() const
{
    double value{0.0};

    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), value)};

    if (error == std::errc::result_out_of_range)
    {
        const double magnitude{above_range(text) ? std::numeric_limits<double>::infinity() : 0.0};

        return text.front() == '-' ? -magnitude : magnitude;
    }

    return value;
}

Value::Value(Node_t node, parse::Source_span span) : node{std::move(node)}, span{span}
{}

Value::~Value()
{
    std::vector<Node_t> pending;

    detach_children(node, pending);

    while (!pending.empty())
    {
        auto current{std::move(pending.back())};

        pending.pop_back();

        detach_children(current, pending);
    }
}

bool Array::operator==(const Array& other) const
{
    return elements == other.elements;
}

const Value* Object::find(const std::string_view name) const noexcept
{
    for (auto it{members.rbegin()}; it != members.rend(); ++it)
    {
        if (it->name == name)
        {
            return &it->value;
        }
    }

    return nullptr;
}

bool Object::operator==(const Object& other) const
{
    return members == other.members;
}
} // namespace hopper::json
