#[test]
fn c_abi_reports_version_one() {
    assert_eq!(vigo_core::ffi::vigo_abi_version(), 1);
}

#[test]
fn c_abi_owns_an_opaque_telex_session() {
    let session = vigo_core::ffi::vigo_session_new(vigo_core::ffi::VIGO_METHOD_TELEX);
    assert!(!session.is_null());

    // SAFETY: the pointer came from `vigo_session_new` and is freed exactly once.
    unsafe { vigo_core::ffi::vigo_session_free(session) };
}

#[test]
fn c_abi_processes_text_through_the_same_session_seam() {
    use vigo_core::ffi::{
        VIGO_ACTION_PASS_THROUGH, VIGO_ACTION_REPLACE_BEFORE, VIGO_EVENT_TEXT, VIGO_METHOD_TELEX,
        VIGO_STATUS_OK, VigoAction, vigo_session_free, vigo_session_new, vigo_session_process,
    };

    let session = vigo_session_new(VIGO_METHOD_TELEX);
    let mut action = VigoAction::default();

    // SAFETY: pointers are live and `action` is writable for the duration of each call.
    assert_eq!(
        unsafe { vigo_session_process(session, VIGO_EVENT_TEXT, 'a' as u32, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_PASS_THROUGH);

    // SAFETY: same live session and output storage as above.
    assert_eq!(
        unsafe { vigo_session_process(session, VIGO_EVENT_TEXT, 'a' as u32, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, 1);
    // SAFETY: replacement bytes remain owned by the session until the next process call.
    let insert = unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
    assert_eq!(
        std::str::from_utf8(insert).expect("core returns UTF-8"),
        "â"
    );

    // SAFETY: the pointer came from `vigo_session_new` and is freed exactly once.
    unsafe { vigo_session_free(session) };
}

#[test]
fn c_abi_clones_text_expansion_config_into_a_session() {
    use vigo_core::ffi::{
        VIGO_ACTION_PASS_THROUGH, VIGO_ACTION_REPLACE_BEFORE, VIGO_EVENT_TEXT, VIGO_METHOD_TELEX,
        VIGO_STATUS_OK, VigoAction, vigo_config_add_text_expansion, vigo_config_free,
        vigo_config_new, vigo_session_free, vigo_session_new_with_config, vigo_session_process,
    };

    let trigger = "btw";
    let replacement = "nhân tiện";
    let config = vigo_config_new();
    assert!(!config.is_null());

    // SAFETY: both UTF-8 buffers and the config remain live for this call.
    assert_eq!(
        unsafe {
            vigo_config_add_text_expansion(
                config,
                trigger.as_ptr(),
                trigger.len(),
                replacement.as_ptr(),
                replacement.len(),
            )
        },
        VIGO_STATUS_OK
    );

    // SAFETY: the config pointer is live for the duration of the constructor call.
    let session = unsafe { vigo_session_new_with_config(VIGO_METHOD_TELEX, config) };
    assert!(!session.is_null());
    // SAFETY: the session owns a snapshot, so the live config can now be released.
    unsafe { vigo_config_free(config) };

    let mut action = VigoAction::default();
    for character in trigger.chars() {
        // SAFETY: the session and writable action storage remain live.
        assert_eq!(
            unsafe {
                vigo_session_process(session, VIGO_EVENT_TEXT, character as u32, &mut action)
            },
            VIGO_STATUS_OK
        );
        assert_eq!(action.kind, VIGO_ACTION_PASS_THROUGH);
    }

    // SAFETY: the session and writable action storage remain live.
    assert_eq!(
        unsafe { vigo_session_process(session, VIGO_EVENT_TEXT, ' ' as u32, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, trigger.chars().count());
    // SAFETY: replacement bytes remain session-owned until its next process call.
    let insert = unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
    assert_eq!(
        std::str::from_utf8(insert).expect("core returns UTF-8"),
        "nhân tiện "
    );

    // SAFETY: the pointer came from `vigo_session_new_with_config` and is freed once.
    unsafe { vigo_session_free(session) };
}

#[test]
fn c_abi_backspace_updates_session_state_and_passes_through() {
    use vigo_core::ffi::{
        VIGO_ACTION_PASS_THROUGH, VIGO_EVENT_BACKSPACE, VIGO_EVENT_TEXT, VIGO_METHOD_TELEX,
        VIGO_STATUS_OK, VigoAction, vigo_session_free, vigo_session_new, vigo_session_process,
    };

    let session = vigo_session_new(VIGO_METHOD_TELEX);
    let mut action = VigoAction::default();
    for character in ['a', 'a'] {
        // SAFETY: the session and writable action storage remain live.
        assert_eq!(
            unsafe {
                vigo_session_process(session, VIGO_EVENT_TEXT, character as u32, &mut action)
            },
            VIGO_STATUS_OK
        );
    }

    // SAFETY: the session and writable action storage remain live.
    assert_eq!(
        unsafe { vigo_session_process(session, VIGO_EVENT_BACKSPACE, 0, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_PASS_THROUGH);

    // SAFETY: same live pointers; a fresh `a` must start a new word after deleting `â`.
    assert_eq!(
        unsafe { vigo_session_process(session, VIGO_EVENT_TEXT, 'a' as u32, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_PASS_THROUGH);

    // SAFETY: the pointer came from `vigo_session_new` and is freed exactly once.
    unsafe { vigo_session_free(session) };
}

#[test]
fn c_abi_clone_predicts_without_mutating_the_source_session() {
    use vigo_core::ffi::{
        VIGO_ACTION_REPLACE_BEFORE, VIGO_EVENT_TEXT, VIGO_METHOD_TELEX, VIGO_STATUS_OK, VigoAction,
        vigo_session_clone, vigo_session_free, vigo_session_new, vigo_session_process,
    };

    let session = vigo_session_new(VIGO_METHOD_TELEX);
    let mut action = VigoAction::default();
    for character in "giai".chars() {
        // SAFETY: the session and writable action storage remain live.
        assert_eq!(
            unsafe {
                vigo_session_process(session, VIGO_EVENT_TEXT, character as u32, &mut action)
            },
            VIGO_STATUS_OK
        );
    }

    // SAFETY: the source session is live for the clone operation.
    let preview = unsafe { vigo_session_clone(session) };
    assert!(!preview.is_null());
    // SAFETY: the cloned session and writable action storage remain live.
    assert_eq!(
        unsafe { vigo_session_process(preview, VIGO_EVENT_TEXT, 'r' as u32, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, 2);
    // SAFETY: the replacement remains owned by the clone until its next call.
    let preview_insert =
        unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
    assert_eq!(std::str::from_utf8(preview_insert).unwrap(), "ải");
    // SAFETY: the clone is released exactly once.
    unsafe { vigo_session_free(preview) };

    // Processing the source must produce the same action, proving the preview
    // did not advance source composition state.
    // SAFETY: the source session and output storage remain live.
    assert_eq!(
        unsafe { vigo_session_process(session, VIGO_EVENT_TEXT, 'r' as u32, &mut action) },
        VIGO_STATUS_OK
    );
    assert_eq!(action.kind, VIGO_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, 2);
    // SAFETY: the source session is released exactly once.
    unsafe { vigo_session_free(session) };
}
