# Third-party notices

## MyBot SDK

The sources under `include/` and `src/` are based on a snapshot of
https://github.com/junlon2006/mybot at commit
`58f0ef9c14be9f159b1dc167bf0f035983b25117` (tag `v1.3.0`), with the BK7259 HTTPS
debug patch recorded in `SDK_REVISION`. Its source digests identify the upstream snapshot
and vendored contents. They are provided under the Apache
License 2.0 in `LICENSE`,
except where a source file states otherwise.

`src/support/mybot_json.c` and `src/internal/mybot_json.h` are namespaced
derivatives of cJSON. Their MIT license and Dave Gamble copyright notice are
retained in those files.

## LVGL status view and assets

`platforms/bk7259/display/` adapts the platform-independent status view and
generated resources from `junlon2006/mybot-esp32` commit
`19af3bdfaf1dd35461af48def8f9558e3503e755`. File provenance, local adaptations,
and resource limits are recorded in `platforms/bk7259/display/SOURCES.md`.
The following asset paths are relative to `platforms/bk7259/`.

- The view is MIT-licensed. Its license terms are recorded in
  `display/assets/licenses/LVGL_VIEW_LICENSE.txt`; the asset API header retains
  Apache-2.0.
- `display/lvgl_fonts.c` contains the MyBot UI Sans 20-pixel subset derived
  from Source Han Sans SC Normal, copyright 2014-2021 Adobe. It is licensed
  under SIL OFL-1.1 in `display/assets/licenses/LVGL_VIEW_FONT_LICENSE.txt`.
  The generated data is unchanged from the recorded ESP32 revision and uses
  the distinct MyBot UI Sans name.
- `display/lvgl_assets.c` contains four rasterized Noto Color Emoji glyphs,
  copyright 2021 Google Inc., licensed under SIL OFL-1.1 in
  `display/assets/ui/noto_emoji/LICENSE.txt`. Upstream revision and source
  hashes are preserved in `display/assets/ui/noto_emoji/SOURCES.json`.

The LVGL 9.5.0 implementation and built-in fonts are consumed from the
repository's pinned `bk_avdk_smp/ap/components/lvgl` component. Its license is
recorded in that component's `LICENCE.txt`; source-specific notices remain
applicable. The BK7259 view does not copy the ESP-IDF display driver.

## Voice prompts

The embedded Chinese and English Ogg/Opus prompt assets under
`projects/mybot/assets/` are part of this repository. No standalone
third-party attribution is recorded for these files; confirm their ownership
and redistribution terms before release.

## AOSL

AOSL is vendored by the sibling `mybot_aosl` component from commit
`84e086084ebcd0ae2455a0ce5721950c5fe2e656`. Its license adds conditions to
Apache-2.0; read `../mybot_aosl/aosl/LICENSE` before use or redistribution.

## Agora RTSA SDK

The BK7259 RTSA binary is supplied externally and is not part of this source
tree. Its package contains no standalone redistribution grant. Do not publish
the package, static library, or firmware containing it without written Agora
authorization.
