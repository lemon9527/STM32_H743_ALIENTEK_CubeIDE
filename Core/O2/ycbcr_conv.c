/**
 * ycbcr_conv.c - YCbCr → RGB565 conversion (compiled with -O2)
 *
 * Separated from jpeg_decoder.c so these hot-path functions can be
 * compiled with -O2 while the rest of the project uses -O0.
 *
 * Optimization: MCU data is read from non-cacheable SDRAM into a
 * stack-local buffer (DTCM) using 32-bit burst reads, then processed
 * from DTCM (zero-wait-state). This reduces SDRAM transactions from
 * ~320 byte reads to 96 uint32_t reads per MCU.
 */
#include <stdint.h>

/* ITU-R BT.601 YCbCr → RGB565 (fixed-point, 8 fractional bits) */
static inline int clip8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static inline uint16_t ycbcr_to_rgb565(int y, int cb, int cr)
{
    int Cr = cr - 128;
    int Cb = cb - 128;
    int r = y + ((359 * Cr) >> 8);
    int g = y - ((88  * Cb) >> 8) - ((183 * Cr) >> 8);
    int b = y + ((454 * Cb) >> 8);
    r = clip8(r);
    g = clip8(g);
    b = clip8(b);
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* Convert one 4:2:0 MCU (16x16 pixels) to RGB565.
 * Copies MCU from SDRAM (non-cacheable) to stack (DTCM) with
 * 32-bit burst reads, then processes from fast DTCM.
 *
 * MCU layout (384 bytes = 96 x uint32_t):
 *   Y0[0..15]  Y1[0..15]  Y2[0..15]  Y3[0..15]  Cb[0..15]  Cr[0..15]
 *   (64 bytes) (64 bytes) (64 bytes) (64 bytes) (64 bytes) (64 bytes) */
void convert_mcu_420(uint16_t *dst, int stride, const uint8_t *mcu)
{
    /* Copy entire MCU from SDRAM to local stack with sequential 32-bit reads.
     * Manual loop ensures predictable LDR word access (SDRAM page mode).
     * Avoid memcpy() which may use LDM or byte access unpredictable on SDRAM. */
    uint32_t local[96];
    const uint32_t *src = (const uint32_t *)mcu;
    for (int i = 0; i < 96; i++) {
        local[i] = src[i];
    }

    const uint8_t *Y0 = (const uint8_t *)&local[ 0];  /* offset 0 */
    const uint8_t *Y1 = (const uint8_t *)&local[16];  /* offset 64 */
    const uint8_t *Y2 = (const uint8_t *)&local[32];  /* offset 128 */
    const uint8_t *Y3 = (const uint8_t *)&local[48];  /* offset 192 */
    const uint8_t *Cb = (const uint8_t *)&local[64];  /* offset 256 */
    const uint8_t *Cr = (const uint8_t *)&local[80];  /* offset 320 */

    /* Top half: quadrants TL(Y0) + TR(Y1) */
    for (int cy = 0; cy < 4; cy++) {
        int py = cy * 2;
        uint16_t *r0_tl = dst + py * stride;
        uint16_t *r1_tl = r0_tl + stride;
        uint16_t *r0_tr = r0_tl + 8;
        uint16_t *r1_tr = r1_tl + 8;
        for (int cx = 0; cx < 4; cx++) {
            int cb = Cb[cy * 8 + cx];
            int cr = Cr[cy * 8 + cx];
            int yr = cy * 2, xc = cx * 2, px = cx * 2;

            r0_tl[px]     = ycbcr_to_rgb565(Y0[yr * 8 + xc],     cb, cr);
            r0_tl[px + 1] = ycbcr_to_rgb565(Y0[yr * 8 + xc + 1], cb, cr);
            r1_tl[px]     = ycbcr_to_rgb565(Y0[(yr + 1) * 8 + xc],     cb, cr);
            r1_tl[px + 1] = ycbcr_to_rgb565(Y0[(yr + 1) * 8 + xc + 1], cb, cr);

            r0_tr[px]     = ycbcr_to_rgb565(Y1[yr * 8 + xc],     cb, cr);
            r0_tr[px + 1] = ycbcr_to_rgb565(Y1[yr * 8 + xc + 1], cb, cr);
            r1_tr[px]     = ycbcr_to_rgb565(Y1[(yr + 1) * 8 + xc],     cb, cr);
            r1_tr[px + 1] = ycbcr_to_rgb565(Y1[(yr + 1) * 8 + xc + 1], cb, cr);
        }
    }

    /* Bottom half: quadrants BL(Y2) + BR(Y3) */
    for (int cy = 0; cy < 4; cy++) {
        int py = (cy + 4) * 2;
        uint16_t *r0_bl = dst + py * stride;
        uint16_t *r1_bl = r0_bl + stride;
        uint16_t *r0_br = r0_bl + 8;
        uint16_t *r1_br = r1_bl + 8;
        for (int cx = 0; cx < 4; cx++) {
            int cb = Cb[(cy + 4) * 8 + cx];
            int cr = Cr[(cy + 4) * 8 + cx];
            int yr = cy * 2, xc = cx * 2, px = cx * 2;

            r0_bl[px]     = ycbcr_to_rgb565(Y2[yr * 8 + xc],     cb, cr);
            r0_bl[px + 1] = ycbcr_to_rgb565(Y2[yr * 8 + xc + 1], cb, cr);
            r1_bl[px]     = ycbcr_to_rgb565(Y2[(yr + 1) * 8 + xc],     cb, cr);
            r1_bl[px + 1] = ycbcr_to_rgb565(Y2[(yr + 1) * 8 + xc + 1], cb, cr);

            r0_br[px]     = ycbcr_to_rgb565(Y3[yr * 8 + xc],     cb, cr);
            r0_br[px + 1] = ycbcr_to_rgb565(Y3[yr * 8 + xc + 1], cb, cr);
            r1_br[px]     = ycbcr_to_rgb565(Y3[(yr + 1) * 8 + xc],     cb, cr);
            r1_br[px + 1] = ycbcr_to_rgb565(Y3[(yr + 1) * 8 + xc + 1], cb, cr);
        }
    }
}

/* Convert one 4:2:2 MCU (16x8 pixels) to RGB565.
 * Copies MCU from SDRAM to stack (DTCM) with 32-bit reads.
 * MCU layout: Y0[64] Y1[64] Cb[64] Cr[64] = 256 bytes = 64 x uint32_t */
void convert_mcu_422(uint16_t *dst, int stride, const uint8_t *mcu)
{
    uint32_t local[64];
    const uint32_t *src = (const uint32_t *)mcu;
    for (int i = 0; i < 64; i++) {
        local[i] = src[i];
    }

    const uint8_t *Y0 = (const uint8_t *)&local[ 0];
    const uint8_t *Y1 = (const uint8_t *)&local[16];
    const uint8_t *Cb = (const uint8_t *)&local[32];
    const uint8_t *Cr = (const uint8_t *)&local[48];

    for (int by = 0; by < 8; by++) {
        uint16_t *row = dst + by * stride;
        for (int cx = 0; cx < 8; cx++) {
            int px = cx * 2;
            int cb = Cb[by * 8 + cx];
            int cr = Cr[by * 8 + cx];
            const uint8_t *Y = (cx < 4) ? Y0 : Y1;
            int xc = (cx % 4) * 2;
            row[px]     = ycbcr_to_rgb565(Y[by * 8 + xc],     cb, cr);
            row[px + 1] = ycbcr_to_rgb565(Y[by * 8 + xc + 1], cb, cr);
        }
    }
}

/* Convert one 4:4:4 MCU (8x8 pixels) to RGB565.
 * Copies MCU from SDRAM to stack (DTCM) with 32-bit reads.
 * MCU layout: Y[64] Cb[64] Cr[64] = 192 bytes = 48 x uint32_t */
void convert_mcu_444(uint16_t *dst, int stride, const uint8_t *mcu)
{
    uint32_t local[48];
    const uint32_t *src = (const uint32_t *)mcu;
    for (int i = 0; i < 48; i++) {
        local[i] = src[i];
    }

    const uint8_t *Y  = (const uint8_t *)&local[ 0];
    const uint8_t *Cb = (const uint8_t *)&local[16];
    const uint8_t *Cr = (const uint8_t *)&local[32];

    for (int by = 0; by < 8; by++) {
        uint16_t *row = dst + by * stride;
        for (int bx = 0; bx < 8; bx++) {
            int idx = by * 8 + bx;
            row[bx] = ycbcr_to_rgb565(Y[idx], Cb[idx], Cr[idx]);
        }
    }
}