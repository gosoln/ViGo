#include "composition_session.h"

#include "nakl_core.h"

#include <new>
#include <string>
#include <utility>

namespace nakl::windows
{
namespace
{

[[nodiscard]] bool IsHighSurrogate(char16_t unit)
{
    return unit >= 0xD800 && unit <= 0xDBFF;
}

[[nodiscard]] bool IsLowSurrogate(char16_t unit)
{
    return unit >= 0xDC00 && unit <= 0xDFFF;
}

[[nodiscard]] std::optional<std::u16string> Utf8ToUtf16(const std::uint8_t *bytes,
                                                        std::size_t length)
{
    if (length == 0)
    {
        return std::u16string{};
    }
    if (bytes == nullptr)
    {
        return std::nullopt;
    }

    std::u16string output;
    output.reserve(length);
    std::size_t index = 0;
    while (index < length)
    {
        const std::uint8_t lead = bytes[index++];
        char32_t scalar = 0;
        std::size_t continuation_count = 0;
        if (lead <= 0x7F)
        {
            scalar = lead;
        }
        else if ((lead & 0xE0) == 0xC0)
        {
            scalar = lead & 0x1F;
            continuation_count = 1;
        }
        else if ((lead & 0xF0) == 0xE0)
        {
            scalar = lead & 0x0F;
            continuation_count = 2;
        }
        else if ((lead & 0xF8) == 0xF0)
        {
            scalar = lead & 0x07;
            continuation_count = 3;
        }
        else
        {
            return std::nullopt;
        }

        if (continuation_count > length - index)
        {
            return std::nullopt;
        }
        for (std::size_t offset = 0; offset < continuation_count; ++offset)
        {
            const std::uint8_t continuation = bytes[index++];
            if ((continuation & 0xC0) != 0x80)
            {
                return std::nullopt;
            }
            scalar = (scalar << 6) | (continuation & 0x3F);
        }

        const bool overlong = (continuation_count == 1 && scalar < 0x80) ||
                              (continuation_count == 2 && scalar < 0x800) ||
                              (continuation_count == 3 && scalar < 0x10000);
        if (overlong || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        {
            return std::nullopt;
        }

        if (scalar <= 0xFFFF)
        {
            output.push_back(static_cast<char16_t>(scalar));
        }
        else
        {
            scalar -= 0x10000;
            output.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
            output.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
        }
    }
    return output;
}

[[nodiscard]] std::optional<EditAction> ProcessHandle(NaklSession *session, InputEvent event)
{
    const auto scalar = static_cast<std::uint32_t>(event.scalar);
    if (session == nullptr || (event.kind == EventKind::Text &&
                               (scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))))
    {
        return std::nullopt;
    }

    NaklAction core_action{};
    const std::uint32_t status =
        nakl_session_process(session, static_cast<std::uint32_t>(event.kind), scalar, &core_action);
    if (status != NAKL_STATUS_OK)
    {
        return std::nullopt;
    }
    if (core_action.kind == NAKL_ACTION_PASS_THROUGH)
    {
        return EditAction{};
    }
    if (core_action.kind != NAKL_ACTION_REPLACE_BEFORE)
    {
        return std::nullopt;
    }

    auto insert = Utf8ToUtf16(core_action.insert_ptr, core_action.insert_len);
    if (!insert.has_value())
    {
        return std::nullopt;
    }
    return EditAction{
        .kind = EditAction::Kind::ReplaceBefore,
        .delete_scalars = core_action.delete_scalars,
        .insert = std::move(*insert),
    };
}

void AppendUtf16Scalar(std::u16string &text, char32_t scalar)
{
    if (scalar <= 0xFFFF)
    {
        text.push_back(static_cast<char16_t>(scalar));
        return;
    }
    scalar -= 0x10000;
    text.push_back(static_cast<char16_t>(0xD800 + (scalar >> 10)));
    text.push_back(static_cast<char16_t>(0xDC00 + (scalar & 0x3FF)));
}

[[nodiscard]] std::u16string TrailingScalars(std::u16string_view text, std::size_t scalar_count)
{
    const std::size_t units = Utf16UnitsForTrailingScalars(text, scalar_count);
    return std::u16string(text.substr(text.size() - units));
}

void TrimHistory(std::u16string &history)
{
    constexpr std::size_t MaxTrackedScalars = 256;
    const std::size_t retained_units = Utf16UnitsForTrailingScalars(history, MaxTrackedScalars);
    if (retained_units < history.size())
    {
        history.erase(0, history.size() - retained_units);
    }
}

} // namespace

struct CompositionSession::Impl
{
    Impl(TypingMethod method, const std::vector<TextExpansion> &expansions)
    {
        NaklConfig *config = nakl_config_new();
        if (config == nullptr)
        {
            return;
        }
        bool valid_config = true;
        for (const auto &expansion : expansions)
        {
            if (expansion.trigger.empty() ||
                nakl_config_add_text_expansion(
                    config, reinterpret_cast<const std::uint8_t *>(expansion.trigger.data()),
                    expansion.trigger.size(),
                    reinterpret_cast<const std::uint8_t *>(expansion.replacement.data()),
                    expansion.replacement.size()) != NAKL_STATUS_OK)
            {
                valid_config = false;
                break;
            }
        }
        if (valid_config)
        {
            handle = nakl_session_new_with_config(static_cast<std::uint32_t>(method), config);
        }
        nakl_config_free(config);
    }

    ~Impl()
    {
        nakl_session_free(handle);
    }

    NaklSession *handle = nullptr;
    std::u16string committed_history;
};

CompositionSession::CompositionSession(TypingMethod method,
                                       const std::vector<TextExpansion> &expansions) noexcept
    : impl_(new (std::nothrow) Impl(method, expansions))
{
}

CompositionSession::~CompositionSession() = default;
CompositionSession::CompositionSession(CompositionSession &&) noexcept = default;
CompositionSession &CompositionSession::operator=(CompositionSession &&) noexcept = default;

bool CompositionSession::valid() const noexcept
{
    return impl_ != nullptr && impl_->handle != nullptr;
}

std::optional<EditAction> CompositionSession::Preview(InputEvent event) const
{
    if (!valid())
    {
        return std::nullopt;
    }
    NaklSession *preview = nakl_session_clone(impl_->handle);
    if (preview == nullptr)
    {
        return std::nullopt;
    }
    auto action = ProcessHandle(preview, event);
    nakl_session_free(preview);
    if (action.has_value() && action->kind == EditAction::Kind::ReplaceBefore)
    {
        action->expected_before =
            TrailingScalars(impl_->committed_history, action->delete_scalars);
    }
    return action;
}

std::optional<EditAction> CompositionSession::Process(InputEvent event)
{
    if (!valid())
    {
        return std::nullopt;
    }
    auto action = ProcessHandle(impl_->handle, event);
    if (!action.has_value())
    {
        return std::nullopt;
    }
    if (action->kind == EditAction::Kind::ReplaceBefore)
    {
        action->expected_before =
            TrailingScalars(impl_->committed_history, action->delete_scalars);
        const std::size_t units =
            Utf16UnitsForTrailingScalars(impl_->committed_history, action->delete_scalars);
        impl_->committed_history.erase(impl_->committed_history.size() - units);
        impl_->committed_history.append(action->insert);
    }
    else if (event.kind == EventKind::Text)
    {
        AppendUtf16Scalar(impl_->committed_history, event.scalar);
    }
    else if (event.kind == EventKind::Backspace && !impl_->committed_history.empty())
    {
        const std::size_t units = Utf16UnitsForTrailingScalars(impl_->committed_history, 1);
        impl_->committed_history.erase(impl_->committed_history.size() - units);
    }
    else if (event.kind == EventKind::Boundary)
    {
        impl_->committed_history.clear();
    }
    TrimHistory(impl_->committed_history);
    return action;
}

void CompositionSession::Reset() noexcept
{
    if (!valid())
    {
        return;
    }
    const auto ignored = Process(InputEvent{.kind = EventKind::Boundary});
    if (!ignored.has_value())
    {
        impl_.reset();
    }
}

std::size_t Utf16UnitsForTrailingScalars(std::u16string_view text, std::size_t scalar_count)
{
    std::size_t index = text.size();
    std::size_t consumed_scalars = 0;
    while (index > 0 && consumed_scalars < scalar_count)
    {
        --index;
        if (IsLowSurrogate(text[index]) && index > 0 && IsHighSurrogate(text[index - 1]))
        {
            --index;
        }
        ++consumed_scalars;
    }
    return text.size() - index;
}

} // namespace nakl::windows
