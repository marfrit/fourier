# chromium-fourier

Four chromium patches that unlock V4L2 hardware video decode on
mainline Linux Wayland on Rockchip SoCs (RK3399 / RK3566 / RK3588)
with mesa panfrost / panthor — no vendor MPP, no Mali blob, no
panfork, no 5.10 BSP kernel.

See the top-level [`README.md`](../README.md) for the campaign-level
context. This subdirectory is the chromium-specific portion.

## Patches (apply in order)

1. `enable-v4l2-decoder-default.patch` — flips
   `media::kAcceleratedVideoDecodeLinux` to enabled-by-default on
   `USE_V4L2_CODEC` builds.
2. `wayland-allow-direct-egl-gles2.patch` — re-allows the direct
   EGL/GLES2 path in `WaylandSurfaceFactory::GetAllowedGLImplementations()`
   so panfrost's native dmabuf import surfaces to chrome's GL display.
3. `nv12-external-oes-on-modifier-external-only.patch` — extends
   `OzoneImageGLTexturesHolder::GetBinding` to pick
   `GL_TEXTURE_EXTERNAL_OES` when the NV12 dmabuf's modifier is
   advertised `external_only` by the EGL driver. Closes the AFBC/AFRC
   tiled-modifier gap on panfrost / panthor.
4. `v4l2-capture-pool-floor-at-16.patch` — floors the V4L2 capture
   pool depth at 16 buffers so chrome's wayland pipeline depth has
   margin against compositor scheduling jitter.

## Per-board status

| Board | SoC | GPU | Driver | Status |
|---|---|---|---|---|
| `pinetab2/` | RK3566 | Mali-G52 (Bifrost) | panfrost | end-to-end validated |
| `rk3399/` | RK3399 | Mali-T860 (Midgard) | panfrost | V4L2 dispatch validated; no Vulkan (panvk doesn't target Midgard) |
| `rk3588/` | RK3588 | Mali-G610 (Valhall) | panthor | V4L2 dispatch validated; Vulkan-default-on |

The patches themselves are board-agnostic — only PKGBUILDs differ in
launcher defaults (Vulkan-on vs. Vulkan-off) and pkgver-tracking.
