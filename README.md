ViGo - Vietnamese input method for macOS, Windows, and Linux
=======

ViGo (formerly NAKL 2.0) is derived from [NAKL](https://github.com/huyphan/NAKL) by Huy Phan. It uses a deterministic Rust composition core through a versioned C ABI. Native adapters now
live under `platform/macos/`, `platform/windows/`, and `platform/linux/` (IBus plus Fcitx5); the
original macOS app remains frozen under `legacy/macos-hook/` as a behavioral oracle.

```sh
cargo test --workspace
cargo clippy --workspace --all-targets -- -D warnings
cargo +nightly fuzz run composition --fuzz-dir fuzz -- -max_total_time=10
make -C legacy/oracle
swift build --package-path platform/macos
cargo build -p vigo-core --release
cmake -S platform/linux -B build/linux -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release \
  -DVIGO_BUILD_IBUS=ON -DVIGO_BUILD_FCITX5=ON
```

See [the core interface design](docs/design/core-interface.md) and [CONTRIBUTING.md](CONTRIBUTING.md) before contributing to ViGo.

* The original project homepage is [huyphan.github.com/NAKL](http://huyphan.github.com/NAKL).

Contributing
=======

* Please report bugs and request features through this repository's GitHub issue templates. Personal emails are not appropriate for bug reports. Do not paste private text you typed.
* Patches are preferred as GitHub pull requests. See [CONTRIBUTING.md](CONTRIBUTING.md).

Credits
=======
* chuoi (a.k.a Dao Hai Lam): author of [xvnkb](http://xvnkb.sourceforge.net).
* Thanks to mybb (a.k.a hieuln) and pmquan (a.k.a co`i) for helping me testing out this software.


License
=======
* Distributed under the GNU GPLv3. See the file [LICENSE](LICENSE).
* Key handling algorithm and keymap of this project are based on [xvnkb](http://xvnkb.sourceforge.net/) written by Dao Hai Lam.
* HotKey setting is based on [ShortcutRecorder](http://wafflesoftware.net/shortcut/) library.
