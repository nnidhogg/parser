#include "hopper/json/document.hpp"

#include <algorithm>
#include <cstddef>
#include <munch/core/lexer.hpp>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace hopper::json
{
namespace
{
/**
 * @brief The compiled JSON lexer, built once and shared by every document.
 * @return The lexer.
 */
const munch::core::Lexer& shared_lexer()
{
    static const munch::core::Lexer instance{lexer()};

    return instance;
}

/**
 * @brief How far before an edit the search for a certified start looks before giving up and relexing the whole text.
 *
 * A certified start is common in JSON, every structural byte is one, so the search rarely needs more than a token or
 * two; the budget only bounds the cost on a text that certifies nothing for a long stretch, such as one long string.
 */
constexpr std::size_t search_budget{4096};

} // namespace

Document::Document(std::string text) : text_{std::move(text)}, tokens_{}, complete_{false}
{
    relex_all();
}

bool Document::scan(const std::size_t from, std::vector<Token>& out) const
{
    const auto& lexer{shared_lexer()};

    const std::string_view view{text_};

    std::size_t at{from};

    while (at < view.size())
    {
        const auto match{lexer.tokenize<Token_kind>(view.substr(at))};

        if (!match.token || match.length == 0)
        {
            return false;
        }

        out.push_back({.kind = *match.token, .offset = at, .length = match.length});

        at += match.length;
    }

    return true;
}

Document::Relex Document::relex_all()
{
    tokens_.clear();

    complete_ = scan(0, tokens_);

    return {.rescanned = text_.size(), .whole = true};
}

Document::Relex Document::edit(const std::size_t offset, const std::size_t removed, const std::string_view inserted)
{
    if (offset > text_.size() || removed > text_.size() - offset)
    {
        throw std::out_of_range{"Document::edit: the range lies outside the text"};
    }

    // The anchor: the last certified start whose evidence the edit does not touch. The certificate walk only runs
    // forward, so the search starts a reach before the edit and keeps the last answer whose evidence ends at or
    // before it, widening the reach when a stretch certifies nothing, so a nearby anchor costs a short walk and only
    // a long certificate-free stretch pays for the whole budget.
    const auto& lexer{shared_lexer()};

    const std::string_view old_view{text_};

    std::optional<std::size_t> anchor;

    for (std::size_t reach{64}; !anchor && reach <= search_budget; reach *= 4)
    {
        for (auto from{offset > reach ? offset - reach : 0}; from <= offset;)
        {
            const auto found{lexer.next_certified_evidence(old_view, from)};

            if (!found || found->start > offset)
            {
                break;
            }

            if (found->evidence_end <= offset)
            {
                anchor = found->start;
            }

            from = found->start + 1;
        }

        if (reach >= offset)
        {
            break;
        }
    }

    const auto old_size{text_.size()};

    text_.replace(offset, removed, inserted);

    if (!complete_ || !anchor)
    {
        return relex_all();
    }

    // The old tokens before the anchor stay; the old tokens from the first boundary at or after the replaced range
    // are candidates to rejoin, shifted by the edit's size change.
    const auto first_after{
            std::ranges::lower_bound(tokens_, offset + removed, {}, [](const Token& token) { return token.offset; })};

    const auto keep_before{
            std::ranges::lower_bound(tokens_, *anchor, {}, [](const Token& token) { return token.offset; })};

    std::vector<Token> fresh(tokens_.begin(), keep_before);

    const std::string_view view{text_};

    const auto delta{static_cast<std::ptrdiff_t>(text_.size()) - static_cast<std::ptrdiff_t>(old_size)};

    const auto edit_end{offset + inserted.size()};

    auto rejoin{first_after};

    std::size_t at{*anchor};

    while (at < view.size())
    {
        // Past the edit, the first boundary the old stream also had is where the two scans agree from then on.
        if (at >= edit_end)
        {
            while (rejoin != tokens_.end() &&
                   static_cast<std::ptrdiff_t>(rejoin->offset) + delta < static_cast<std::ptrdiff_t>(at))
            {
                ++rejoin;
            }

            if (rejoin != tokens_.end() &&
                static_cast<std::ptrdiff_t>(rejoin->offset) + delta == static_cast<std::ptrdiff_t>(at))
            {
                for (auto it{rejoin}; it != tokens_.end(); ++it)
                {
                    fresh.push_back(
                            {.kind = it->kind,
                             .offset = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(it->offset) + delta),
                             .length = it->length});
                }

                tokens_ = std::move(fresh);

                return {.rescanned = at - *anchor, .whole = false};
            }
        }

        const auto match{lexer.tokenize<Token_kind>(view.substr(at))};

        if (!match.token || match.length == 0)
        {
            return relex_all();
        }

        fresh.push_back({.kind = *match.token, .offset = at, .length = match.length});

        at += match.length;
    }

    tokens_ = std::move(fresh);

    return {.rescanned = at - *anchor, .whole = false};
}

} // namespace hopper::json
