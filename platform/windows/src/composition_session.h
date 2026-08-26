#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace nakl::windows
{

enum class TypingMethod : std::uint32_t
{
    Telex = 1,
    Vni = 2,
};

enum class EventKind : std::uint32_t
{
    Text = 1,
    Boundary = 2,
    Backspace = 3,
};

struct InputEvent
{
    EventKind kind;
    char32_t scalar = U'\0';
};

struct EditAction
{
    enum class Kind
    {
        PassThrough,
        ReplaceBefore,
    };

    Kind kind = Kind::PassThrough;
    std::size_t delete_scalars = 0;
    std::u16string insert;
};

class CompositionSession final
{
  public:
    explicit CompositionSession(TypingMethod method) noexcept;
    ~CompositionSession();

    CompositionSession(const CompositionSession &) = delete;
    CompositionSession &operator=(const CompositionSession &) = delete;
    CompositionSession(CompositionSession &&) noexcept;
    CompositionSession &operator=(CompositionSession &&) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::optional<EditAction> Preview(InputEvent event) const;
    [[nodiscard]] std::optional<EditAction> Process(InputEvent event);
    void Reset() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Converts a scalar deletion count into UTF-16 code units without splitting a
// well-formed surrogate pair. TSF ranges are measured in UTF-16 units.
[[nodiscard]] std::size_t Utf16UnitsForTrailingScalars(std::u16string_view text,
                                                       std::size_t scalar_count);

} // namespace nakl::windows
