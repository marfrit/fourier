# kwin-fourier

A diagnostic patch that no-ops `Transaction::watchDmaBuf` in KWin
6.6.4. **Test fixture, not the upstream-bound shape.**

## Background

KWin's `Transaction::watchDmaBuf`
(`src/wayland/transaction.cpp:265`) calls
`DMA_BUF_IOCTL_EXPORT_SYNC_FILE` on every plane of every imported
dmabuf and parks the transaction on a `QSocketNotifier(POLLIN)`
waiting for the resulting sync_file fd to become readable.

For V4L2-produced dmabufs (hantro / rockchip-rga and any other vb2
driver), that fence either:

- **Stub-signals immediately**, because vb2 doesn't populate
  `dma_resv` exclusive fences (see kernel layer in the top-level
  README) and `dma_buf_export_sync_file` substitutes
  `dma_fence_get_stub()`. Pure latency cost: a synchronous ioctl +
  socket-notifier setup per frame, for a fence that signals in
  microseconds and represents nothing real.
- **Signals very late or not at all**, on edge cases that we hit
  during the chromium-fourier validation campaign. KWin's
  transaction parks indefinitely; the previous wl_buffer never gets
  released to the client; the client's V4L2 capture pool starves;
  hard stall.

## Patches

This directory carries **two** patches — the PKGBUILD applies only
`0001` for now (validated on ohm), while `0002` is the
upstream-bound shape staged here for later validation and
submission.

### `0001-transaction-bypass-watchDmaBuf-fence-wait.patch` *(currently shipped)*

No-ops `Transaction::watchDmaBuf` entirely. Every transaction
commits without waiting on implicit-sync fences for the dmabufs
it imports. **Test fixture, validated end-to-end on ohm**: the
patch unblocks chromium-fourier 1080p30 H.264 playback under KDE
Plasma 6.6.4 Wayland on RK3566 + panfrost + mainline 6.19.

### `0002-transaction-poll-dmabuf-fd-directly-upstream-shape.patch` *(unvalidated, upstream-bound)*

Rewrites `Transaction::watchDmaBuf` to call `poll(POLLIN)` on the
dmabuf fd directly via a duplicated fd in a `QSocketNotifier`,
instead of going through `DMA_BUF_IOCTL_EXPORT_SYNC_FILE` plus a
sync_file fd. The dma-buf core has supported polling the dmabuf fd
for implicit-sync write fences since the introduction of the
feature; the export-then-poll round-trip is per-frame syscall
overhead with no semantic difference.

This shape preserves KWin's defense — the wait still actually
*waits* on the producer's fence — while shedding the per-frame
overhead. It is **not validated yet** and is offered here as the
shape upstream review will likely converge on. Validation gates
before swapping the PKGBUILD to apply 0002 instead of 0001:

1. Build kwin-fourier with 0002 instead of 0001 (one PKGBUILD line
   change).
2. Install on ohm; restart Plasma session so the new
   `kwin_wayland` is mapped.
3. Run chromium-fourier + bbb sample as before. Expectation:
   plays through end-to-end at the same ~81 % combined CPU.
   Equivalence with 0001 confirms the upstream shape works
   without weakening defenses.
4. Capture before/after `dma_buf_export_sync_file` syscall counts
   via `strace -c` on `kwin_wayland` (the per-frame syscall savings
   are the patch's claimed benefit).
5. Submit to invent.kde.org/plasma/kwin against `master`.

## Why patch 0001 is *not* the upstream-bound shape

Wayland's security model is "compositor trusts no client" —
watchDmaBuf is a defense against a misbehaving client that attaches
a buffer the GPU is still writing. The blanket no-op makes a
correctness-equivalent assumption (`every Wayland client honors the
spec`) that KWin maintainers are reasonably unwilling to take
unconditionally.

**The upstream-correct fix lives in the kernel** (vb2 / hantro /
rga don't populate `dma_resv` — fix that, KWin's wait now works
correctly because the fence is real). Once the kernel side lands,
KWin can either keep its current wait (now correct) or migrate to
`poll(POLLIN)` directly on the dmabuf fd, skipping the
`EXPORT_SYNC_FILE` ioctl.

The kwin-fourier patch in this repo is the **diagnostic** that
identified the kernel bug and lets the chromium-fourier validation
proceed today on stock kernel + KWin. It will be rewritten or
removed once the kernel side is upstream.

## Building / installing

```sh
makepkg -si
```

The PKGBUILD inherits from upstream Arch's kwin 6.6.4-1, applies
the single watchDmaBuf bypass, and bumps `epoch=1` to dominate
upstream pkgrel.

## Side effect

Across the test session, every wp_linux_dmabuf client on the
compositor (chrome, brave, mpv, VLC, …) feels markedly snappier on
Mali-class hardware because the per-frame sync_file roundtrip is
gone. A pleasant accident; the cleaner, kernel-side fix will
preserve the speedup without weakening the defense.
