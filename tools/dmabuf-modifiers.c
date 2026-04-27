/* dmabuf-modifiers — dump EGL_EXT_image_dma_buf_import_modifiers table.
 *
 * Build:  gcc -o dmabuf-modifiers dmabuf-modifiers.c -lEGL
 * Run:    ./dmabuf-modifiers   (uses GBM on /dev/dri/renderD128)
 *
 * Prints every (fourcc, modifier) the mesa EGL implementation says it
 * can import. Shipped under chromium-fourier as a community-targetable
 * snapshot tool — same answer that gates zero-copy in mpv, gstreamer,
 * chromium, and any other dmabuf consumer. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>

#ifndef EGL_PLATFORM_GBM_MESA
#define EGL_PLATFORM_GBM_MESA 0x31D7
#endif

static const char *fcc_str(uint32_t f, char buf[5]) {
    buf[0] =  f        & 0xff;
    buf[1] = (f >>  8) & 0xff;
    buf[2] = (f >> 16) & 0xff;
    buf[3] = (f >> 24) & 0xff;
    buf[4] = 0;
    for (int i = 0; i < 4; i++)
        if (buf[i] < 0x20 || buf[i] > 0x7e) buf[i] = '?';
    return buf;
}

int main(void) {
    int drm_fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) { perror("open(/dev/dri/renderD128)"); return 1; }

    struct gbm_device *gbm = gbm_create_device(drm_fd);
    if (!gbm) { fprintf(stderr, "gbm_create_device failed\n"); return 1; }

    EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gbm, NULL);
    if (dpy == EGL_NO_DISPLAY) { fprintf(stderr, "eglGetPlatformDisplay failed\n"); return 1; }
    EGLint major, minor;
    if (!eglInitialize(dpy, &major, &minor)) { fprintf(stderr, "eglInitialize failed: 0x%x\n", eglGetError()); return 1; }
    printf("EGL %d.%d on %s\n", major, minor, eglQueryString(dpy, EGL_VENDOR));
    printf("EGL extensions: %s\n\n", eglQueryString(dpy, EGL_EXTENSIONS));

    PFNEGLQUERYDMABUFFORMATSEXTPROC q_fmts =
        (void*)eglGetProcAddress("eglQueryDmaBufFormatsEXT");
    PFNEGLQUERYDMABUFMODIFIERSEXTPROC q_mods =
        (void*)eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    if (!q_fmts || !q_mods) {
        fprintf(stderr, "EGL_EXT_image_dma_buf_import_modifiers not exposed\n");
        return 1;
    }

    EGLint nfmt = 0;
    q_fmts(dpy, 0, NULL, &nfmt);
    EGLint *fmts = calloc(nfmt, sizeof(EGLint));
    q_fmts(dpy, nfmt, fmts, &nfmt);
    printf("== %d importable formats ==\n", nfmt);

    for (int i = 0; i < nfmt; i++) {
        char b[5];
        EGLint nmod = 0;
        q_mods(dpy, fmts[i], 0, NULL, NULL, &nmod);
        printf("%s (0x%08x) — %d modifiers", fcc_str(fmts[i], b), fmts[i], nmod);
        if (nmod > 0) {
            EGLuint64KHR *mods = calloc(nmod, sizeof(EGLuint64KHR));
            EGLBoolean   *ext  = calloc(nmod, sizeof(EGLBoolean));
            q_mods(dpy, fmts[i], nmod, mods, ext, &nmod);
            printf(":\n");
            for (int j = 0; j < nmod; j++)
                printf("    0x%016llx%s\n",
                       (unsigned long long)mods[j],
                       ext[j] ? " (external_only)" : "");
            free(mods); free(ext);
        } else {
            printf("\n");
        }
    }
    return 0;
}
