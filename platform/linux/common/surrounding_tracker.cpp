#include "surrounding_tracker.h"

#include <limits>

namespace nakl::linux
{
namespace
{

constexpr std::size_t MaxSurroundingBytes = 1024 * 1024;

[[nodiscard]] std::optional<std::size_t> ScalarCount(std::string_view text) noexcept
{
    std::size_t count = 0;
    for (std::size_t index = 0; index < text.size(); ++count)
    {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t width = 0;
        char32_t scalar = 0;
        if (lead <= 0x7F)
        {
            width = 1;
            scalar = lead;
        }
        else if ((lead & 0xE0) == 0xC0)
        {
            width = 2;
            scalar = lead & 0x1F;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
            width = 3;
            scalar = lead & 0x0F;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
            width = 4;
            scalar = lead & 0x07;
        }
        else
        {
            return std::nullopt;
        }
        if (width > text.size() - index)
        {
            return std::nullopt;
        }
        for (std::size_t offset = 1; offset < width; ++offset)
        {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80)
            {
                return std::nullopt;
            }
            scalar = (scalar << 6) | (continuation & 0x3F);
        }
        const bool overlong = (width == 2 && scalar < 0x80) ||
                              (width == 3 && scalar < 0x800) ||
                              (width == 4 && scalar < 0x10000);
        if (overlong || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        {
            return std::nullopt;
        }
        index += width;
    }
    return count;
}

[[nodiscard]] std::optional<std::size_t> ByteOffset(std::string_view text,
                                                     std::size_t scalar_offset) noexcept
{
    std::size_t index = 0;
    while (index < text.size() && scalar_offset > 0)
    {
        ++index;
        while (index < text.size() &&
               (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80)
        {
            ++index;
        }
        --scalar_offset;
    }
    return scalar_offset == 0 ? std::optional<std::size_t>(index) : std::nullopt;
}

[[nodiscard]] std::string Utf8(char32_t scalar)
{
    std::string result;
    if (scalar <= 0x7F)
    {
        result.push_back(static_cast<char>(scalar));
    }
    else if (scalar <= 0x7FF)
    {
        result.push_back(static_cast<char>(0xC0 | (scalar >> 6)));
        result.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
    else if (scalar <= 0xFFFF)
    {
        result.push_back(static_cast<char>(0xE0 | (scalar >> 12)));
        result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
    else
    {
        result.push_back(static_cast<char>(0xF0 | (scalar >> 18)));
        result.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
    return result;
}

} // namespace

bool SurroundingTracker::Observe(std::string_view text, std::size_t cursor,
                                 std::size_t anchor)
{
    const auto scalar_count = text.size() <= MaxSurroundingBytes ? ScalarCount(text) : std::nullopt;
    if (!scalar_count.has_value() || cursor > *scalar_count || anchor > *scalar_count)
    {
        const bool changed = current_.valid || pending();
        Clear();
        return changed;
    }

    Snapshot observed{.text = std::string(text),
                      .cursor = cursor,
                      .anchor = anchor,
                      .valid = true};
    if (!current_.valid)
    {
        current_ = std::move(observed);
        intermediate_.reset();
        final_.reset();
        return false;
    }
    if (pending() && observed == current_)
    {
        // Some frontends repeat the last snapshot before delivering the
        // mutation caused by the key. Keep waiting for the predicted state.
        return false;
    }
    if (intermediate_.has_value() && observed == *intermediate_)
    {
        current_ = std::move(observed);
        intermediate_.reset();
        return false;
    }
    if (final_.has_value() && observed == *final_)
    {
        current_ = std::move(observed);
        intermediate_.reset();
        final_.reset();
        return false;
    }

    const bool changed = !(observed == current_);
    current_ = std::move(observed);
    intermediate_.reset();
    final_.reset();
    return changed;
}

void SurroundingTracker::Clear() noexcept
{
    current_ = {};
    intermediate_.reset();
    final_.reset();
}

bool SurroundingTracker::pending() const noexcept
{
    return intermediate_.has_value() || final_.has_value();
}

void SurroundingTracker::CancelPending() noexcept
{
    Clear();
}

bool SurroundingTracker::CanReplaceBefore(std::size_t scalar_count,
                                          std::string_view expected_before) const noexcept
{
    if (!current_.valid || pending() || current_.cursor != current_.anchor ||
        scalar_count > current_.cursor)
    {
        return false;
    }
    const auto start = ByteOffset(current_.text, current_.cursor - scalar_count);
    const auto end = ByteOffset(current_.text, current_.cursor);
    return start.has_value() && end.has_value() &&
           std::string_view(current_.text).substr(*start, *end - *start) == expected_before;
}

bool SurroundingTracker::ExpectPassThrough(InputEvent event)
{
    if (!current_.valid || pending() || current_.cursor != current_.anchor)
    {
        return false;
    }
    const auto cursor_byte = ByteOffset(current_.text, current_.cursor);
    if (!cursor_byte.has_value())
    {
        return false;
    }

    Snapshot expected = current_;
    if (event.kind == EventKind::Text)
    {
        const auto scalar = static_cast<std::uint32_t>(event.scalar);
        if (scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        {
            return false;
        }
        const std::string insertion = Utf8(event.scalar);
        expected.text.insert(*cursor_byte, insertion);
        ++expected.cursor;
        ++expected.anchor;
    }
    else if (event.kind == EventKind::Backspace && current_.cursor > 0)
    {
        const auto start = ByteOffset(current_.text, current_.cursor - 1);
        if (!start.has_value())
        {
            return false;
        }
        expected.text.erase(*start, *cursor_byte - *start);
        --expected.cursor;
        --expected.anchor;
    }
    else
    {
        return false;
    }
    final_ = std::move(expected);
    return true;
}

bool SurroundingTracker::ExpectReplacement(const EditAction &action)
{
    if (action.kind != EditAction::Kind::ReplaceBefore ||
        !CanReplaceBefore(action.delete_scalars, action.expected_before))
    {
        return false;
    }
    const auto start = ByteOffset(current_.text, current_.cursor - action.delete_scalars);
    const auto end = ByteOffset(current_.text, current_.cursor);
    const auto insert_scalars = ScalarCount(action.insert);
    if (!start.has_value() || !end.has_value() || !insert_scalars.has_value())
    {
        return false;
    }

    Snapshot deleted = current_;
    deleted.text.erase(*start, *end - *start);
    deleted.cursor -= action.delete_scalars;
    deleted.anchor = deleted.cursor;
    Snapshot committed = deleted;
    const auto committed_byte = ByteOffset(committed.text, committed.cursor);
    if (!committed_byte.has_value() ||
        action.insert.size() > MaxSurroundingBytes - committed.text.size())
    {
        return false;
    }
    committed.text.insert(*committed_byte, action.insert);
    committed.cursor += *insert_scalars;
    committed.anchor = committed.cursor;
    intermediate_ = std::move(deleted);
    final_ = std::move(committed);
    return true;
}

} // namespace nakl::linux
