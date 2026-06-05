#ifndef CONFIG_H
#define CONFIG_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAGIC        0x44454C32UL  /* "2LED" — increment on struct changes */

/* STM32G474 has 512 KB dual-bank flash (2 KB pages, 128 pages per bank).
   Config is stored in the last page of Bank 2. */
#define CONFIG_FLASH_PAGE   ((FLASH_SIZE / FLASH_PAGE_SIZE / 2U) - 1U)  /* 127 - bank-relative */
#define CONFIG_FLASH_BANK   FLASH_BANK_2
#define CONFIG_FLASH_ADDR   (FLASH_BASE + FLASH_SIZE - FLASH_PAGE_SIZE)

#define CONFIG_NUM_BAUDS    11U

/* Config address space (passed to config_get/set/validate):
     0–15  : max_brightness per channel (0–4095)
    16–31  : ramp_time_ms per channel   (0–65535)
    32–36  : comm config (slave_addr, baud_code, parity, data_bits, stop_bits) */
#define CONFIG_NUM_REGS         37U
#define CONFIG_REG_COMM_BASE    32U
#define CONFIG_NUM_COMM_REGS    5U

extern const uint32_t config_baud_table[CONFIG_NUM_BAUDS];

typedef struct {
    uint32_t magic;               /* offset  0, size  4 */
    uint8_t  slave_addr;          /* offset  4          */
    uint8_t  baud_code;           /* offset  5          */
    uint8_t  parity;              /* offset  6          */
    uint8_t  data_bits;           /* offset  7          */
    uint8_t  stop_bits;           /* offset  8          */
    uint8_t  _pad[1];             /* offset  9 — align uint16_t */
    uint16_t max_brightness[16];  /* offset 10, size 32 */
    uint16_t ramp_time_ms[16];    /* offset 42, size 32 */
    uint8_t  _pad2[6];            /* offset 74 — pad to 80 bytes (10 doublewords) */
} Config;

Config   config_defaults(void);
Config   config_load(void);
bool     config_save(const Config *cfg);
void     config_apply(const Config *cfg, UART_HandleTypeDef *huart, TIM_HandleTypeDef *htim);
bool     config_validate_reg(uint16_t addr, uint16_t value);
void     config_set_reg(Config *cfg, uint16_t addr, uint16_t value);
uint16_t config_get_reg(const Config *cfg, uint16_t addr);

#endif /* CONFIG_H */
