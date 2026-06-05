#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

TEST_GROUP(modbus_handler_read_holding_registers);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static uint16_t last_addr;
static uint16_t last_count;
static bool     trigger_addr_error;
static bool     trigger_device_failure;

static ModbusExceptionCode mock_read_holding_registers(uint16_t addr, uint16_t count,
                                                       uint8_t *dest)
{
    last_addr  = addr;
    last_count = count;
    if (trigger_device_failure) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (trigger_addr_error)     return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    /* Fill each register with its 1-based sequential index for easy verification */
    for (uint16_t i = 0; i < count; i++)
        modbus_be16_set(&dest[i * 2u], (uint16_t)(i + 1u));
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_handler_read_holding_registers)
{
    setup_slave(&slave, &config);
    config.read_holding_registers = mock_read_holding_registers;
    slave.config                  = config;
    last_addr = last_count        = 0;
    trigger_addr_error            = false;
    trigger_device_failure        = false;
}

TEST_TEAR_DOWN(modbus_handler_read_holding_registers) {}

/* --- valid request -------------------------------------------------------- */

/*
 * Read 2 holding registers starting at address 0x0010.
 * Mock fills register 0 = 0x0001, register 1 = 0x0002.
 * Response: [addr][0x03][byte_count=4][0x00][0x01][0x00][0x02][crc×2]  — 9 bytes.
 */
TEST(modbus_handler_read_holding_registers, test_read_holding_registers_valid)
{
    uint8_t pdu[] = {0x03, 0x00, 0x10, 0x00, 0x02};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(9, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x04,            g_tx_buf[2]); /* byte count = 4 */
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01,            g_tx_buf[4]); /* register 0 = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x02,            g_tx_buf[6]); /* register 1 = 2 */
    TEST_ASSERT_TRUE(response_crc_valid());

    TEST_ASSERT_EQUAL_HEX16(0x0010, last_addr);
    TEST_ASSERT_EQUAL(2, last_count);
}

/* --- frame length validation ---------------------------------------------- */

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_frame_too_short)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x03, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- null callback -------------------------------------------------------- */

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_null_callback)
{
    slave.config.read_holding_registers = NULL;
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x03, MODBUS_EX_ILLEGAL_FUNCTION);
}

/* --- count validation ----------------------------------------------------- */

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_count_zero)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x03, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_count_exceeds_max)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x7E}; /* count = 126 */
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x03, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_count_at_max)
{
    /* count = 125: response = 1+1+1+250+2 = 255 bytes */
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x7D};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x03, g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFA, g_tx_buf[2]); /* byte count = 125 × 2 = 250 */
    TEST_ASSERT_TRUE(response_crc_valid());
    TEST_ASSERT_EQUAL(125, last_count);
}

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_count_at_min)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x03, g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, g_tx_buf[2]); /* byte count = 2 */
    TEST_ASSERT_TRUE(response_crc_valid());
}

/* --- callback exceptions -------------------------------------------------- */

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_callback_addr_error)
{
    trigger_addr_error = true;
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x03, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

TEST(modbus_handler_read_holding_registers, test_read_holding_registers_callback_device_failure)
{
    trigger_device_failure = true;
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x03, MODBUS_EX_SLAVE_DEVICE_FAILURE);
}
