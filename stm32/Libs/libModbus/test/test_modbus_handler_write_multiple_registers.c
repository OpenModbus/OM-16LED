#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

#include <string.h>

TEST_GROUP(modbus_handler_write_multiple_registers);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static uint16_t       last_addr;
static uint16_t       last_count;
static const uint8_t *last_data;
static bool           trigger_addr_error;
static bool           trigger_device_failure;

static ModbusExceptionCode mock_write_multiple_registers(uint16_t addr, uint16_t count,
                                                         const uint8_t *src)
{
    last_addr  = addr;
    last_count = count;
    last_data  = src;
    if (trigger_device_failure) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (trigger_addr_error)     return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_handler_write_multiple_registers)
{
    setup_slave(&slave, &config);
    config.write_multiple_registers = mock_write_multiple_registers;
    slave.config                    = config;
    last_addr = last_count          = 0;
    last_data                       = NULL;
    trigger_addr_error              = false;
    trigger_device_failure          = false;
}

TEST_TEAR_DOWN(modbus_handler_write_multiple_registers) {}

/* --- valid request -------------------------------------------------------- */

/*
 * Write 2 registers at address 0x0064 with values 0x1234, 0x5678.
 * Response echoes start address and quantity:
 *   [addr][0x10][0x00][0x64][0x00][0x02][crc×2]  — 8 bytes total.
 */
TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_valid)
{
    uint8_t pdu[] = {0x10, 0x00, 0x64, 0x00, 0x02, 0x04, 0x12, 0x34, 0x56, 0x78};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(8, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x64,            g_tx_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x02,            g_tx_buf[5]);
    TEST_ASSERT_TRUE(response_crc_valid());

    TEST_ASSERT_EQUAL_HEX16(0x0064, last_addr);
    TEST_ASSERT_EQUAL(2, last_count);
    TEST_ASSERT_NOT_NULL(last_data);
    TEST_ASSERT_EQUAL_HEX16(0x1234, modbus_be16_get(&last_data[0]));
    TEST_ASSERT_EQUAL_HEX16(0x5678, modbus_be16_get(&last_data[2]));
}

/* --- frame length validation ---------------------------------------------- */

/* PDU has FC + start(2) + count(2) only — missing byte_count and data */
TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_frame_too_short)
{
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- null callback -------------------------------------------------------- */

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_null_callback)
{
    slave.config.write_multiple_registers = NULL;
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_ILLEGAL_FUNCTION);
}

/* --- count validation ----------------------------------------------------- */

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_count_zero)
{
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_count_exceeds_max)
{
    /* count = 124 (0x007C), max is 123 (0x007B) */
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x7C, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_count_at_max)
{
    /*
     * count = 123 (0x007B), byte_count = 246 (0xF6).
     * Frame = 1+1+2+2+1+246+2 = 255 bytes — fits in 256-byte buffer.
     */
    uint8_t pdu[252];
    pdu[0] = 0x10;
    pdu[1] = 0x00; pdu[2] = 0x00;  /* start addr = 0 */
    pdu[3] = 0x00; pdu[4] = 0x7B;  /* count = 123 */
    pdu[5] = 0xF6;                  /* byte_count = 246 */
    memset(&pdu[6], 0x00, 246);
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x10, g_tx_buf[1]);
    TEST_ASSERT_TRUE(response_crc_valid());
    TEST_ASSERT_EQUAL(123, last_count);
}

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_invalid_byte_count)
{
    /* 2 registers require 4 bytes, but byte_count = 3 */
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x02, 0x03, 0x12, 0x34, 0x56};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- callback exceptions -------------------------------------------------- */

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_callback_addr_error)
{
    trigger_addr_error = true;
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

TEST(modbus_handler_write_multiple_registers, test_write_multiple_registers_callback_device_failure)
{
    trigger_device_failure = true;
    uint8_t pdu[] = {0x10, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x10, MODBUS_EX_SLAVE_DEVICE_FAILURE);
}
