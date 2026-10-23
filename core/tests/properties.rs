use vigo_core::{Action, CompositionSession, InputEvent, TypingMethod};

const INPUT_ALPHABET: &[char] = &[
    'a', 'A', 'e', 'i', 'o', 'u', 'y', 'd', 'w', 's', 'f', 'r', 'x', 'j', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', 'ă', 'ấ', 'ơ', 'ự', 'Đ', ' ', '-', '🙂',
];

fn next_random(state: &mut u64) -> u64 {
    *state = state
        .wrapping_mul(6_364_136_223_846_793_005)
        .wrapping_add(1);
    *state
}

#[test]
fn arbitrary_event_streams_never_delete_before_the_document_start() {
    for method in [TypingMethod::Telex, TypingMethod::Vni] {
        let mut session = CompositionSession::new(method);
        let mut document = String::new();
        let mut random = 0x4e_41_4b_4c_u64;

        for _ in 0..20_000 {
            let value = next_random(&mut random);
            let event = if value.is_multiple_of(23) {
                InputEvent::Boundary
            } else {
                InputEvent::Text(INPUT_ALPHABET[value as usize % INPUT_ALPHABET.len()])
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
                    assert!(
                        delete_scalars <= document.chars().count(),
                        "{method:?} attempted to delete {delete_scalars} scalars from {document:?}"
                    );
                    for _ in 0..delete_scalars {
                        document.pop();
                    }
                    document.push_str(&insert);
                }
            }

            assert!(std::str::from_utf8(document.as_bytes()).is_ok());
        }
    }
}
