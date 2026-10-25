#pragma once

#include "com_ptr.h"

#include <msctf.h>

#include <string_view>

namespace vigo::windows
{

// Outcome of a composition edit. `Committed` means the client refused to
// start a composition and the text was inserted directly instead; the caller
// must then forget its Composition Session because later ordered replacements
// have no range to act on.
enum class CompositionEditResult
{
    Composing,
    Committed,
    Ended,
};

// Replaces the text of the live composition inside a synchronous read/write
// edit session, starting a composition over the current selection when none
// exists. `expected_text` guards against client edits the service did not see:
// when the composition range no longer holds it, nothing changes and S_FALSE is
// returned. `end_after` (or empty `text`) ends the composition after the edit so
// its text becomes ordinary committed client text.
[[nodiscard]] HRESULT SetCompositionText(ITfContext *context, TfClientId client_id,
                                         ITfCompositionSink *sink,
                                         ComPtr<ITfComposition> &composition,
                                         std::u16string_view expected_text,
                                         std::u16string_view text, bool end_after,
                                         CompositionEditResult *outcome);

// Ends the composition and releases it. Tries a synchronous session first and
// falls back to an asynchronous one that owns its own reference, so the caller
// may drop the pointer immediately.
[[nodiscard]] HRESULT EndComposition(ITfContext *context, TfClientId client_id,
                                     ComPtr<ITfComposition> &composition);

} // namespace vigo::windows
