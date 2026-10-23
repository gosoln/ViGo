#![no_main]

use libfuzzer_sys::fuzz_target;
use vigo_core::{Action, CompositionSession, InputEvent, TypingMethod};

fuzz_target!(|data: &[u8]| {
    let Some((&selector, input)) = data.split_first() else {
        return;
    };
    let method = if selector & 1 == 0 {
        TypingMethod::Telex
    } else {
        TypingMethod::Vni
    };
    let mut session = CompositionSession::new(method);
    let mut document = String::new();

    for bytes in input.chunks(4) {
        let mut scalar_bytes = [0_u8; 4];
        scalar_bytes[..bytes.len()].copy_from_slice(bytes);
        let value = u32::from_le_bytes(scalar_bytes);
        let event = if value.is_multiple_of(29) {
            InputEvent::Boundary
        } else {
            let scalar = value % 0x11_0000;
            let Some(character) = char::from_u32(scalar) else {
                continue;
            };
            InputEvent::Text(character)
        };

        match session.process(event) {
            Action::PassThrough => {
                if let InputEvent::Text(character) = event {
                    document.push(character);
                }
            }
            Action::ReplaceBefore {
                delete_scalars,
                insert,
            } => {
                assert!(delete_scalars <= document.chars().count());
                for _ in 0..delete_scalars {
                    document.pop();
                }
                document.push_str(&insert);
            }
        }
    }
});
