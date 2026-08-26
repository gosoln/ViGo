#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nakl::linux
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
    std::string expected_before;
    std::string insert;
};

struct TextExpansion
{
    std::string trigger;
    std::string replacement;
};

// Owns exactly one core Composition Session for one Linux input context.
// Framework adapters translate native key events before crossing this seam and
// apply the returned committed edit exactly once.
class CompositionSession final
{
  public:
    explicit CompositionSession(TypingMethod method,
                                const std::vector<TextExpansion> &expansions = {}) noexcept;
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

} // namespace nakl::linux
