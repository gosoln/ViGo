#include "vigo_core.h"

#include <string.h>

int main(void)
{
    const uint8_t trigger[] = "btw";
    const uint8_t replacement[] = "by the way";
    VigoConfig *config = vigo_config_new();
    if (config == NULL) {
        return 1;
    }
    if (vigo_config_add_text_expansion(
            config,
            trigger,
            sizeof(trigger) - 1,
            replacement,
            sizeof(replacement) - 1
        ) != VIGO_STATUS_OK) {
        vigo_config_free(config);
        return 1;
    }

    VigoSession *session = vigo_session_new_with_config(VIGO_METHOD_TELEX, config);
    vigo_config_free(config);
    if (session == NULL) {
        return 1;
    }
    VigoSession *preview = vigo_session_clone(session);
    if (preview == NULL) {
        vigo_session_free(session);
        return 1;
    }
    vigo_session_free(preview);

    VigoAction action = {0};
    const char input[] = "btw ";
    uint32_t status = VIGO_STATUS_OK;
    for (size_t index = 0; index < sizeof(input) - 1; ++index) {
        status = vigo_session_process(
            session,
            VIGO_EVENT_TEXT,
            (uint32_t)input[index],
            &action
        );
        if (status != VIGO_STATUS_OK) {
            vigo_session_free(session);
            return 1;
        }
    }

    const uint8_t expected[] = "by the way ";
    int matches = action.kind == VIGO_ACTION_REPLACE_BEFORE
        && action.delete_scalars == 3
        && action.insert_len == sizeof(expected) - 1
        && memcmp(action.insert_ptr, expected, sizeof(expected) - 1) == 0;
    status = vigo_session_process(
        session,
        VIGO_EVENT_BACKSPACE,
        0,
        &action
    );
    matches = matches
        && status == VIGO_STATUS_OK
        && action.kind == VIGO_ACTION_PASS_THROUGH;
    vigo_session_free(session);
    return matches ? 0 : 1;
}
