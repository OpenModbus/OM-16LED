#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

/*
 * Shared test utilities for libModbus test suite.
 *
 * All functions and variables are static so each translation unit gets
 * its own copy — no linkage conflicts across test files.
 *
 * Usage:
 *   1. Call setup_slave() in TEST_SETUP to initialise the slave and reset
 *      the transmit capture state.
 *   2. Call send_request() to push a full Modbus PDU through the pipeline
 *      (CRC is appended automatically).
 *   3. Assert on g_tx_buf / g_tx_len / g_tx_called, or use the helpers
 *      response_crc_valid() and assert_exception_response().
 */

#include "unity_fixture.h"
#include "modbus_slave.h"
#include "modbus_crc16.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define TEST_SLAVE_ADDR 0x01

/* Captured transmit state ------------------------------------------------- */

static uint8_t  g_tx_buf[256];
static uint16_t g_tx_len;
static bool     g_tx_called;

static void mock_write(const uint8_t *data, uint16_t length)
{
    memcpy(g_tx_buf, data, length);
    g_tx_len    = length;
    g_tx_called = true;
}

static void helpers_reset_tx(void)
{
    memset(g_tx_buf, 0, sizeof(g_tx_buf));
    g_tx_len    = 0;
    g_tx_called = false;
}

/* Slave initialisation ----------------------------------------------------- */

static void setup_slave(ModbusSlave *slave, ModbusSlaveConfig *config)
{
    memset(slave,  0, sizeof(*slave));
    memset(config, 0, sizeof(*config));
    config->address = TEST_SLAVE_ADDR;
    config->write   = mock_write;
    helpers_reset_tx();
    modbus_slave_init(slave, config);
}

/* Full-pipeline request helper --------------------------------------------- */

/*
 * Build a complete Modbus RTU frame from a raw PDU (FC + data, no address,
 * no CRC), feed it byte-by-byte through the slave, fire both timer callbacks,
 * then call poll().  The transmit capture is reset before each call.
 *
 *   addr    — destination address byte (use TEST_SLAVE_ADDR for normal requests)
 *   pdu     — function code followed by request data
 *   pdu_len — number of bytes in pdu
 */
static void send_request(ModbusSlave *slave, uint8_t addr,
                         const uint8_t *pdu, uint16_t pdu_len)
{
    uint8_t  frame[256];
    uint16_t frame_len = (uint16_t)(1u + pdu_len);

    frame[0] = addr;
    memcpy(&frame[1], pdu, pdu_len);

    uint16_t crc       = modbus_crc16(frame, frame_len);
    frame[frame_len]   = (uint8_t)(crc & 0xFFu);
    frame[frame_len+1] = (uint8_t)(crc >> 8);
    frame_len         += 2u;

    helpers_reset_tx();

    for (uint16_t i = 0; i < frame_len; i++)
        modbus_slave_rx_byte(slave, frame[i]);

    modbus_slave_1_5t_elapsed(slave);
    modbus_slave_3_5t_elapsed(slave);
    modbus_slave_poll(slave);
}

/* Response assertions ------------------------------------------------------ */

/* Returns true when the CRC appended to the last captured response is valid. */
static bool response_crc_valid(void)
{
    if (g_tx_len < 4u) return false;
    uint16_t received = modbus_le16_get(&g_tx_buf[g_tx_len - 2]);
    uint16_t expected = modbus_crc16(g_tx_buf, (uint16_t)(g_tx_len - 2));
    return received == expected;
}

/*
 * Assert the captured response is a well-formed Modbus exception:
 *   [TEST_SLAVE_ADDR][fc | 0x80][ex_code][crc_lo][crc_hi]  (5 bytes total)
 */
static void assert_exception_response(uint8_t fc, uint8_t ex_code)
{
    TEST_ASSERT_TRUE_MESSAGE(g_tx_called,       "expected a response to be transmitted");
    TEST_ASSERT_EQUAL_MESSAGE(5, g_tx_len,      "exception response must be 5 bytes");
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR,     g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(fc | 0x80u,          g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(ex_code,             g_tx_buf[2]);
    TEST_ASSERT_TRUE_MESSAGE(response_crc_valid(), "exception response CRC is invalid");
}

#endif /* TEST_HELPERS_H */
