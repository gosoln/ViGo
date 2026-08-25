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
