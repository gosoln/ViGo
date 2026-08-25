NAKL - Yet another Vietnamese input keyboard for Mac OS
=======

NAKL 2.0 modernization is underway. Milestone M0 adds a deterministic Rust composition core, reviewed Telex/VNI golden fixtures, a versioned C ABI, and a command-line oracle for the legacy Objective-C behavior. The original macOS app is frozen under `legacy/macos-hook/`; it is not the production base for the cross-platform rewrite.

```sh
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
cargo +nightly fuzz run composition --fuzz-dir fuzz -- -max_total_time=10
make -C legacy/oracle
```

See [the modernization plan](docs/modernization-plan.md), [platform evaluation](docs/research/cross-platform-input-method-platform-evaluation.md), and [architecture decisions](docs/adr/) before contributing to NAKL 2.0.

* Please visit [homepage](http://huyphan.github.com/NAKL) of this project.

Contributing
=======

* Please report issues on the [GitHub issue tracker](https://github.com/huyphan/NAKL/issues). Personal emails are not appropriate for bug reports.
* Patches are preferred as GitHub pull requests. Contributors are welcome to request free beer from author of this project.

Credits
=======
* chuoi (a.k.a Dao Hai Lam): author of [xvnkb](http://xvnkb.sourceforge.net).
* Thanks to mybb (a.k.a hieuln) and pmquan (a.k.a co`i) for helping me testing out this software.


License
=======
* Distributed under the GNU GPLv3. See the file [LICENSE](https://github.com/huyphan/NAKL/blob/master/LICENSE).
* Key handling algorithm and keymap of this project are based on [xvnkb](http://xvnkb.sourceforge.net/) written by Dao Hai Lam.
* HotKey setting is based on [ShortcutRecorder](http://wafflesoftware.net/shortcut/) library.
