#![no_main]

use libfuzzer_sys::fuzz_target;
use vigo_core::ffi::{
    VIGO_ACTION_PASS_THROUGH, VIGO_ACTION_REPLACE_BEFORE, VIGO_EVENT_BACKSPACE,
    VIGO_EVENT_BOUNDARY, VIGO_EVENT_TEXT, VIGO_METHOD_TELEX, VIGO_METHOD_VNI,
    VIGO_STATUS_INVALID_INPUT, VIGO_STATUS_OK, VigoAction, vigo_config_add_text_expansion,
    vigo_config_free, vigo_config_new, vigo_session_clone, vigo_session_free, vigo_session_new,
    vigo_session_new_with_config, vigo_session_process,
};

fuzz_target!(|data: &[u8]| {
    let config = vigo_config_new();
    if config.is_null() {
        return;
    }

    let split = data.first().copied().unwrap_or_default() as usize % (data.len() + 1);
    let (trigger, replacement) = data.split_at(split);
    // SAFETY: slice pointers remain readable for the duration of this call.
    let config_status = unsafe {
        vigo_config_add_text_expansion(
            config,
            trigger.as_ptr(),
            trigger.len(),
            replacement.as_ptr(),
            replacement.len(),
        )
    };
    assert!(matches!(
        config_status,
        VIGO_STATUS_OK | VIGO_STATUS_INVALID_INPUT
    ));

    let method = if data.first().copied().unwrap_or_default() & 1 == 0 {
        VIGO_METHOD_TELEX
    } else {
        VIGO_METHOD_VNI
    };
    // SAFETY: config is live and the constructor only borrows it for this call.
    let session = unsafe { vigo_session_new_with_config(method, config) };
    // SAFETY: config came from `vigo_config_new` and is released exactly once.
    unsafe { vigo_config_free(config) };
    if session.is_null() {
        return;
    }

    // Exercise preview ownership independently from the live session. A clone
    // must remain safe to process and release at any point in the event stream.
    // SAFETY: session is live and clone only borrows it for this call.
    let preview = unsafe { vigo_session_clone(session) };
    if !preview.is_null() {
        let mut action = VigoAction::default();
        // SAFETY: preview and action are live for the duration of this call.
        let status = unsafe { vigo_session_process(preview, VIGO_EVENT_BOUNDARY, 0, &mut action) };
        assert_eq!(status, VIGO_STATUS_OK);
        // SAFETY: preview came from clone and is released exactly once.
        unsafe { vigo_session_free(preview) };
    }

    for chunk in data.chunks(5) {
        let selector = chunk.first().copied().unwrap_or_default();
        let event_kind = match selector % 4 {
            0 => VIGO_EVENT_TEXT,
            1 => VIGO_EVENT_BOUNDARY,
            2 => VIGO_EVENT_BACKSPACE,
            _ => u32::MAX,
        };
        let mut scalar_bytes = [0_u8; 4];
        let scalar_input = chunk.get(1..).unwrap_or_default();
        scalar_bytes[..scalar_input.len()].copy_from_slice(scalar_input);
        let mut action = VigoAction::default();

        // SAFETY: session is live and action is writable for the duration of the call.
        let status = unsafe {
            vigo_session_process(
                session,
                event_kind,
                u32::from_le_bytes(scalar_bytes),
                &mut action,
            )
        };
        assert!(matches!(status, VIGO_STATUS_OK | VIGO_STATUS_INVALID_INPUT));

        if status == VIGO_STATUS_OK {
            assert!(matches!(
                action.kind,
                VIGO_ACTION_PASS_THROUGH | VIGO_ACTION_REPLACE_BEFORE
            ));
            if action.kind == VIGO_ACTION_REPLACE_BEFORE {
                // SAFETY: successful replacement bytes are session-owned until the next call.
                let insert =
                    unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
                assert!(std::str::from_utf8(insert).is_ok());
            }
        }
    }

    // SAFETY: session came from the configured constructor and is released exactly once.
    unsafe { vigo_session_free(session) };

    let maybe_session = vigo_session_new(data.first().copied().unwrap_or_default() as u32);
    if !maybe_session.is_null() {
        // SAFETY: a non-null result is a live session and is released exactly once.
        unsafe { vigo_session_free(maybe_session) };
    }
});
