#include "nakl_core.h"

int main(void)
{
    NaklSession *session = nakl_session_new(NAKL_METHOD_TELEX);
    NaklAction action = {0};
    uint32_t status = nakl_session_process(
        session,
        NAKL_EVENT_TEXT,
        (uint32_t)'a',
        &action
    );
    nakl_session_free(session);
    return status == NAKL_STATUS_OK ? 0 : 1;
}
