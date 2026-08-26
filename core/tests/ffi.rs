#[test]
fn c_abi_reports_version_one() {
    assert_eq!(nakl_core::ffi::nakl_abi_version(), 1);
}

#[test]
fn c_abi_owns_an_opaque_telex_session() {
    let session = nakl_core::ffi::nakl_session_new(nakl_core::ffi::NAKL_METHOD_TELEX);
    assert!(!session.is_null());

    // SAFETY: the pointer came from `nakl_session_new` and is freed exactly once.
    unsafe { nakl_core::ffi::nakl_session_free(session) };
}

#[test]
fn c_abi_processes_text_through_the_same_session_seam() {
    use nakl_core::ffi::{
        NAKL_ACTION_PASS_THROUGH, NAKL_ACTION_REPLACE_BEFORE, NAKL_EVENT_TEXT, NAKL_METHOD_TELEX,
        NAKL_STATUS_OK, NaklAction, nakl_session_free, nakl_session_new, nakl_session_process,
    };

    let session = nakl_session_new(NAKL_METHOD_TELEX);
    let mut action = NaklAction::default();

    // SAFETY: pointers are live and `action` is writable for the duration of each call.
    assert_eq!(
        unsafe { nakl_session_process(session, NAKL_EVENT_TEXT, 'a' as u32, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_PASS_THROUGH);

    // SAFETY: same live session and output storage as above.
    assert_eq!(
        unsafe { nakl_session_process(session, NAKL_EVENT_TEXT, 'a' as u32, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, 1);
    // SAFETY: replacement bytes remain owned by the session until the next process call.
    let insert = unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
    assert_eq!(
        std::str::from_utf8(insert).expect("core returns UTF-8"),
        "â"
    );

    // SAFETY: the pointer came from `nakl_session_new` and is freed exactly once.
    unsafe { nakl_session_free(session) };
}

#[test]
fn c_abi_clones_text_expansion_config_into_a_session() {
    use nakl_core::ffi::{
        NAKL_ACTION_PASS_THROUGH, NAKL_ACTION_REPLACE_BEFORE, NAKL_EVENT_TEXT, NAKL_METHOD_TELEX,
        NAKL_STATUS_OK, NaklAction, nakl_config_add_text_expansion, nakl_config_free,
        nakl_config_new, nakl_session_free, nakl_session_new_with_config, nakl_session_process,
    };

    let trigger = "btw";
    let replacement = "nhân tiện";
    let config = nakl_config_new();
    assert!(!config.is_null());

    // SAFETY: both UTF-8 buffers and the config remain live for this call.
    assert_eq!(
        unsafe {
            nakl_config_add_text_expansion(
                config,
                trigger.as_ptr(),
                trigger.len(),
                replacement.as_ptr(),
                replacement.len(),
            )
        },
        NAKL_STATUS_OK
    );

    // SAFETY: the config pointer is live for the duration of the constructor call.
    let session = unsafe { nakl_session_new_with_config(NAKL_METHOD_TELEX, config) };
    assert!(!session.is_null());
    // SAFETY: the session owns a snapshot, so the live config can now be released.
    unsafe { nakl_config_free(config) };

    let mut action = NaklAction::default();
    for character in trigger.chars() {
        // SAFETY: the session and writable action storage remain live.
        assert_eq!(
            unsafe {
                nakl_session_process(session, NAKL_EVENT_TEXT, character as u32, &mut action)
            },
            NAKL_STATUS_OK
        );
        assert_eq!(action.kind, NAKL_ACTION_PASS_THROUGH);
    }

    // SAFETY: the session and writable action storage remain live.
    assert_eq!(
        unsafe { nakl_session_process(session, NAKL_EVENT_TEXT, ' ' as u32, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, trigger.chars().count());
    // SAFETY: replacement bytes remain session-owned until its next process call.
    let insert = unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
    assert_eq!(
        std::str::from_utf8(insert).expect("core returns UTF-8"),
        "nhân tiện "
    );

    // SAFETY: the pointer came from `nakl_session_new_with_config` and is freed once.
    unsafe { nakl_session_free(session) };
}

#[test]
fn c_abi_backspace_updates_session_state_and_passes_through() {
    use nakl_core::ffi::{
        NAKL_ACTION_PASS_THROUGH, NAKL_EVENT_BACKSPACE, NAKL_EVENT_TEXT, NAKL_METHOD_TELEX,
        NAKL_STATUS_OK, NaklAction, nakl_session_free, nakl_session_new, nakl_session_process,
    };

    let session = nakl_session_new(NAKL_METHOD_TELEX);
    let mut action = NaklAction::default();
    for character in ['a', 'a'] {
        // SAFETY: the session and writable action storage remain live.
        assert_eq!(
            unsafe {
                nakl_session_process(session, NAKL_EVENT_TEXT, character as u32, &mut action)
            },
            NAKL_STATUS_OK
        );
    }

    // SAFETY: the session and writable action storage remain live.
    assert_eq!(
        unsafe { nakl_session_process(session, NAKL_EVENT_BACKSPACE, 0, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_PASS_THROUGH);

    // SAFETY: same live pointers; a fresh `a` must start a new word after deleting `â`.
    assert_eq!(
        unsafe { nakl_session_process(session, NAKL_EVENT_TEXT, 'a' as u32, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_PASS_THROUGH);

    // SAFETY: the pointer came from `nakl_session_new` and is freed exactly once.
    unsafe { nakl_session_free(session) };
}

#[test]
fn c_abi_clone_predicts_without_mutating_the_source_session() {
    use nakl_core::ffi::{
        NAKL_ACTION_REPLACE_BEFORE, NAKL_EVENT_TEXT, NAKL_METHOD_TELEX, NAKL_STATUS_OK, NaklAction,
        nakl_session_clone, nakl_session_free, nakl_session_new, nakl_session_process,
    };

    let session = nakl_session_new(NAKL_METHOD_TELEX);
    let mut action = NaklAction::default();
    for character in "giai".chars() {
        // SAFETY: the session and writable action storage remain live.
        assert_eq!(
            unsafe {
                nakl_session_process(session, NAKL_EVENT_TEXT, character as u32, &mut action)
            },
            NAKL_STATUS_OK
        );
    }

    // SAFETY: the source session is live for the clone operation.
    let preview = unsafe { nakl_session_clone(session) };
    assert!(!preview.is_null());
    // SAFETY: the cloned session and writable action storage remain live.
    assert_eq!(
        unsafe { nakl_session_process(preview, NAKL_EVENT_TEXT, 'r' as u32, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, 2);
    // SAFETY: the replacement remains owned by the clone until its next call.
    let preview_insert =
        unsafe { std::slice::from_raw_parts(action.insert_ptr, action.insert_len) };
    assert_eq!(std::str::from_utf8(preview_insert).unwrap(), "ải");
    // SAFETY: the clone is released exactly once.
    unsafe { nakl_session_free(preview) };

    // Processing the source must produce the same action, proving the preview
    // did not advance source composition state.
    // SAFETY: the source session and output storage remain live.
    assert_eq!(
        unsafe { nakl_session_process(session, NAKL_EVENT_TEXT, 'r' as u32, &mut action) },
        NAKL_STATUS_OK
    );
    assert_eq!(action.kind, NAKL_ACTION_REPLACE_BEFORE);
    assert_eq!(action.delete_scalars, 2);
    // SAFETY: the source session is released exactly once.
    unsafe { nakl_session_free(session) };
}
