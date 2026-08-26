#pragma once

#include "composition_session.h"

#include <cstddef>
#include <string_view>

namespace nakl::linux
{

class SurroundingTracker;

class CommittedTextClient
{
  public:
    virtual ~CommittedTextClient() = default;
    [[nodiscard]] virtual bool CanReplaceBefore(std::size_t scalar_count,
                                                std::string_view expected_before) const noexcept = 0;
    virtual void DeleteBefore(std::size_t scalar_count) = 0;
    virtual void Commit(std::string_view text) = 0;
};

// Applies one replacement in the only supported order: delete committed
// scalars first, then commit NFC UTF-8. There is deliberately no preedit API.
[[nodiscard]] bool ApplyCommittedEdit(CommittedTextClient &client, const EditAction &action);

// Validates the native capability, records the expected surrounding-text
// transition, and only then dispatches the replacement. This ordering lets
// synchronous native callbacks be classified without validating twice after
// the tracker has entered its pending state.
[[nodiscard]] bool ApplyTrackedCommittedEdit(CommittedTextClient &client,
                                             SurroundingTracker &tracker,
                                             const EditAction &action);

} // namespace nakl::linux
