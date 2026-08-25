#include "nakl_core.h"

#include <string.h>

int main(void)
{
    const uint8_t trigger[] = "btw";
    const uint8_t replacement[] = "by the way";
    NaklConfig *config = nakl_config_new();
    if (config == NULL) {
        return 1;
    }
    if (nakl_config_add_text_expansion(
            config,
            trigger,
            sizeof(trigger) - 1,
            replacement,
            sizeof(replacement) - 1
        ) != NAKL_STATUS_OK) {
        nakl_config_free(config);
        return 1;
    }

    NaklSession *session = nakl_session_new_with_config(NAKL_METHOD_TELEX, config);
    nakl_config_free(config);
    if (session == NULL) {
        return 1;
    }

    NaklAction action = {0};
    const char input[] = "btw ";
    uint32_t status = NAKL_STATUS_OK;
    for (size_t index = 0; index < sizeof(input) - 1; ++index) {
        status = nakl_session_process(
            session,
            NAKL_EVENT_TEXT,
            (uint32_t)input[index],
            &action
        );
        if (status != NAKL_STATUS_OK) {
            nakl_session_free(session);
            return 1;
        }
    }

    const uint8_t expected[] = "by the way ";
    int matches = action.kind == NAKL_ACTION_REPLACE_BEFORE
        && action.delete_scalars == 3
        && action.insert_len == sizeof(expected) - 1
        && memcmp(action.insert_ptr, expected, sizeof(expected) - 1) == 0;
    status = nakl_session_process(
        session,
        NAKL_EVENT_BACKSPACE,
        0,
        &action
    );
    matches = matches
        && status == NAKL_STATUS_OK
        && action.kind == NAKL_ACTION_PASS_THROUGH;
    nakl_session_free(session);
    return matches ? 0 : 1;
}
