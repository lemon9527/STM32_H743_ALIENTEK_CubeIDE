/* qspi_video.c - QSPI Flash video frame storage
 *
 * On first boot, writes embedded header + frame table to QSPI Flash.
 * If raw frame data is missing, enters UART receive mode to program it.
 * On subsequent boots, detects valid data and skips programming.
 * During animation, provides memory-mapped addresses for DMA2D copy.
 *
 * QSPI Flash layout (W25Q256):
 *   0x00000000:  header (8 bytes: num_frames + version)
 *   0x00000008:  frame table (num_frames * 8 bytes: offset + size)
 *   0x00000xxx:  raw RGB565 frame data (num_frames * 204800 bytes)
 */

#include "video_frames.h"
#include "qspi_flash.h"
#include <stdio.h>
#include <string.h>

/* UART handle for frame data programming (initialized by CubeMX in usart.c) */
extern UART_HandleTypeDef huart1;

#define QSPI_BASE_ADDR      0x00000000U
#define QSPI_MMAP_BASE      0x90000000U
#define QSPI_HEADER_SIZE    8U
#define QSPI_ENTRY_SIZE     8U
#define QSPI_SECTOR_SIZE    4096U
#define QSPI_PAGE_SIZE      256U

/*---------------------------------------------------------------------------
 * Memory-mapped QSPI read helper
 *---------------------------------------------------------------------------*/
static inline void mmap_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    memcpy(buf, (const void *)(QSPI_MMAP_BASE + addr), len);
}

/*---------------------------------------------------------------------------
 * Check if QSPI Flash already has valid frame data (header + table + data)
 *---------------------------------------------------------------------------*/
static int QSPI_Video_CheckValid(void)
{
    VideoFramesHeader qspi_hdr, emb_hdr;

    memcpy(&emb_hdr, _binary_test_frames_bin_start, sizeof(emb_hdr));

    if (QSPI_Read(QSPI_BASE_ADDR, (uint8_t *)&qspi_hdr, sizeof(qspi_hdr)) != HAL_OK)
    {
        return 0;
    }

    if (qspi_hdr.num_frames == emb_hdr.num_frames
        && qspi_hdr.version == emb_hdr.version)
    {
        printf("QSPI: Video data OK (%lu frames, version %lu)\r\n",
               qspi_hdr.num_frames, qspi_hdr.version);
        return 1;
    }

    printf("QSPI: Data mismatch (QSPI=%lu/%lu, embedded=%lu/%lu)\r\n",
           qspi_hdr.num_frames, qspi_hdr.version,
           emb_hdr.num_frames, emb_hdr.version);
    return 0;
}

/*---------------------------------------------------------------------------
 * Write embedded header + frame table to QSPI Flash
 *---------------------------------------------------------------------------*/
HAL_StatusTypeDef QSPI_Video_WriteToFlash(void)
{
    if (QSPI_Video_CheckValid())
    {
        return HAL_OK;  /* Already valid, skip */
    }

    const uint8_t *data = _binary_test_frames_bin_start;
    uint32_t total_size = VIDEO_FRAMES_BIN_SIZE;
    uint32_t num_sectors = (total_size + QSPI_SECTOR_SIZE - 1) / QSPI_SECTOR_SIZE;

    printf("QSPI: Writing header + table (%lu bytes, %lu sectors)...\r\n",
           total_size, num_sectors);

    /* Erase required sectors */
    for (uint32_t i = 0; i < num_sectors; i++)
    {
        uint32_t addr = QSPI_BASE_ADDR + i * QSPI_SECTOR_SIZE;
        if (QSPI_SectorErase(addr) != HAL_OK)
        {
            printf("QSPI: Erase failed at sector %lu\r\n", i);
            return HAL_ERROR;
        }
    }
    printf("QSPI: Erase done\r\n");

    /* Write data page by page */
    uint32_t written = 0;
    while (written < total_size)
    {
        uint32_t chunk = total_size - written;
        if (chunk > QSPI_PAGE_SIZE) chunk = QSPI_PAGE_SIZE;

        if (QSPI_Write(QSPI_BASE_ADDR + written, data + written, chunk) != HAL_OK)
        {
            printf("QSPI: Write failed at offset %lu\r\n", written);
            return HAL_ERROR;
        }
        written += chunk;
    }

    printf("QSPI: Header written (%lu bytes)\r\n", total_size);
    return HAL_OK;
}

/*---------------------------------------------------------------------------
 * Receive raw frame data via UART and write to QSPI Flash
 *
 * Protocol:
 *   MCU → PC: "READY\r\n"
 *   PC  → MCU: [total_size:4B LE]
 *   MCU: erases QSPI sectors for data area
 *   MCU → PC: "ERASE_OK\r\n"
 *   PC  → MCU: [data: 256B chunks] ...
 *   MCU: writes each chunk to QSPI Flash
 *   MCU → PC: "DONE\r\n"
 *---------------------------------------------------------------------------*/
HAL_StatusTypeDef QSPI_Video_ProgramFrameData(void)
{
    uint32_t total_size;
    uint8_t size_buf[4];
    uint8_t page_buf[QSPI_PAGE_SIZE];
    uint32_t received;

    printf("\r\nQSPI: Waiting for frame data via UART...\r\n");

    /* Repeatedly send READY until PC sends first byte of size header.
     * This avoids race conditions where READY is missed if PC connects late. */
    while (1)
    {
        printf("READY\r\n");

        /* Try to get first byte with short timeout */
        if (HAL_UART_Receive(&huart1, size_buf, 1, 500) == HAL_OK)
        {
            received = 1;
            break;  /* Got first byte */
        }
        /* Else: timeout, re-send READY */
    }

    /* Receive remaining 3 bytes of size header */
    while (received < 4)
    {
        uint8_t c;
        if (HAL_UART_Receive(&huart1, &c, 1, 10000) != HAL_OK)
        {
            printf("QSPI: UART timeout waiting for size\r\n");
            return HAL_ERROR;
        }
        size_buf[received++] = c;
    }
    total_size = size_buf[0] | (size_buf[1] << 8)
               | (size_buf[2] << 16) | (size_buf[3] << 24);
    printf("QSPI: Receiving %lu KB of frame data...\r\n", total_size / 1024);

    /* Erase sectors for the data area */
    uint32_t data_start = QSPI_BASE_ADDR + QSPI_DATA_OFFSET;
    uint32_t num_sectors = (total_size + QSPI_SECTOR_SIZE - 1) / QSPI_SECTOR_SIZE;
    printf("QSPI: Erasing %lu sectors for data area...\r\n", num_sectors);

    for (uint32_t i = 0; i < num_sectors; i++)
    {
        uint32_t addr = data_start + i * QSPI_SECTOR_SIZE;
        if (QSPI_SectorErase(addr) != HAL_OK)
        {
            printf("QSPI: Erase failed at sector %lu (addr=0x%08lX)\r\n", i, addr);
            return HAL_ERROR;
        }
        if ((i % 64) == 0)
        {
            printf("QSPI: Erasing sector %lu/%lu...\r\n", i + 1, num_sectors);
        }
    }
    printf("QSPI: Erase done\r\n");
    printf("ERASE_OK\r\n");

    /* Receive and write data page by page */
    uint32_t written = 0;
    uint32_t qspi_addr = data_start;

    while (written < total_size)
    {
        /* Receive one page (256 bytes) */
        uint32_t chunk = total_size - written;
        if (chunk > QSPI_PAGE_SIZE) chunk = QSPI_PAGE_SIZE;

        received = 0;
        while (received < chunk)
        {
            uint8_t c;
            if (HAL_UART_Receive(&huart1, &c, 1, 300000) != HAL_OK)
            {
                printf("QSPI: UART timeout at byte %lu/%lu\r\n", written + received, total_size);
                return HAL_ERROR;
            }
            page_buf[received++] = c;
        }

        /* Write page to QSPI Flash */
        if (QSPI_Write(qspi_addr, page_buf, chunk) != HAL_OK)
        {
            printf("QSPI: Write failed at offset 0x%08lX\r\n", qspi_addr);
            return HAL_ERROR;
        }

        qspi_addr += chunk;
        written += chunk;

        /* Acknowledge this chunk so PC knows to send next */
        printf("ACK\r\n");
    }

    printf("QSPI: Frame data written (%lu KB)\r\n", total_size / 1024);
    printf("DONE\r\n");
    return HAL_OK;
}

/*---------------------------------------------------------------------------
 * Check if raw frame data is programmed on QSPI Flash
 * Uses direct QSPI_Read (no mmap needed).
 * Verifies BOTH the start and end of the data area are non-erased.
 * Returns 1 if data is present (not all 0xFF), 0 if erased/incomplete.
 *---------------------------------------------------------------------------*/
int QSPI_Video_IsDataProgrammed(void)
{
    uint32_t data_check_start, data_check_end;
    uint32_t data_addr = QSPI_BASE_ADDR + QSPI_DATA_OFFSET;

    /* Total raw data size = num_frames * FRAME_RAW_SIZE */
    VideoFramesHeader emb_hdr;
    memcpy(&emb_hdr, _binary_test_frames_bin_start, sizeof(emb_hdr));
    uint32_t total = emb_hdr.num_frames * FRAME_RAW_SIZE;

    if (QSPI_Read(data_addr, (uint8_t *)&data_check_start, sizeof(data_check_start)) != HAL_OK)
    {
        printf("QSPI: Failed to read data area\r\n");
        return 0;
    }
    if (QSPI_Read(data_addr + total - 4, (uint8_t *)&data_check_end, sizeof(data_check_end)) != HAL_OK)
    {
        printf("QSPI: Failed to read data area (end)\r\n");
        return 0;
    }

    /* Erased Flash reads as 0xFFFFFFFF. If start OR end is erased,
     * the data is missing or incomplete → needs programming. */
    if (data_check_start == 0xFFFFFFFF || data_check_end == 0xFFFFFFFF)
    {
        printf("QSPI: Frame data area is erased/incomplete (needs programming)\r\n");
        return 0;
    }

    return 1;
}

/*---------------------------------------------------------------------------
 * Get frame table entry for a given index (memory-mapped)
 *---------------------------------------------------------------------------*/
static void QSPI_Video_GetEntry(uint32_t index, uint32_t *offset, uint32_t *size)
{
    uint32_t table_addr = QSPI_BASE_ADDR + QSPI_HEADER_SIZE + index * QSPI_ENTRY_SIZE;
    VideoFrameEntry entry;

    mmap_read(table_addr, (uint8_t *)&entry, sizeof(entry));
    *offset = entry.offset;
    *size = entry.size;
}

/*---------------------------------------------------------------------------
 * Get QSPI memory-mapped address for a specific frame
 * Returns 0x90000000 + offset, or 0 on error.
 *---------------------------------------------------------------------------*/
uint32_t QSPI_Video_GetFrameAddr(uint32_t frame_index)
{
    uint32_t offset, size;

    QSPI_Video_GetEntry(frame_index, &offset, &size);

    if (size == 0 || offset < QSPI_DATA_OFFSET)
    {
        return 0;
    }

    return QSPI_MMAP_BASE + offset;
}

/*---------------------------------------------------------------------------
 * Get number of frames stored in QSPI Flash (memory-mapped)
 *---------------------------------------------------------------------------*/
uint32_t QSPI_Video_GetFrameCount(void)
{
    VideoFramesHeader hdr;
    mmap_read(QSPI_BASE_ADDR, (uint8_t *)&hdr, sizeof(hdr));
    return hdr.num_frames;
}