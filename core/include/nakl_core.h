#ifndef NAKL_CORE_H
#define NAKL_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAKL_ABI_VERSION 1u

#define NAKL_METHOD_TELEX 1u
#define NAKL_METHOD_VNI 2u

#define NAKL_EVENT_TEXT 1u
#define NAKL_EVENT_BOUNDARY 2u

#define NAKL_ACTION_PASS_THROUGH 0u
#define NAKL_ACTION_REPLACE_BEFORE 1u

#define NAKL_STATUS_OK 0u
#define NAKL_STATUS_NULL_POINTER 1u
#define NAKL_STATUS_INVALID_INPUT 2u
#define NAKL_STATUS_PANIC 3u

typedef struct NaklSession NaklSession;
typedef struct NaklConfig NaklConfig;

typedef struct NaklAction {
    uint32_t kind;
    size_t delete_scalars;
    const uint8_t *insert_ptr;
    size_t insert_len;
} NaklAction;

uint32_t nakl_abi_version(void);

/* Creates an empty config builder. Returns NULL when allocation fails. */
NaklConfig *nakl_config_new(void);

/* Copies one non-empty UTF-8 trigger and its UTF-8 replacement. */
uint32_t nakl_config_add_text_expansion(
    NaklConfig *config,
    const uint8_t *trigger_ptr,
    size_t trigger_len,
    const uint8_t *replacement_ptr,
    size_t replacement_len
);

/* Accepts NULL. Every non-NULL config must be released exactly once. */
void nakl_config_free(NaklConfig *config);

/* Returns NULL when method is unsupported or session creation fails. */
NaklSession *nakl_session_new(uint32_t method);

/* Copies config; the builder may be released immediately after this call. */
NaklSession *nakl_session_new_with_config(
    uint32_t method,
    const NaklConfig *config
);

/* Accepts NULL. Every non-NULL session must be released exactly once. */
void nakl_session_free(NaklSession *session);

/*
 * insert_ptr is borrowed UTF-8. It remains valid until the next process call
 * on this session or until the session is released.
 */
uint32_t nakl_session_process(
    NaklSession *session,
    uint32_t event_kind,
    uint32_t scalar,
    NaklAction *output
);

#ifdef __cplusplus
}
#endif

#endif
