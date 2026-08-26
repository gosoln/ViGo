use std::time::Instant;

use nakl_core::{CompositionSession, InputEvent, TypingMethod};

#[test]
#[ignore = "release gate; run with --release on a quiet native runner"]
fn processing_p99_stays_below_one_millisecond() {
    const ITERATIONS: usize = 100_000;
    const INPUT: &[char] = &['t', 'i', 'e', 'e', 'n', 'g', 's', ' '];

    let mut session = CompositionSession::new(TypingMethod::Telex);
    let mut samples = Vec::with_capacity(ITERATIONS);
    for index in 0..ITERATIONS {
        let start = Instant::now();
        let _ = session.process(InputEvent::Text(INPUT[index % INPUT.len()]));
        samples.push(start.elapsed().as_nanos());
    }

    samples.sort_unstable();
    let p99 = samples[(ITERATIONS * 99) / 100];
    eprintln!("composition processing p99: {p99} ns");
    assert!(
        p99 < 1_000_000,
        "p99 {p99} ns exceeded the 1 ms release gate"
    );
}
