#ifndef VIGO_CORE_H
#define VIGO_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIGO_ABI_VERSION 1u

#define VIGO_METHOD_TELEX 1u
#define VIGO_METHOD_VNI 2u

#define VIGO_EVENT_TEXT 1u
#define VIGO_EVENT_BOUNDARY 2u
#define VIGO_EVENT_BACKSPACE 3u

#define VIGO_ACTION_PASS_THROUGH 0u
#define VIGO_ACTION_REPLACE_BEFORE 1u

#define VIGO_STATUS_OK 0u
#define VIGO_STATUS_NULL_POINTER 1u
#define VIGO_STATUS_INVALID_INPUT 2u
#define VIGO_STATUS_PANIC 3u

typedef struct VigoSession VigoSession;
typedef struct VigoConfig VigoConfig;

typedef struct VigoAction {
    uint32_t kind;
    size_t delete_scalars;
    const uint8_t *insert_ptr;
    size_t insert_len;
} VigoAction;

uint32_t vigo_abi_version(void);

/* Creates an empty config builder. Returns NULL when allocation fails. */
VigoConfig *vigo_config_new(void);

/* Copies one non-empty UTF-8 trigger and its UTF-8 replacement. */
uint32_t vigo_config_add_text_expansion(
    VigoConfig *config,
    const uint8_t *trigger_ptr,
    size_t trigger_len,
    const uint8_t *replacement_ptr,
    size_t replacement_len
);

/* Accepts NULL. Every non-NULL config must be released exactly once. */
void vigo_config_free(VigoConfig *config);

/* Returns NULL when method is unsupported or session creation fails. */
VigoSession *vigo_session_new(uint32_t method);

/* Copies config; the builder may be released immediately after this call. */
VigoSession *vigo_session_new_with_config(
    uint32_t method,
    const VigoConfig *config
);

/*
 * Clones composition state for action prediction. The returned session is
 * independent and must be released exactly once. Returns NULL on failure.
 */
VigoSession *vigo_session_clone(const VigoSession *session);

/* Accepts NULL. Every non-NULL session must be released exactly once. */
void vigo_session_free(VigoSession *session);

/*
 * insert_ptr is borrowed UTF-8. It remains valid until the next process call
 * on this session or until the session is released.
 */
uint32_t vigo_session_process(
    VigoSession *session,
    uint32_t event_kind,
    uint32_t scalar,
    VigoAction *output
);

#ifdef __cplusplus
}
#endif

#endif
