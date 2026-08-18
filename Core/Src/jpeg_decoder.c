/**
 * @file jpeg_decoder.c
 * @brief STM32H743 hardware JPEG decoder with YCbCr→RGB565 conversion
 *
 * Uses the JPEG codec peripheral in polling mode. The hardware outputs
 * YCbCr MCU (Minimum Coded Unit) blocks. We convert them to RGB565.
 *
 * MCU layout for 4:2:0 (most common for MJPEG):
 *   Each MCU covers 16x16 pixels and contains:
 *   - 4 Y blocks  (8x8 each, 256 bytes total)
 *   - 1 Cb block  (8x8, 64 bytes)
 *   - 1 Cr block  (8x8, 64 bytes)
 *   Total: 384 bytes per MCU
 *
 * YCbCr→RGB conversion uses ITU-R BT.601 coefficients with
 * fixed-point integer arithmetic (8 fractional bits).
 */

#include "jpeg_decoder.h"
#include "stm32h7xx_hal_jpeg.h"
#include <stdio.h>
#include <string.h>

/* DWT cycle counter access (core_cm7.h included via stm32h7xx.h) */

/* SDRAM base address: 0xC0000000, 32MB.
 * Layout:
 *   0xC0000000 - 0xC0176FFF: Front framebuffer (1600*480*2 = 1.5MB)
 *   0xC0180000 - 0xC02F6FFF: Back framebuffer (1.5MB)
 *   0xC0300000 - 0xC034B000: MCU decode buffer (320KB)
 */
#define MCU_BUF_ADDR    ((uint8_t *)0xC0300000U)
#define MCU_BUF_SIZE    (320U * 320U * 3U)   /* 307,200 bytes max */

/* Set to 1 to enable per-frame debug output (slows down animation) */
#define JPEG_DEBUG  0

/* JPEG handle */
static JPEG_HandleTypeDef hjpeg;

/* Internal decode state */
static uint32_t decode_width;
static uint32_t decode_height;
static uint32_t decode_mcu_w;   /* MCUs per row */
static uint32_t decode_mcu_h;   /* MCU rows */
static uint16_t *decode_rgb565_out;

/* Black MCU skip statistics */
static uint32_t jpeg_black_mcu_hit;
static uint32_t jpeg_black_mcu_total;

/* MCU → RGB565 conversion (compiled with -O2 in ycbcr_conv.c) */
extern void convert_mcu_420(uint16_t *dst, int stride, const uint8_t *mcu);
extern void convert_mcu_422(uint16_t *dst, int stride, const uint8_t *mcu);
extern void convert_mcu_444(uint16_t *dst, int stride, const uint8_t *mcu);

/*---------------------------------------------------------------------------
 * Black MCU detection & fill helpers (Plan D: skip black MCU conversion)
 *---------------------------------------------------------------------------
 *
 * IMPORTANT: SDRAM (0xC0300000) is non-cacheable, 16-bit bus.
 * Byte-by-byte reads cause 1 SDRAM transaction per byte (192 reads/MCU).
 * 32-bit word reads cause 2 SDRAM transactions per word (96 reads/MCU).
 * Always use word reads for performance — the byte check overhead was
 * the reason JPEG time *increased* from 100ms to 150ms.
 */

/* Check if an MCU is essentially black (Y=0, Cb=128, Cr=128).
 * Uses 32-bit word reads for SDRAM efficiency.
 * y_words, c_words: pre-computed outside the loop to avoid per-MCU if-else. */
__attribute__((always_inline))
static inline int is_mcu_black_words(const uint32_t *mcu32, uint32_t y_words, uint32_t c_words)
{
    /* Check Y blocks: all words must be 0 */
    for (uint32_t i = 0; i < y_words; i++) {
        if (mcu32[i] != 0) return 0;
    }

    /* Check Cb block: all words must be 0x80808080 */
    const uint32_t *cb = mcu32 + y_words;
    for (uint32_t i = 0; i < c_words; i++) {
        if (cb[i] != 0x80808080) return 0;
    }

    /* Check Cr block: all words must be 0x80808080 */
    const uint32_t *cr = cb + c_words;
    for (uint32_t i = 0; i < c_words; i++) {
        if (cr[i] != 0x80808080) return 0;
    }

    return 1;
}

/* Fill MCU area with black pixels (RGB565 = 0x0000).
 * Uses memset for speed — writes to cacheable RAM_D1. */
__attribute__((always_inline))
static inline void fill_mcu_black(uint16_t *dst, int stride, int mcu_w, int mcu_h)
{
    for (int y = 0; y < mcu_h; y++) {
        memset(dst + y * stride, 0, mcu_w * 2);
    }
}

/*---------------------------------------------------------------------------
 * JPEG info ready callback (called by HAL after header parsing)
 *---------------------------------------------------------------------------
 */
void HAL_JPEG_InfoReadyCallback(JPEG_HandleTypeDef *hjpeg_ptr, JPEG_ConfTypeDef *pInfo)
{
    decode_width  = pInfo->ImageWidth;
    decode_height = pInfo->ImageHeight;

#if JPEG_DEBUG
    printf("JPEG: %lux%lu, subsampling=%lu\r\n",
           (unsigned long)decode_width,
           (unsigned long)decode_height,
           (unsigned long)pInfo->ChromaSubsampling);
#endif

    /* Compute MCU dimensions */
    uint32_t mcu_w = 8, mcu_h = 8;
    if (pInfo->ChromaSubsampling == JPEG_420_SUBSAMPLING)
    {
        mcu_w = 16; mcu_h = 16;
    }
    else if (pInfo->ChromaSubsampling == JPEG_422_SUBSAMPLING)
    {
        mcu_w = 16; mcu_h = 8;
    }
    /* else 4:4:4: MCU = 8x8 */

    decode_mcu_w = (decode_width  + mcu_w - 1) / mcu_w;
    decode_mcu_h = (decode_height + mcu_h - 1) / mcu_h;
}

/*---------------------------------------------------------------------------
 * JPEG data ready callback (called by HAL when output data is available)
 *---------------------------------------------------------------------------
 */
void HAL_JPEG_DataReadyCallback(JPEG_HandleTypeDef *hjpeg_ptr,
                                 uint8_t *pDataOut, uint32_t OutDataLength)
{
    (void)hjpeg_ptr;
    (void)pDataOut;
    (void)OutDataLength;
    /* We use polling mode, so this callback is not used.
     * The MCU data is processed after HAL_JPEG_Decode returns. */
}

/*---------------------------------------------------------------------------
 * JPEG decode complete callback
 *---------------------------------------------------------------------------
 */
void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg_ptr)
{
    (void)hjpeg_ptr;
    /* Polling mode: handled in decode function */
}

/*---------------------------------------------------------------------------
 * JPEG error callback
 *---------------------------------------------------------------------------
 */
void HAL_JPEG_ErrorCallback(JPEG_HandleTypeDef *hjpeg_ptr)
{
    printf("JPEG: Error code 0x%08lX\r\n", (unsigned long)hjpeg_ptr->ErrorCode);
}

/*---------------------------------------------------------------------------
 * MSP Init: enable JPEG clock
 *---------------------------------------------------------------------------
 */
void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg_ptr)
{
    if (hjpeg_ptr->Instance == JPEG)
    {
        /* Enable JPEG clock */
        __HAL_RCC_JPEG_CLK_ENABLE();
    }
}

/*---------------------------------------------------------------------------
 * MSP DeInit: disable JPEG clock
 *---------------------------------------------------------------------------
 */
void HAL_JPEG_MspDeInit(JPEG_HandleTypeDef *hjpeg_ptr)
{
    if (hjpeg_ptr->Instance == JPEG)
    {
        __HAL_RCC_JPEG_CLK_DISABLE();
    }
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------
 */

HAL_StatusTypeDef JPEG_Decoder_Init(void)
{
    memset(&hjpeg, 0, sizeof(hjpeg));
    hjpeg.Instance = JPEG;

    HAL_StatusTypeDef status = HAL_JPEG_Init(&hjpeg);
    if (status != HAL_OK)
    {
        printf("JPEG: HAL_JPEG_Init failed\r\n");
        return status;
    }

    printf("JPEG: Hardware codec initialized\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef JPEG_Decode_To_RGB565(const uint8_t *jpeg_data,
                                         uint32_t jpeg_size,
                                         uint16_t *rgb565_out,
                                         JPEG_InfoTypeDef *info)
{
    if (jpeg_data == NULL || rgb565_out == NULL || jpeg_size == 0)
    {
        return HAL_ERROR;
    }

    /* Store output pointer for callback use */
    decode_rgb565_out = rgb565_out;
    decode_width  = 0;
    decode_height = 0;
    decode_mcu_w  = 0;
    decode_mcu_h  = 0;

    /* Step 1: Decode JPEG → YCbCr MCU blocks (polling) */
#if JPEG_DEBUG
    printf("JPEG: Decoding %lu bytes...\r\n", (unsigned long)jpeg_size);
#endif

    #if JPEG_DEBUG
    uint32_t t_hw_start = DWT->CYCCNT;
#endif

    HAL_StatusTypeDef status = HAL_JPEG_Decode(&hjpeg,
                                                (uint8_t *)jpeg_data,
                                                jpeg_size,
                                                MCU_BUF_ADDR,
                                                MCU_BUF_SIZE,
                                                HAL_MAX_DELAY);

#if JPEG_DEBUG
    uint32_t t_hw_end = DWT->CYCCNT;
#endif

    if (status != HAL_OK)
    {
        printf("JPEG: Decode failed (status=%d)\r\n", status);
        return status;
    }

    /* Step 2: Get image info */
    JPEG_ConfTypeDef jpeg_info;
    status = HAL_JPEG_GetInfo(&hjpeg, &jpeg_info);
    if (status != HAL_OK)
    {
        printf("JPEG: GetInfo failed\r\n");
        return status;
    }

    decode_width  = jpeg_info.ImageWidth;
    decode_height = jpeg_info.ImageHeight;

    if (info != NULL)
    {
        info->width       = decode_width;
        info->height      = decode_height;
        info->subsampling = jpeg_info.ChromaSubsampling;
    }

    #if JPEG_DEBUG
    printf("JPEG: Decoded %lux%lu, subsampling=%lu\r\n",
           (unsigned long)decode_width,
           (unsigned long)decode_height,
           (unsigned long)jpeg_info.ChromaSubsampling);
#endif

    /* Step 3: Convert MCU blocks → RGB565 */
    uint32_t mcu_w = 8, mcu_h = 8;
    uint32_t mcu_bytes = 64 * 3;  /* 4:4:4: 3 blocks * 64 bytes */
    uint32_t y_words = 16, c_words = 16;  /* for is_mcu_black_words */

    if (jpeg_info.ChromaSubsampling == JPEG_420_SUBSAMPLING)
    {
        mcu_w = 16; mcu_h = 16;
        mcu_bytes = 384;  /* 6 blocks * 64 bytes */
        y_words = 64; c_words = 16;
    }
    else if (jpeg_info.ChromaSubsampling == JPEG_422_SUBSAMPLING)
    {
        mcu_w = 16; mcu_h = 8;
        mcu_bytes = 256;  /* 4 blocks * 64 bytes */
        y_words = 32; c_words = 16;
    }

    decode_mcu_w = (decode_width  + mcu_w - 1) / mcu_w;
    decode_mcu_h = (decode_height + mcu_h - 1) / mcu_h;

    const uint8_t *mcu_ptr = MCU_BUF_ADDR;

    jpeg_black_mcu_hit   = 0;
    jpeg_black_mcu_total = 0;

    for (uint32_t mcu_row = 0; mcu_row < decode_mcu_h; mcu_row++)
    {
        for (uint32_t mcu_col = 0; mcu_col < decode_mcu_w; mcu_col++)
        {
            /* Destination position in output framebuffer */
            uint32_t dst_x = mcu_col * mcu_w;
            uint32_t dst_y = mcu_row * mcu_h;
            uint16_t *dst = decode_rgb565_out + dst_y * decode_width + dst_x;

            /* Plan D: skip black MCU conversion.
             * Pre-computed y_words/c_words avoids per-MCU subsampling if-else. */
            if (is_mcu_black_words((const uint32_t *)mcu_ptr, y_words, c_words))
            {
                fill_mcu_black(dst, decode_width, mcu_w, mcu_h);
                jpeg_black_mcu_hit++;
            }
            else if (jpeg_info.ChromaSubsampling == JPEG_420_SUBSAMPLING)
            {
                convert_mcu_420(dst, decode_width, mcu_ptr);
            }
            else if (jpeg_info.ChromaSubsampling == JPEG_422_SUBSAMPLING)
            {
                convert_mcu_422(dst, decode_width, mcu_ptr);
            }
            else
            {
                convert_mcu_444(dst, decode_width, mcu_ptr);
            }

            mcu_ptr += mcu_bytes;
            jpeg_black_mcu_total++;
        }
    }

    #if JPEG_DEBUG
    uint32_t t_conv_end = DWT->CYCCNT;
    printf("JPEG: HW=%luus MCU=%luus\r\n",
           (t_hw_end - t_hw_start) / 200,
           (t_conv_end - t_hw_end) / 200);
#endif

    return HAL_OK;
}

/* Return black MCU skip ratio in permille (0-1000).
 * 1000 = all MCUs skipped, 0 = none skipped. */
int JPEG_Get_BlackMCU_Ratio(void)
{
    if (jpeg_black_mcu_total == 0) return 0;
    return (int)((jpeg_black_mcu_hit * 1000) / jpeg_black_mcu_total);
}