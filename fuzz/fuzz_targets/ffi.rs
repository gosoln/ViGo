#![no_main]

use libfuzzer_sys::fuzz_target;
use nakl_core::ffi::{
    NAKL_ACTION_PASS_THROUGH, NAKL_ACTION_REPLACE_BEFORE, NAKL_EVENT_BACKSPACE,
    NAKL_EVENT_BOUNDARY, NAKL_EVENT_TEXT, NAKL_METHOD_TELEX, NAKL_METHOD_VNI,
    NAKL_STATUS_INVALID_INPUT, NAKL_STATUS_OK, NaklAction, nakl_config_add_text_expansion,
    nakl_config_free, nakl_config_new, nakl_session_free, nakl_session_new,
    nakl_session_new_with_config, nakl_session_process,
};

fuzz_target!(|data: &[u8]| {
    let config = nakl_config_new();
    if config.is_null() {
        return;
    }

    let split = data.first().copied().unwrap_or_default() as usize % (data.len() + 1);
    let (trigger, replacement) = data.split_at(split);
    // SAFETY: slice pointers remain readable for the duration of this call.
    let config_status = unsafe {
        nakl_config_add_text_expansion(
            config,
            trigger.as_ptr(),
            trigger.len(),
            replacement.as_ptr(),
            replacement.len(),
        )
    };
    assert!(matches!(
        config_status,
        NAKL_STATUS_OK | NAKL_STATUS_INVALID_INPUT
    ));

    let method = if data.first().copied().unwrap_or_default() & 1 == 0 {
        NAKL_METHOD_TELEX
    } else {
        NAKL_METHOD_VNI
    };
    // SAFETY: config is live and the constructor only borrows it for this call.
    let session = unsafe { nakl_session_new_with_config(method, config) };
    // SAFETY: config came from `nakl_config_new` and is released exactly once.
    unsafe { nakl_config_free(config) };
    if session.is_null() {
        return;
    }

    for chunk in data.chunks(5) {
        let selector = chunk.first().copied().unwrap_or_default();
        let event_kind = match selector % 4 {
            0 => NAKL_EVENT_TEXT,
            1 => NAKL_EVENT_BOUNDARY,
            2 => NAKL_EVENT_BACKSPACE,
            _ => u32::MAX,
        };
        let mut scalar_bytes = [0_u8; 4];
        let scalar_input = chunk.get(1..).unwrap_or_default();
        scalar_bytes[..scalar_input.len()].copy_from_slice(scalar_input);
        let mut action = NaklAction::default();

        // SAFETY: session is live and action is writable for the duration of the call.
        let status = unsafe {
            nakl_session_process(
                session,
                event_kind,
                u32::from_le_bytes(scalar_bytes),
                &mut action,
            )
        };
        assert!(matches!(status, NAKL_STATUS_OK | NAKL_STATUS_INVALID_INPUT));

        if status == NAKL_STATUS_OK {
            assert!(matches!(
                action.kind,
                NAKL_ACTION_PASS_THROUGH | NAKL_ACTION_REPLACE_BEFORE
            ));
            if action.kind == NAKL_ACTION_REPLACE_BEFORE {
                // SAFETY: successful replacement bytes are session-owned until the next call.
                let insert =
                    unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
                assert!(std::str::from_utf8(insert).is_ok());
            }
        }
    }

    // SAFETY: session came from the configured constructor and is released exactly once.
    unsafe { nakl_session_free(session) };

    let maybe_session = nakl_session_new(data.first().copied().unwrap_or_default() as u32);
    if !maybe_session.is_null() {
        // SAFETY: a non-null result is a live session and is released exactly once.
        unsafe { nakl_session_free(maybe_session) };
    }
});
