/* qspi_video.h - QSPI Flash video frame storage */
#ifndef __QSPI_VIDEO_H__
#define __QSPI_VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Write embedded header + frame table to QSPI Flash (first boot only) */
HAL_StatusTypeDef QSPI_Video_WriteToFlash(void);

/* Receive raw frame data via UART and write to QSPI Flash.
 * Called when QSPI data is missing or version mismatch.
 * Blocks until all data is received or timeout. */
HAL_StatusTypeDef QSPI_Video_ProgramFrameData(void);

/* Get QSPI memory-mapped address for a specific frame.
 * Returns 0x90000000 + offset, or 0 on error. */
uint32_t QSPI_Video_GetFrameAddr(uint32_t frame_index);

/* Check if raw frame data is programmed on QSPI Flash (direct read, no mmap).
 * Returns 1 if data is present, 0 if erased/missing. */
int QSPI_Video_IsDataProgrammed(void);

/* Get number of frames stored in QSPI Flash */
uint32_t QSPI_Video_GetFrameCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __QSPI_VIDEO_H__ */