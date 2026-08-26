//! Platform-independent Vietnamese composition.

use std::collections::BTreeMap;

pub mod ffi;

/// A user-selectable Vietnamese typing convention.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TypingMethod {
    Telex,
    Vni,
}

/// A platform-normalized input delivered to one composition session.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum InputEvent {
    Text(char),
    Backspace,
    Boundary,
}

/// The complete observable result of processing one input event.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum Action {
    PassThrough,
    ReplaceBefore {
        delete_scalars: usize,
        insert: String,
    },
}

/// Immutable settings captured when a composition session is created.
#[derive(Clone, Debug, Default)]
pub struct SessionConfig {
    text_expansions: BTreeMap<String, String>,
}

impl SessionConfig {
    /// Adds an exact, case-sensitive trigger expanded when space is entered.
    pub fn with_text_expansion(
        mut self,
        trigger: impl Into<String>,
        replacement: impl Into<String>,
    ) -> Self {
        self.text_expansions
            .insert(trigger.into(), replacement.into());
        self
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Base {
    A,
    E,
    I,
    O,
    U,
    Y,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Shape {
    Plain,
    Breve,
    Circumflex,
    Horn,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Tone {
    None,
    Acute,
    Grave,
    Hook,
    Tilde,
    Dot,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
struct Vowel {
    base: Base,
    shape: Shape,
    tone: Tone,
    uppercase: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum Modifier {
    Shape {
        shape: Shape,
        bases: &'static [Base],
        literal: char,
    },
    TelexW,
    Tone {
        tone: Tone,
        literal: char,
    },
    StrokeD,
    Remove,
}

const VOWEL_ROWS: &[(Base, Shape, &str, &str)] = &[
    (Base::A, Shape::Plain, "aáàảãạ", "AÁÀẢÃẠ"),
    (Base::A, Shape::Breve, "ăắằẳẵặ", "ĂẮẰẲẴẶ"),
    (Base::A, Shape::Circumflex, "âấầẩẫậ", "ÂẤẦẨẪẬ"),
    (Base::E, Shape::Plain, "eéèẻẽẹ", "EÉÈẺẼẸ"),
    (Base::E, Shape::Circumflex, "êếềểễệ", "ÊẾỀỂỄỆ"),
    (Base::I, Shape::Plain, "iíìỉĩị", "IÍÌỈĨỊ"),
    (Base::O, Shape::Plain, "oóòỏõọ", "OÓÒỎÕỌ"),
    (Base::O, Shape::Circumflex, "ôốồổỗộ", "ÔỐỒỔỖỘ"),
    (Base::O, Shape::Horn, "ơớờởỡợ", "ƠỚỜỞỠỢ"),
    (Base::U, Shape::Plain, "uúùủũụ", "UÚÙỦŨỤ"),
    (Base::U, Shape::Horn, "ưứừửữự", "ƯỨỪỬỮỰ"),
    (Base::Y, Shape::Plain, "yýỳỷỹỵ", "YÝỲỶỸỴ"),
];

fn tone_from_index(index: usize) -> Tone {
    match index {
        0 => Tone::None,
        1 => Tone::Acute,
        2 => Tone::Grave,
        3 => Tone::Hook,
        4 => Tone::Tilde,
        5 => Tone::Dot,
        _ => unreachable!("vowel rows contain exactly six tones"),
    }
}

fn tone_index(tone: Tone) -> usize {
    match tone {
        Tone::None => 0,
        Tone::Acute => 1,
        Tone::Grave => 2,
        Tone::Hook => 3,
        Tone::Tilde => 4,
        Tone::Dot => 5,
    }
}

fn parse_vowel(character: char) -> Option<Vowel> {
    for &(base, shape, lowercase, uppercase) in VOWEL_ROWS {
        if let Some(index) = lowercase
            .chars()
            .position(|candidate| candidate == character)
        {
            return Some(Vowel {
                base,
                shape,
                tone: tone_from_index(index),
                uppercase: false,
            });
        }
        if let Some(index) = uppercase
            .chars()
            .position(|candidate| candidate == character)
        {
            return Some(Vowel {
                base,
                shape,
                tone: tone_from_index(index),
                uppercase: true,
            });
        }
    }
    None
}

fn render_vowel(vowel: Vowel) -> char {
    let (_, _, lowercase, uppercase) = VOWEL_ROWS
        .iter()
        .find(|(base, shape, _, _)| *base == vowel.base && *shape == vowel.shape)
        .expect("every supported base and shape has a Unicode row");
    let row = if vowel.uppercase {
        uppercase
    } else {
        lowercase
    };
    row.chars()
        .nth(tone_index(vowel.tone))
        .expect("every vowel row contains every tone")
}

fn modifier_for(method: TypingMethod, character: char) -> Option<Modifier> {
    let key = character.to_ascii_lowercase();
    match (method, key) {
        (TypingMethod::Telex, 'a') => Some(Modifier::Shape {
            shape: Shape::Circumflex,
            bases: &[Base::A],
            literal: character,
        }),
        (TypingMethod::Telex, 'e') => Some(Modifier::Shape {
            shape: Shape::Circumflex,
            bases: &[Base::E],
            literal: character,
        }),
        (TypingMethod::Telex, 'o') => Some(Modifier::Shape {
            shape: Shape::Circumflex,
            bases: &[Base::O],
            literal: character,
        }),
        (TypingMethod::Telex, 'w') => Some(Modifier::TelexW),
        (TypingMethod::Telex, 'd') => Some(Modifier::StrokeD),
        (TypingMethod::Telex, 's') => Some(Modifier::Tone {
            tone: Tone::Acute,
            literal: character,
        }),
        (TypingMethod::Telex, 'f') => Some(Modifier::Tone {
            tone: Tone::Grave,
            literal: character,
        }),
        (TypingMethod::Telex, 'r') => Some(Modifier::Tone {
            tone: Tone::Hook,
            literal: character,
        }),
        (TypingMethod::Telex, 'x') => Some(Modifier::Tone {
            tone: Tone::Tilde,
            literal: character,
        }),
        (TypingMethod::Telex, 'j') => Some(Modifier::Tone {
            tone: Tone::Dot,
            literal: character,
        }),
        (TypingMethod::Telex, 'z') => Some(Modifier::Remove),
        (TypingMethod::Vni, '6') => Some(Modifier::Shape {
            shape: Shape::Circumflex,
            bases: &[Base::A, Base::E, Base::O],
            literal: character,
        }),
        (TypingMethod::Vni, '7') => Some(Modifier::Shape {
            shape: Shape::Horn,
            bases: &[Base::O, Base::U],
            literal: character,
        }),
        (TypingMethod::Vni, '8') => Some(Modifier::Shape {
            shape: Shape::Breve,
            bases: &[Base::A],
            literal: character,
        }),
        (TypingMethod::Vni, '9') => Some(Modifier::StrokeD),
        (TypingMethod::Vni, '1') => Some(Modifier::Tone {
            tone: Tone::Acute,
            literal: character,
        }),
        (TypingMethod::Vni, '2') => Some(Modifier::Tone {
            tone: Tone::Grave,
            literal: character,
        }),
        (TypingMethod::Vni, '3') => Some(Modifier::Tone {
            tone: Tone::Hook,
            literal: character,
        }),
        (TypingMethod::Vni, '4') => Some(Modifier::Tone {
            tone: Tone::Tilde,
            literal: character,
        }),
        (TypingMethod::Vni, '5') => Some(Modifier::Tone {
            tone: Tone::Dot,
            literal: character,
        }),
        (TypingMethod::Vni, '0') => Some(Modifier::Remove),
        _ => None,
    }
}

/// Vietnamese composition state belonging to exactly one text context.
#[derive(Debug)]
pub struct CompositionSession {
    method: TypingMethod,
    config: SessionConfig,
    word: String,
}

impl CompositionSession {
    pub fn new(method: TypingMethod) -> Self {
        Self::with_config(method, SessionConfig::default())
    }

    pub fn with_config(method: TypingMethod, config: SessionConfig) -> Self {
        Self {
            method,
            config,
            word: String::new(),
        }
    }

    pub fn process(&mut self, event: InputEvent) -> Action {
        match event {
            InputEvent::Backspace => {
                self.word.pop();
                Action::PassThrough
            }
            InputEvent::Boundary => {
                self.word.clear();
                Action::PassThrough
            }
            InputEvent::Text(' ') => {
                if let Some(replacement) = self.config.text_expansions.get(&self.word) {
                    let action = Action::ReplaceBefore {
                        delete_scalars: self.word.chars().count(),
                        insert: format!("{replacement} "),
                    };
                    self.word.clear();
                    return action;
                }
                self.word.clear();
                Action::PassThrough
            }
            InputEvent::Text(character)
                if !character.is_alphabetic() && modifier_for(self.method, character).is_none() =>
            {
                self.word.clear();
                Action::PassThrough
            }
            InputEvent::Text(character) if modifier_for(self.method, character).is_some() => {
                let modifier = modifier_for(self.method, character).expect("guarded by is_some");
                if let Some(action) = self.apply_modifier(modifier) {
                    return action;
                }
                self.word.push(character);
                Action::PassThrough
            }
            InputEvent::Text(character) => {
                self.word.push(character);
                Action::PassThrough
            }
        }
    }

    fn apply_modifier(&mut self, modifier: Modifier) -> Option<Action> {
        match modifier {
            Modifier::Shape {
                shape,
                bases,
                literal,
            } => {
                if let Some((byte_index, vowel)) =
                    self.word.char_indices().rev().find_map(|(index, ch)| {
                        let vowel = parse_vowel(ch)?;
                        (vowel.shape == shape && bases.contains(&vowel.base))
                            .then_some((index, vowel))
                    })
                {
                    return Some(self.replace_scalar_and_append(
                        byte_index,
                        render_vowel(Vowel {
                            shape: Shape::Plain,
                            ..vowel
                        }),
                        literal,
                    ));
                }

                let (byte_index, mut vowel) =
                    self.word.char_indices().rev().find_map(|(index, ch)| {
                        let vowel = parse_vowel(ch)?;
                        (vowel.shape == Shape::Plain && bases.contains(&vowel.base))
                            .then_some((index, vowel))
                    })?;
                vowel.shape = shape;
                Some(self.replace_scalar(byte_index, render_vowel(vowel)))
            }
            Modifier::TelexW => {
                if self.word.ends_with("uo") {
                    let byte_index = self.word.len() - "uo".len();
                    return Some(self.replace_suffix(byte_index, "ươ"));
                }

                let (byte_index, vowel) =
                    self.word.char_indices().rev().find_map(|(index, ch)| {
                        let vowel = parse_vowel(ch)?;
                        let shape = match (vowel.base, vowel.shape) {
                            (Base::A, Shape::Plain) => Shape::Breve,
                            (Base::O | Base::U, Shape::Plain) => Shape::Horn,
                            _ => return None,
                        };
                        Some((index, Vowel { shape, ..vowel }))
                    })?;
                Some(self.replace_scalar(byte_index, render_vowel(vowel)))
            }
            Modifier::Tone { tone, literal } => {
                let vowels: Vec<_> = self
                    .word
                    .char_indices()
                    .filter_map(|(index, ch)| parse_vowel(ch).map(|vowel| (index, vowel)))
                    .collect();
                let shaped = vowels
                    .iter()
                    .rev()
                    .find(|(_, vowel)| vowel.shape != Shape::Plain)
                    .copied();
                let (byte_index, mut vowel) = shaped.or_else(|| self.tone_target(&vowels))?;
                if vowel.tone == tone {
                    return Some(self.replace_scalar_and_append(
                        byte_index,
                        render_vowel(Vowel {
                            tone: Tone::None,
                            ..vowel
                        }),
                        literal,
                    ));
                }
                vowel.tone = tone;
                Some(self.replace_scalar(byte_index, render_vowel(vowel)))
            }
            Modifier::StrokeD => {
                let (byte_index, replacement) =
                    self.word
                        .char_indices()
                        .rev()
                        .find_map(|(index, ch)| match ch {
                            'd' => Some((index, 'đ')),
                            'D' => Some((index, 'Đ')),
                            _ => None,
                        })?;
                Some(self.replace_scalar(byte_index, replacement))
            }
            Modifier::Remove => {
                let (byte_index, vowel) =
                    self.word.char_indices().rev().find_map(|(index, ch)| {
                        let vowel = parse_vowel(ch)?;
                        (vowel.shape != Shape::Plain || vowel.tone != Tone::None)
                            .then_some((index, vowel))
                    })?;
                Some(self.replace_scalar(
                    byte_index,
                    render_vowel(Vowel {
                        shape: Shape::Plain,
                        tone: Tone::None,
                        ..vowel
                    }),
                ))
            }
        }
    }

    fn tone_target(&self, vowels: &[(usize, Vowel)]) -> Option<(usize, Vowel)> {
        let mut nucleus = vowels;
        if vowels.len() > 1 {
            let (first_index, first) = vowels[0];
            let preceding = self.word[..first_index].chars().next_back();
            let leading_glide = matches!((preceding, first.base), (Some('q' | 'Q'), Base::U))
                || matches!((preceding, first.base), (Some('g' | 'G'), Base::I));
            if leading_glide {
                nucleus = &vowels[1..];
            }
        }

        match nucleus {
            [] => None,
            [only] => Some(*only),
            [first, second] => {
                let second_length = self.word[second.0..]
                    .chars()
                    .next()
                    .expect("vowel index points at a scalar")
                    .len_utf8();
                let has_final_consonant = second.0 + second_length < self.word.len();
                let open_final_vowel_cluster = matches!(
                    (first.1.base, second.1.base),
                    (Base::O, Base::A | Base::E) | (Base::U, Base::Y)
                );
                if has_final_consonant || open_final_vowel_cluster {
                    Some(*second)
                } else {
                    Some(*first)
                }
            }
            _ => Some(nucleus[nucleus.len() - 2]),
        }
    }

    fn replace_scalar(&mut self, byte_index: usize, replacement: char) -> Action {
        let previous = self.word[byte_index..]
            .chars()
            .next()
            .expect("replacement index points at a scalar");
        let delete_scalars = self.word[byte_index..].chars().count();
        self.word.replace_range(
            byte_index..byte_index + previous.len_utf8(),
            &replacement.to_string(),
        );
        Action::ReplaceBefore {
            delete_scalars,
            insert: self.word[byte_index..].to_owned(),
        }
    }

    fn replace_scalar_and_append(
        &mut self,
        byte_index: usize,
        replacement: char,
        literal: char,
    ) -> Action {
        let previous = self.word[byte_index..]
            .chars()
            .next()
            .expect("replacement index points at a scalar");
        let delete_scalars = self.word[byte_index..].chars().count();
        let replacement = format!("{replacement}{literal}");
        self.word
            .replace_range(byte_index..byte_index + previous.len_utf8(), &replacement);
        Action::ReplaceBefore {
            delete_scalars,
            insert: self.word[byte_index..].to_owned(),
        }
    }

    fn replace_suffix(&mut self, byte_index: usize, replacement: &str) -> Action {
        let delete_scalars = self.word[byte_index..].chars().count();
        self.word.replace_range(byte_index.., replacement);
        Action::ReplaceBefore {
            delete_scalars,
            insert: replacement.to_owned(),
        }
    }
}
