#ifndef DEK_CRC_H
#define DEK_CRC_H

#include <stdint.h>
#include <stddef.h>

#define DEK_CRC16_INITIAL_VALUE  0xFFFF
#define DEK_CRC16_POLYNOMIAL     0x1021
#define DEK_CRC16_MSB_MASK       0x8000

uint16_t dek_crc16(const uint8_t *data, uint16_t length);

#endif 