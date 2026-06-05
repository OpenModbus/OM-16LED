#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

TEST_GROUP(modbus_handler_write_single_register);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static uint16_t last_addr;
static uint16_t last_value;
static bool     trigger_addr_error;
static bool     trigger_device_failure;

static ModbusExceptionCode mock_write_single_register(uint16_t addr, uint16_t value)
{
    last_addr  = addr;
    last_value = value;
    if (trigger_device_failure) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (trigger_addr_error)     return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_handler_write_single_register)
{
    setup_slave(&slave, &config);
    config.write_single_register = mock_write_single_register;
    slave.config                 = config;
    last_addr = last_value       = 0;
    trigger_addr_error           = false;
    trigger_device_failure       = false;
}

TEST_TEAR_DOWN(modbus_handler_write_single_register) {}

/* --- valid request -------------------------------------------------------- */

/*
 * Write register 0x0300 = 0x1234.
 * Response echoes the full request PDU:
 *   [addr][0x06][0x03][0x00][0x12][0x34][crc×2]  — 8 bytes total.
 */
TEST(modbus_handler_write_single_register, test_write_single_register_valid)
{
    uint8_t pdu[] = {0x06, 0x03, 0x00, 0x12, 0x34};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(8, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x06,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03,            g_tx_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x12,            g_tx_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0x34,            g_tx_buf[5]);
    TEST_ASSERT_TRUE(response_crc_valid());

    TEST_ASSERT_EQUAL_HEX16(0x0300, last_addr);
    TEST_ASSERT_EQUAL_HEX16(0x1234, last_value);
}

/* Write register 0x0000 = 0xFFFF (max register value). */
TEST(modbus_handler_write_single_register, test_write_single_register_max_value)
{
    uint8_t pdu[] = {0x06, 0x00, 0x00, 0xFF, 0xFF};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(8, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(0xFF, g_tx_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, g_tx_buf[5]);
    TEST_ASSERT_TRUE(response_crc_valid());
}

/* --- frame length validation ---------------------------------------------- */

TEST(modbus_handler_write_single_register, test_write_single_register_frame_too_short)
{
    uint8_t pdu[] = {0x06, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x06, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- null callback -------------------------------------------------------- */

TEST(modbus_handler_write_single_register, test_write_single_register_null_callback)
{
    slave.config.write_single_register = NULL;
    uint8_t pdu[] = {0x06, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x06, MODBUS_EX_ILLEGAL_FUNCTION);
}

/* --- callback exceptions -------------------------------------------------- */

TEST(modbus_handler_write_single_register, test_write_single_register_callback_addr_error)
{
    trigger_addr_error = true;
    uint8_t pdu[] = {0x06, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x06, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

TEST(modbus_handler_write_single_register, test_write_single_register_callback_device_failure)
{
    trigger_device_failure = true;
    uint8_t pdu[] = {0x06, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x06, MODBUS_EX_SLAVE_DEVICE_FAILURE);
}
