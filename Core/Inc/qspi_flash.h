/* qspi_flash.h - simple W25Q256 driver (init + JEDEC ID + memory-mapped) */
#ifndef __QSPI_FLASH_H__
#define __QSPI_FLASH_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"

/* Initialize QSPI flash and basic probing */
HAL_StatusTypeDef QSPI_Flash_Init(void);

/* Read JEDEC ID (3 bytes) into buffer (must be at least 3 bytes) */
HAL_StatusTypeDef QSPI_ReadJEDECID(uint8_t *id);

/* Enter 4-byte addressing mode */
HAL_StatusTypeDef QSPI_Enter_4Byte_AddressMode(void);

/* Enable memory-mapped mode (maps QSPI to 0x90000000) */
HAL_StatusTypeDef QSPI_EnableMemoryMappedMode(void);

/* Read data from QSPI Flash using indirect read mode.
 * @param addr  Flash address (0-based, 4-byte addressing)
 * @param buf   Output buffer
 * @param len   Number of bytes to read
 * @return HAL_OK on success */
HAL_StatusTypeDef QSPI_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/* Sector erase (4KB) for W25Q256.
 * @param addr  Flash address (must be sector-aligned)
 * @return HAL_OK on success */
HAL_StatusTypeDef QSPI_SectorErase(uint32_t addr);

/* Write data to QSPI Flash (page program, max 256 bytes per call).
 * @param addr  Flash address
 * @param buf   Data to write
 * @param len   Number of bytes (max 256)
 * @return HAL_OK on success */
HAL_StatusTypeDef QSPI_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
 }
#endif

#endif /* __QSPI_FLASH_H__ */
