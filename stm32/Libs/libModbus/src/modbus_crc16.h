#ifndef MODBUS_CRC16_H
#define MODBUS_CRC16_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint16_t modbus_crc16_table[256];
uint16_t modbus_crc16(const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CRC16_H */
