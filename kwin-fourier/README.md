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

## Patch

`0001-transaction-bypass-watchDmaBuf-fence-wait.patch` no-ops the
function. Every transaction commits without waiting on
implicit-sync fences for the dmabufs it imports.

## Why this is *not* the upstream-bound shape

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
