/**
 * @file jpeg_decoder.h
 * @brief STM32H743 hardware JPEG decoder wrapper
 *
 * Uses the hardware JPEG codec (polling mode) to decode JPEG data
 * and convert YCbCr MCU output to RGB565 for the LCD framebuffer.
 */
#ifndef __JPEG_DECODER_H__
#define __JPEG_DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* JPEG decode result */
typedef struct {
    uint32_t width;        /* Image width in pixels */
    uint32_t height;       /* Image height in pixels */
    uint32_t subsampling;  /* Chroma subsampling: 0=4:4:4, 1=4:2:2, 2=4:2:0 */
} JPEG_InfoTypeDef;

/* Initialize the JPEG hardware codec.
 * Must be called once before any decode operations.
 * Returns HAL_OK on success. */
HAL_StatusTypeDef JPEG_Decoder_Init(void);

/* Decode JPEG data and convert to RGB565.
 * @param jpeg_data   Pointer to JPEG bitstream in memory
 * @param jpeg_size   Size of JPEG data in bytes
 * @param rgb565_out  Output buffer for RGB565 pixels (must be pre-allocated,
 *                    size >= width * height * 2 bytes)
 * @param info        [out] Decoded image information (dimensions, subsampling)
 * @return HAL_OK on success, HAL_ERROR on decode failure
 *
 * The output buffer receives RGB565 pixels in row-major order.
 * Caller is responsible for allocating a large enough buffer.
 */
HAL_StatusTypeDef JPEG_Decode_To_RGB565(const uint8_t *jpeg_data,
                                         uint32_t jpeg_size,
                                         uint16_t *rgb565_out,
                                         JPEG_InfoTypeDef *info);

/* Return black MCU skip ratio in permille (0-1000).
 * 1000 = all MCUs skipped (all black image), 0 = none skipped. */
int JPEG_Get_BlackMCU_Ratio(void);

#ifdef __cplusplus
}
#endif

#endif /* __JPEG_DECODER_H__ */