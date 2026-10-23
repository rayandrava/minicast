# Third-party notices

Minicast's own source, documentation, and geometric logo assets are covered by the [MIT license](LICENSE). Third-party components retain their own licenses. Minicast is an independent project, inspired by launcher interfaces including Raycast and Tinycast; it is not affiliated with or endorsed by those products. Their names and application icons belong to their respective owners.

## Vendored icons

The SVGs in `assets/lucide/` come from [Lucide 0.468.0](https://github.com/lucide-icons/lucide/tree/0.468.0/icons). `src/lucide_data.inc` is generated from those SVG paths by `scripts/generate-icons.ps1`.

- Keep [Lucide's original ISC notice](assets/lucide/LICENSE), including its attribution to Feather and Cole Bemis.
- [Feather's MIT notice](assets/lucide/FEATHER-LICENSE) is also included for the Feather-derived paths. Its source is [Feather v4.29.2](https://github.com/feathericons/feather/blob/v4.29.2/LICENSE).

The `assets/brand/` circle and crescent studies were created for Minicast. The selected double-crescent SVG is the source for the generated app and tray icons; no Raycast or Tinycast logo asset is included.

## Windows build runtime

The supported build uses the pinned [Zig 0.14.1 distribution](https://ziglang.org/download/0.14.1/). The compiler itself lives in ignored `.tools/` and is not included in Minicast source or release archives. Compiled executables can contain runtime and C++ library portions supplied by the toolchain. The following notices are retained with releases:

| Component | Notice |
| --- | --- |
| Zig and its compiler runtime | [Zig MIT license](licenses/ZIG.txt) |
| LLVM libc++ | [Apache 2.0 with LLVM exceptions and retained legacy notices](licenses/LLVM-LIBCXX.txt) |
| LLVM libc++abi | [Apache 2.0 with LLVM exceptions and retained legacy notices](licenses/LLVM-LIBCXXABI.txt) |
| LLVM libunwind | [Apache 2.0 with LLVM exceptions and retained legacy notices](licenses/LLVM-LIBUNWIND.txt) |
| MinGW-w64 CRT | [Zope Public License 2.1 and exceptions noted upstream](licenses/MINGW-W64.txt) |
| Additional MinGW-w64 runtime portions | [winpthreads, formatting, gdtoa and Sun math notices](licenses/MINGW-ADDITIONAL.txt) |
| musl-derived compiler runtime math | [musl copyright and license notices](licenses/MUSL.txt) |

The full toolchain notices above were copied from the pinned Zig distribution, with additional source-header notices identified in `licenses/MINGW-ADDITIONAL.txt`. This is not a claim that every function in those libraries is linked. Review the notices again when changing the compiler, runtime, target, or vendored dependencies. The MinGW-w64 source tree's umbrella notice explicitly permits separately marked licenses; do not replace it with a blanket MIT label.

Microsoft Windows APIs, DLLs, fonts and locally installed application icons are used from the user's system. Those files are not bundled with Minicast or relicensed by this repository.

## Emoji

`src/core.cpp` contains a small curated table of Unicode emoji characters, short labels and search keywords. The repository does not vendor an upstream Unicode/CLDR data file, emoji image collection, or font. Glyphs are rendered by installed Windows fonts. Unicode distinguishes using characters from redistributing its datasets or glyph artwork; see its [font and character FAQ](https://www.unicode.org/faq/font_keyboard.html). If a future change imports Unicode or CLDR data, preserve its applicable license and record its version and source here.

## Exchange-rate service

Minicast requests daily reference rates from [Frankfurter](https://frankfurter.dev/) and caches them locally. No Frankfurter server code is bundled. Frankfurter's software license does **not** relicense all underlying rate data: rates come from central banks and other official providers, whose terms can apply. See [Frankfurter's license and provider terms](https://frankfurter.dev/license/). Cached rates are user data and are excluded from source and release packages.

## Redistribution

Distribute `LICENSE`, this file, `assets/lucide/LICENSE`, `assets/lucide/FEATHER-LICENSE`, and the `licenses/` directory alongside binaries. Retain their relative paths so these links remain usable. Do not include `.tools/`, personal profiles, cached clipboard contents, local app indexes or downloaded exchange rates.
