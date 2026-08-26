use nakl_core::{Action, CompositionSession, InputEvent, SessionConfig, TypingMethod};

fn compose(method: TypingMethod, input: &str) -> String {
    let mut session = CompositionSession::new(method);
    apply_input(&mut session, input)
}

fn apply_input(session: &mut CompositionSession, input: &str) -> String {
    let mut document = String::new();

    for character in input.chars() {
        match session.process(InputEvent::Text(character)) {
            Action::PassThrough => document.push(character),
            Action::ReplaceBefore {
                delete_scalars,
                insert,
            } => {
                for _ in 0..delete_scalars {
                    document
                        .pop()
                        .expect("action must not delete past the document");
                }
                document.push_str(&insert);
            }
        }
    }

    document
}

#[test]
fn configured_text_expansion_replaces_trigger_on_space() {
    let config = SessionConfig::default().with_text_expansion("btw", "by the way");
    let mut session = CompositionSession::with_config(TypingMethod::Telex, config);

    assert_eq!(apply_input(&mut session, "btw "), "by the way ");
}

#[test]
fn backspace_keeps_the_remaining_word_composable() {
    let mut session = CompositionSession::new(TypingMethod::Telex);
    let mut document = apply_input(&mut session, "tieengs");
    assert_eq!(document, "tiếng");

    assert_eq!(session.process(InputEvent::Backspace), Action::PassThrough);
    document.pop();
    assert_eq!(document, "tiến");

    match session.process(InputEvent::Text('f')) {
        Action::ReplaceBefore {
            delete_scalars,
            insert,
        } => {
            for _ in 0..delete_scalars {
                document.pop();
            }
            document.push_str(&insert);
        }
        Action::PassThrough => document.push('f'),
    }
    assert_eq!(document, "tiền");
}

#[test]
fn telex_double_a_forms_circumflex() {
    assert_eq!(compose(TypingMethod::Telex, "aa"), "â");
}

#[test]
fn telex_a_w_forms_breve() {
    assert_eq!(compose(TypingMethod::Telex, "aw"), "ă");
}

#[test]
fn vni_a_six_forms_circumflex() {
    assert_eq!(compose(TypingMethod::Vni, "a6"), "â");
}

#[test]
fn boundary_starts_a_new_composition_session() {
    let mut session = CompositionSession::new(TypingMethod::Telex);

    assert_eq!(session.process(InputEvent::Text('a')), Action::PassThrough);
    assert_eq!(session.process(InputEvent::Boundary), Action::PassThrough);
    assert_eq!(session.process(InputEvent::Text('a')), Action::PassThrough);
}

#[test]
fn punctuation_starts_a_new_composition_session() {
    let mut session = CompositionSession::new(TypingMethod::Telex);

    assert_eq!(session.process(InputEvent::Text('a')), Action::PassThrough);
    assert_eq!(session.process(InputEvent::Text('.')), Action::PassThrough);
    assert_eq!(session.process(InputEvent::Text('s')), Action::PassThrough);
}

#[test]
fn telex_double_d_forms_stroked_d() {
    assert_eq!(compose(TypingMethod::Telex, "dd"), "đ");
}

#[test]
fn telex_s_places_an_acute_tone() {
    assert_eq!(compose(TypingMethod::Telex, "as"), "á");
}

#[test]
fn telex_places_tone_on_a_shaped_vowel_inside_a_word() {
    assert_eq!(compose(TypingMethod::Telex, "tieengs"), "tiếng");
}

#[test]
fn telex_o_w_forms_horn() {
    assert_eq!(compose(TypingMethod::Telex, "ow"), "ơ");
}

#[test]
fn vni_a_eight_forms_breve() {
    assert_eq!(compose(TypingMethod::Vni, "a8"), "ă");
}

#[test]
fn vni_one_places_an_acute_tone() {
    assert_eq!(compose(TypingMethod::Vni, "a1"), "á");
}

#[test]
fn repeating_a_shape_modifier_restores_literal_input() {
    assert_eq!(compose(TypingMethod::Telex, "aaa"), "aa");
}

#[test]
fn repeating_a_tone_modifier_restores_literal_input() {
    assert_eq!(compose(TypingMethod::Telex, "ass"), "as");
}

#[test]
fn telex_places_tone_on_the_leading_vowel_in_ua() {
    assert_eq!(compose(TypingMethod::Telex, "tuar"), "tủa");
}

#[test]
fn telex_places_tone_on_the_nucleus_in_giai() {
    assert_eq!(compose(TypingMethod::Telex, "giair"), "giải");
}

#[test]
fn product_golden_corpus_matches_reviewed_behavior() {
    let corpus = include_str!("../../tests/golden/product-expected.tsv");

    for line in corpus.lines().filter(|line| !line.starts_with('#')) {
        let mut fields = line.split('\t');
        let method = match fields.next().expect("method field") {
            "telex" => TypingMethod::Telex,
            "vni" => TypingMethod::Vni,
            unknown => panic!("unknown typing method: {unknown}"),
        };
        let input = fields.next().expect("input field");
        let expected = fields.next().expect("expected output field");

        assert_eq!(
            compose(method, input),
            expected,
            "golden case failed for {input}"
        );
    }
}
