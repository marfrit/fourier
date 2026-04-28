# qt6-base-fourier

Three small runtime checks in qtbase 6.11.0 so Qt 6 picks `GL_R8`
instead of `GL_ALPHA` when the live OpenGL context advertises
ES 3.x or newer.

## Background

When qtbase is built with `QT_CONFIG(opengles2)` (every aarch64
distribution does this), three sites hard-code `GL_ALPHA` as the
`glTexImage2D` internalFormat with no runtime check for the actual
context's ES version:

- `src/opengl/qopengltextureglyphcache.cpp:111-117` — text glyph
  cache upload path. Primary KDE-decoration trigger.
- `src/gui/rhi/qrhigles2.cpp:1373-1378` — Qt-Quick-RHI's
  `RED_OR_ALPHA8` path.
- `src/opengl/qopengltextureuploader.cpp:253-275` — `Format_Alpha8`
  and `Format_Grayscale8` short-circuit on `isOpenGLES()` before
  reaching the existing `TextureSwizzle` fallback.

OpenGL ES 3.0 (spec section 3.8.3) deprecated `GL_ALPHA` as a
`glTexImage2D` internalFormat — only sized formats (`GL_R8`,
`GL_R16F`, …) are valid. On Mali-class hardware running mesa
panfrost / panthor with KWin Wayland (which requests OpenGL ES 3.2),
this hard-coded path emits `GL_INVALID_VALUE` on every glyph cache
upload. KWin's debug callback fills the journal and adds enough
load to mask other latent bugs in the pipeline.

## Patches

1. `0001-qopengltextureglyphcache-pick-GL_R8-on-ES3.patch`
2. `0002-qrhigles2-RED_OR_ALPHA8-pick-GL_R8-on-ES3.patch`
3. `0003-qopengltextureuploader-pick-GL_R8-on-ES3.patch`

Each adds a runtime predicate (`useR8 = caps.gles && caps.ctxMajor >= 3`
or equivalent via `ctx->format().majorVersion()`) so ES 3+ contexts
get `GL_R8` + `GL_RED` (and the matching swizzle), while true ES 2
contexts retain the legacy `GL_ALPHA`.

The PKGBUILD also carries `qt6-base-cflags.patch` and
`qt6-base-nostrip.patch` from upstream Arch packaging; those are
unchanged.

## Building / installing

```sh
makepkg -si
```

The PKGBUILD inherits from upstream Arch's qt6-base 6.11.0-2,
disables the `FEATURE_sql_ibase` plugin (Arch ARM's libfbclient is
older than Qt 6.11 expects, unrelated to fourier), and bumps
`epoch=1` so the package strictly dominates upstream `6.11.0-N`
until the patches land upstream and the epoch can be dropped.

## Upstream submission

These patches are pure spec-correctness improvements with no
behavior change for ES 2 or desktop GL Core/Compat contexts; they
should be cleanly upstreamable. Target: bugreports.qt.io against
**QtBase: OpenGL**, with a Gerrit changeset against `qtbase` `dev`
branch.
