#pragma once

#include "composition_session.h"

#include <msctf.h>

namespace vigo::windows
{

// Applies exactly one core replacement inside the TSF context's synchronous
// keystroke edit session. No caller needs to know the ITfEditSession class.
[[nodiscard]] HRESULT ApplyEditAction(ITfContext *context, TfClientId client_id,
                                      const EditAction &action);

} // namespace vigo::windows
