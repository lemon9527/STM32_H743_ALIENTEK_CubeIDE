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

#ifdef __cplusplus
 }
#endif

#endif /* __QSPI_FLASH_H__ */
