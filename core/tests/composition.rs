use vigo_core::{Action, CompositionSession, InputEvent, SessionConfig, TypingMethod};

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
fn repeating_a_tone_modifier_disables_later_tones_for_the_word() {
    assert_eq!(compose(TypingMethod::Telex, "verrs"), "vers");
    assert_eq!(compose(TypingMethod::Telex, "verrsion"), "version");
    assert_eq!(compose(TypingMethod::Telex, "verrf"), "verf");
    assert_eq!(compose(TypingMethod::Vni, "ve331"), "ve31");
    assert_eq!(compose(TypingMethod::Telex, "aaas"), "aas");
    assert_eq!(compose(TypingMethod::Telex, "verr as"), "ver á");
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

#[test]
fn telex_english_word_with_invalid_coda_stays_literal() {
    assert_eq!(compose(TypingMethod::Telex, "deve"), "deve");
    assert_eq!(compose(TypingMethod::Telex, "developer"), "developer");
}

#[test]
fn telex_tone_key_after_a_second_vowel_cluster_stays_literal() {
    assert_eq!(compose(TypingMethod::Telex, "loper"), "loper");
    assert_eq!(compose(TypingMethod::Telex, "develope"), "develope");
}

#[test]
fn telex_delayed_shape_after_a_valid_coda_still_applies() {
    assert_eq!(compose(TypingMethod::Telex, "tama"), "tâm");
    assert_eq!(compose(TypingMethod::Telex, "dongo"), "dông");
    assert_eq!(compose(TypingMethod::Telex, "viete"), "viêt");
}

#[test]
fn vni_shape_digit_after_a_second_vowel_cluster_stays_literal() {
    assert_eq!(compose(TypingMethod::Vni, "deve6"), "deve6");
    assert_eq!(compose(TypingMethod::Vni, "an6"), "ân");
}

#[test]
fn telex_multi_cluster_english_words_stay_literal() {
    for input in [
        "return", "internet", "function", "computer", "window", "facebook", "youtube",
    ] {
        assert_eq!(compose(TypingMethod::Telex, input), input);
    }
}

#[test]
fn telex_vowel_key_after_an_invalid_coda_stays_literal() {
    assert_eq!(compose(TypingMethod::Telex, "ebe"), "ebe");
    assert_eq!(compose(TypingMethod::Telex, "aba"), "aba");
}

#[test]
fn telex_delayed_shape_applies_over_each_valid_coda() {
    assert_eq!(compose(TypingMethod::Telex, "cana"), "cân");
    assert_eq!(compose(TypingMethod::Telex, "hiepe"), "hiêp");
    assert_eq!(compose(TypingMethod::Telex, "vieec"), "viêc");
    assert_eq!(compose(TypingMethod::Telex, "tieech"), "tiêch");
    assert_eq!(compose(TypingMethod::Telex, "vieenh"), "viênh");
}

#[test]
fn telex_delayed_tone_stroke_and_w_apply_after_a_coda() {
    assert_eq!(compose(TypingMethod::Telex, "tamf"), "tàm");
    assert_eq!(compose(TypingMethod::Telex, "vietj"), "viẹt");
    assert_eq!(compose(TypingMethod::Telex, "dad"), "đa");
    assert_eq!(compose(TypingMethod::Telex, "did"), "đi");
    assert_eq!(compose(TypingMethod::Telex, "tamw"), "tăm");
    assert_eq!(compose(TypingMethod::Telex, "canw"), "căn");
}

#[test]
fn vni_delayed_modifiers_apply_over_a_coda() {
    assert_eq!(compose(TypingMethod::Vni, "tam1"), "tám");
    assert_eq!(compose(TypingMethod::Vni, "ta6m"), "tâm");
    assert_eq!(compose(TypingMethod::Vni, "vie6t1"), "viết");
}

#[test]
fn vni_digits_stay_literal_in_english_words() {
    assert_eq!(compose(TypingMethod::Vni, "hello1"), "hello1");
    assert_eq!(compose(TypingMethod::Vni, "window123"), "window123");
}

#[test]
fn modifiers_apply_again_in_the_next_word_after_a_boundary() {
    assert_eq!(
        compose(TypingMethod::Telex, "developer tieengs"),
        "developer tiếng"
    );
}

#[test]
fn uppercase_english_words_stay_literal() {
    assert_eq!(compose(TypingMethod::Telex, "Developer"), "Developer");
    assert_eq!(compose(TypingMethod::Telex, "DEVELOPER"), "DEVELOPER");
}

#[test]
fn backspace_into_a_multi_cluster_word_keeps_modifier_keys_literal() {
    let mut session = CompositionSession::new(TypingMethod::Telex);
    let mut document = apply_input(&mut session, "develope");
    assert_eq!(document, "develope");

    assert_eq!(session.process(InputEvent::Backspace), Action::PassThrough);
    document.pop();
    assert_eq!(document, "develop");

    assert_eq!(session.process(InputEvent::Text('r')), Action::PassThrough);
    document.push('r');
    assert_eq!(document, "developr");
}
