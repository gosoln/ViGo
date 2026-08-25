//! Stable C interface for native platform adapters.

use std::panic::{AssertUnwindSafe, catch_unwind};
use std::ptr;

use crate::{Action, CompositionSession, InputEvent, TypingMethod};

pub const ABI_VERSION: u32 = 1;
pub const NAKL_METHOD_TELEX: u32 = 1;
pub const NAKL_METHOD_VNI: u32 = 2;
pub const NAKL_EVENT_TEXT: u32 = 1;
pub const NAKL_EVENT_BOUNDARY: u32 = 2;
pub const NAKL_ACTION_PASS_THROUGH: u32 = 0;
pub const NAKL_ACTION_REPLACE_BEFORE: u32 = 1;
pub const NAKL_STATUS_OK: u32 = 0;
pub const NAKL_STATUS_NULL_POINTER: u32 = 1;
pub const NAKL_STATUS_INVALID_INPUT: u32 = 2;
pub const NAKL_STATUS_PANIC: u32 = 3;

/// Borrowed action data returned by [`nakl_session_process`].
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct NaklAction {
    pub kind: u32,
    pub delete_scalars: usize,
    pub insert_ptr: *const u8,
    pub insert_len: usize,
}

impl Default for NaklAction {
    fn default() -> Self {
        Self {
            kind: NAKL_ACTION_PASS_THROUGH,
            delete_scalars: 0,
            insert_ptr: ptr::null(),
            insert_len: 0,
        }
    }
}

/// Opaque owner of one core composition session.
pub struct NaklSession {
    session: CompositionSession,
    last_insert: Vec<u8>,
}

#[unsafe(no_mangle)]
pub extern "C" fn nakl_abi_version() -> u32 {
    ABI_VERSION
}

#[unsafe(no_mangle)]
pub extern "C" fn nakl_session_new(method: u32) -> *mut NaklSession {
    catch_unwind(AssertUnwindSafe(|| {
        let method = match method {
            NAKL_METHOD_TELEX => TypingMethod::Telex,
            NAKL_METHOD_VNI => TypingMethod::Vni,
            _ => return ptr::null_mut(),
        };
        Box::into_raw(Box::new(NaklSession {
            session: CompositionSession::new(method),
            last_insert: Vec::new(),
        }))
    }))
    .unwrap_or(ptr::null_mut())
}

/// Releases a session returned by [`nakl_session_new`].
///
/// # Safety
///
/// `session` must be null or a live pointer returned by [`nakl_session_new`]
/// that has not already been released.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nakl_session_free(session: *mut NaklSession) {
    if !session.is_null() {
        // SAFETY: upheld by the caller contract above.
        drop(unsafe { Box::from_raw(session) });
    }
}

/// Processes one normalized event and writes its action to `output`.
///
/// Replacement bytes are UTF-8 owned by `session` and remain valid until the
/// next process call or until the session is released.
///
/// # Safety
///
/// `session` must point to a live session returned by [`nakl_session_new`], and
/// `output` must point to writable storage for one [`NaklAction`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn nakl_session_process(
    session: *mut NaklSession,
    event_kind: u32,
    scalar: u32,
    output: *mut NaklAction,
) -> u32 {
    if session.is_null() || output.is_null() {
        return NAKL_STATUS_NULL_POINTER;
    }

    catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: non-null pointers and their validity are guaranteed by the caller contract.
        let session = unsafe { &mut *session };
        // SAFETY: same caller contract as above.
        let output = unsafe { &mut *output };
        let event = match event_kind {
            NAKL_EVENT_TEXT => match char::from_u32(scalar) {
                Some(character) => InputEvent::Text(character),
                None => return NAKL_STATUS_INVALID_INPUT,
            },
            NAKL_EVENT_BOUNDARY => InputEvent::Boundary,
            _ => return NAKL_STATUS_INVALID_INPUT,
        };

        session.last_insert.clear();
        *output = match session.session.process(event) {
            Action::PassThrough => NaklAction::default(),
            Action::ReplaceBefore {
                delete_scalars,
                insert,
            } => {
                session.last_insert.extend_from_slice(insert.as_bytes());
                NaklAction {
                    kind: NAKL_ACTION_REPLACE_BEFORE,
                    delete_scalars,
                    insert_ptr: session.last_insert.as_ptr(),
                    insert_len: session.last_insert.len(),
                }
            }
        };
        NAKL_STATUS_OK
    }))
    .unwrap_or(NAKL_STATUS_PANIC)
}
