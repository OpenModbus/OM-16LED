#include "modbus_slave.h"
#include "modbus_bytes.h"
#include "modbus_crc16.h"

#include <string.h>

#include "modbus_slave_handlers.h"

// =============================================================================
// Initialization
// =============================================================================

/**
 * Initialize Modbus slave instance
 * @param slave Pointer to slave instance
 * @param cfg   Pointer to configuration
 * @return 0 on success, -1 on error
 */
int modbus_slave_init(ModbusSlave *slave, const ModbusSlaveConfig *cfg) {
    if (!slave || !cfg || !cfg->write) return -1;

    if (cfg->address == 0x00 || cfg->address > 0xF7) return -1; // 0x00 broadcast, 0xF8-0xFF reserved

    slave->config = *cfg;
    slave->state = IDLE;
    slave->frame_len = 0;
    slave->frame_ok = true;
    slave->frame_available = false;

#ifdef MODBUS_DOUBLE_BUFFER
    slave->frame      = slave->_frames[0];
    slave->_rx_frame  = slave->_frames[1];
    slave->_rx_frame_len = 0;
    slave->_rx_frame_ok  = true;
#else
    slave->processing_frame = false;
#endif

    return 0;
}

// =============================================================================
// Receive byte (ISR-safe)
// =============================================================================

/**
 * Process received byte - call from UART ISR
 * @param slave Slave instance
 * @param byte  Received byte
 */
void modbus_slave_rx_byte(ModbusSlave *slave, uint8_t byte) {
#ifndef MODBUS_DOUBLE_BUFFER
    if (slave->processing_frame) return;
#endif

    if (slave->state == IDLE) {
        slave->state = RECEPTION;
#ifdef MODBUS_DOUBLE_BUFFER
        slave->_rx_frame_len = 0;
        slave->_rx_frame_ok  = true;
#else
        slave->frame_len = 0;
        slave->frame_ok  = true;
#endif
    }

    if (slave->state == RECEPTION) {
#ifdef MODBUS_DOUBLE_BUFFER
        if (slave->_rx_frame_len < MODBUS_MAX_FRAME_LENGTH) {
            slave->_rx_frame[slave->_rx_frame_len++] = byte;
        } else {
            slave->_rx_frame_ok = false;
            slave->state = CONTROL_AND_WAITING;
        }
#else
        if (slave->frame_len < MODBUS_MAX_FRAME_LENGTH) {
            slave->frame[slave->frame_len++] = byte;
        } else {
            slave->frame_ok = false;
            slave->state = CONTROL_AND_WAITING;
        }
#endif
    }
}

// =============================================================================
// Frame timeout (call from timer ISR)
//
// First call (in RECEPTION)      — end of 1.5-character-time gap
// Second call (in C&W)           — end of 3.5-character-time silence
// =============================================================================

/**
 * 1.5 character time elapsed - end of character reception
 * @param slave Slave instance
 */
void modbus_slave_1_5t_elapsed(ModbusSlave *slave) {
    if (slave->state == RECEPTION) slave->state = CONTROL_AND_WAITING;
}

/**
 * 3.5 character time elapsed - end of frame
 * @param slave Slave instance
 */
void modbus_slave_3_5t_elapsed(ModbusSlave *slave) {
    if (slave->state != CONTROL_AND_WAITING) return;

#ifdef MODBUS_DOUBLE_BUFFER
    /* Snapshot rx metadata, reset rx side, then swap buffers */
    slave->frame_len     = slave->_rx_frame_len;
    slave->frame_ok      = slave->_rx_frame_ok;
    slave->_rx_frame_len = 0;
    slave->_rx_frame_ok  = true;
    uint8_t *tmp      = slave->frame;
    slave->frame      = slave->_rx_frame;
    slave->_rx_frame  = tmp;
#endif

    if (slave->frame_ok) slave->frame_available = true;
    slave->state = IDLE;
}

// =============================================================================
// Frame validation
// =============================================================================

static int modbus_validate_frame(ModbusSlave *slave) {
    if (slave->frame_len < MODBUS_MIN_FRAME_LENGTH) return -1;

    uint8_t address = slave->frame[0];
    if (address != 0x00 && address != slave->config.address) return -1;

    uint16_t received_crc = modbus_le16_get(&slave->frame[slave->frame_len - 2]);
    uint16_t expected_crc = modbus_crc16(slave->frame, slave->frame_len - 2);
    if (received_crc != expected_crc) return -1;

    return 0;
}

// =============================================================================
// Frame processor
// =============================================================================

static bool is_read_fc(uint8_t fc) {
    return fc == MODBUS_FC_READ_COILS              ||
           fc == MODBUS_FC_READ_DISCRETE_INPUTS    ||
           fc == MODBUS_FC_READ_HOLDING_REGISTERS  ||
           fc == MODBUS_FC_READ_INPUT_REGISTERS    ||
           fc == MODBUS_FC_READ_WRITE_MULTIPLE_REGS;
}

static void modbus_process_frame(ModbusSlave *slave) {
    if (modbus_validate_frame(slave) != 0) return;

    uint8_t *request = slave->frame;

    /* Silently discard read function codes sent to the broadcast address */
    if (request[0] == 0x00 && is_read_fc(request[1])) return;

    uint8_t  *response     = slave->response;
    uint8_t  *response_pdu = response + 1;
    uint16_t  response_len = 0;

    ModbusExceptionCode ex_code = MODBUS_EX_NONE;

    switch (request[1]) {
        case MODBUS_FC_READ_COILS:
            ex_code = handle_read_coils(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_READ_DISCRETE_INPUTS:
            ex_code = handle_read_discrete_inputs(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            ex_code = handle_read_holding_registers(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_READ_INPUT_REGISTERS:
            ex_code = handle_read_input_registers(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_WRITE_SINGLE_COIL:
            ex_code = handle_write_single_coil(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
            ex_code = handle_write_single_register(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
            ex_code = handle_write_multiple_coils(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
            ex_code = handle_write_multiple_registers(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_MASK_WRITE_REGISTER:
            ex_code = handle_mask_write_register(slave, response_pdu, &response_len);
            break;
        case MODBUS_FC_READ_WRITE_MULTIPLE_REGS:
            ex_code = handle_read_write_multiple_registers(slave, response_pdu, &response_len);
            break;
        default:
            ex_code = MODBUS_EX_ILLEGAL_FUNCTION;
            break;
    }

    if (request[0] == 0x00) return; /* broadcast — no response */

    response[0] = request[0];
    response_len += 1;

    if (ex_code != MODBUS_EX_NONE) {
        response[1] = request[1] | MODBUS_FC_EXCEPTION_MASK;
        response[2] = (uint8_t)ex_code;
        response_len = 3;
    }

    uint16_t crc = modbus_crc16(response, response_len);
    modbus_le16_set(&response[response_len], crc);
    response_len += 2;

    slave->config.write(response, response_len);
}

// =============================================================================
// Polling (call from main loop)
// =============================================================================

/**
 * Process received frames - call periodically from main loop
 * @param slave Slave instance
 */
void modbus_slave_poll(ModbusSlave *slave) {
#ifndef MODBUS_DOUBLE_BUFFER
    slave->processing_frame = true;
#endif
    if (!slave->frame_available) {
#ifndef MODBUS_DOUBLE_BUFFER
        slave->processing_frame = false;
#endif
        return;
    }

    slave->frame_available = false;

    modbus_process_frame(slave);

    slave->frame_len = 0;
#ifndef MODBUS_DOUBLE_BUFFER
    slave->processing_frame = false;
#endif
}
