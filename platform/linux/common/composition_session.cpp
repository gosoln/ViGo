#include "composition_session.h"

#include "nakl_core.h"

#include <new>
#include <string_view>
#include <utility>

namespace nakl::linux
{
namespace
{

[[nodiscard]] bool IsValidUtf8(const std::uint8_t *bytes, std::size_t length)
{
    if (length == 0)
    {
        return true;
    }
    if (bytes == nullptr)
    {
        return false;
    }

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
            return false;
        }

        if (continuation_count > length - index)
        {
            return false;
        }
        for (std::size_t offset = 0; offset < continuation_count; ++offset)
        {
            const std::uint8_t continuation = bytes[index++];
            if ((continuation & 0xC0) != 0x80)
            {
                return false;
            }
            scalar = (scalar << 6) | (continuation & 0x3F);
        }

        const bool overlong = (continuation_count == 1 && scalar < 0x80) ||
                              (continuation_count == 2 && scalar < 0x800) ||
                              (continuation_count == 3 && scalar < 0x10000);
        if (overlong || scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
        {
            return false;
        }
    }
    return true;
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
    if (core_action.kind != NAKL_ACTION_REPLACE_BEFORE ||
        !IsValidUtf8(core_action.insert_ptr, core_action.insert_len))
    {
        return std::nullopt;
    }

    const auto *begin = reinterpret_cast<const char *>(core_action.insert_ptr);
    return EditAction{
        .kind = EditAction::Kind::ReplaceBefore,
        .delete_scalars = core_action.delete_scalars,
        .expected_before = {},
        .insert = std::string(begin, core_action.insert_len),
    };
}

[[nodiscard]] std::size_t TrailingUtf8Bytes(std::string_view text, std::size_t scalar_count)
{
    std::size_t index = text.size();
    while (index > 0 && scalar_count > 0)
    {
        --index;
        while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80)
        {
            --index;
        }
        --scalar_count;
    }
    return text.size() - index;
}

void AppendUtf8Scalar(std::string &text, char32_t scalar)
{
    if (scalar <= 0x7F)
    {
        text.push_back(static_cast<char>(scalar));
    }
    else if (scalar <= 0x7FF)
    {
        text.push_back(static_cast<char>(0xC0 | (scalar >> 6)));
        text.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
    else if (scalar <= 0xFFFF)
    {
        text.push_back(static_cast<char>(0xE0 | (scalar >> 12)));
        text.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
    else
    {
        text.push_back(static_cast<char>(0xF0 | (scalar >> 18)));
        text.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
        text.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
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
    std::string committed_history;
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
    std::unique_ptr<NaklSession, decltype(&nakl_session_free)> preview_owner(preview,
                                                                           nakl_session_free);
    auto action = ProcessHandle(preview_owner.get(), event);
    if (action.has_value() && action->kind == EditAction::Kind::ReplaceBefore)
    {
        const std::size_t bytes =
            TrailingUtf8Bytes(impl_->committed_history, action->delete_scalars);
        action->expected_before = impl_->committed_history.substr(
            impl_->committed_history.size() - bytes);
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
        const std::size_t bytes =
            TrailingUtf8Bytes(impl_->committed_history, action->delete_scalars);
        action->expected_before = impl_->committed_history.substr(
            impl_->committed_history.size() - bytes);
        impl_->committed_history.erase(impl_->committed_history.size() - bytes);
        impl_->committed_history.append(action->insert);
    }
    else if (event.kind == EventKind::Text)
    {
        AppendUtf8Scalar(impl_->committed_history, event.scalar);
    }
    else if (event.kind == EventKind::Backspace && !impl_->committed_history.empty())
    {
        const std::size_t bytes = TrailingUtf8Bytes(impl_->committed_history, 1);
        impl_->committed_history.erase(impl_->committed_history.size() - bytes);
    }
    else if (event.kind == EventKind::Boundary)
    {
        impl_->committed_history.clear();
    }
    constexpr std::size_t MaxHistoryBytes = 4096;
    if (impl_->committed_history.size() > MaxHistoryBytes)
    {
        const std::size_t bytes = TrailingUtf8Bytes(impl_->committed_history, 256);
        impl_->committed_history.erase(0, impl_->committed_history.size() - bytes);
    }
    return action;
}

void CompositionSession::Reset() noexcept
{
    if (!valid())
    {
        return;
    }
    if (!Process(InputEvent{.kind = EventKind::Boundary}).has_value())
    {
        impl_.reset();
    }
}

} // namespace nakl::linux
