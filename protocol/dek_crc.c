#include "dek_crc.h"

uint16_t dek_crc16(const uint8_t *data, uint16_t length) {
    // init crc
    uint16_t crc = DEK_CRC16_INITIAL_VALUE;
    
    if (data == NULL || length == 0) {
        return crc; // Return initial value for empty data
    }

    // For each byte in data, update the crc
    for (uint16_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8); // XOR byte into upper half

        for (int j = 0; j < 8; j++) { // For each bit
            if (crc & DEK_CRC16_MSB_MASK) { // If the uppermost bit is set
                crc = (crc << 1) ^ DEK_CRC16_POLYNOMIAL; // Shift left and XOR with polynomial
            } else {
                crc <<= 1; // Just shift left
            }
        }
    }

    return crc;
}
