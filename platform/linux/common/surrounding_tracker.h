#pragma once

#include "composition_session.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace nakl::linux
{

// Distinguishes native text updates caused by the key NAKL just handled from
// cursor moves and edits made outside the composition session.
class SurroundingTracker final
{
  public:
    // Returns true when a valid observation differs from the update NAKL was
    // expecting. Callers must reset their Composition Session in that case.
    [[nodiscard]] bool Observe(std::string_view text, std::size_t cursor,
                               std::size_t anchor);
    void Clear() noexcept;

    [[nodiscard]] bool pending() const noexcept;
    void CancelPending() noexcept;
    [[nodiscard]] bool CanReplaceBefore(std::size_t scalar_count,
                                        std::string_view expected_before) const noexcept;
    [[nodiscard]] bool ExpectPassThrough(InputEvent event);
    [[nodiscard]] bool ExpectReplacement(const EditAction &action);

  private:
    struct Snapshot
    {
        std::string text;
        std::size_t cursor = 0;
        std::size_t anchor = 0;
        bool valid = false;

        [[nodiscard]] bool operator==(const Snapshot &other) const noexcept = default;
    };

    Snapshot current_;
    std::optional<Snapshot> intermediate_;
    std::optional<Snapshot> final_;
};

} // namespace nakl::linux
