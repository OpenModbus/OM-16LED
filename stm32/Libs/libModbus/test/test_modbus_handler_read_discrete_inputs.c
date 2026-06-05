#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

TEST_GROUP(modbus_handler_read_discrete_inputs);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static uint16_t last_addr;
static uint16_t last_count;
static bool     trigger_addr_error;
static bool     trigger_device_failure;

static ModbusExceptionCode mock_read_discrete_inputs(uint16_t addr, uint16_t count,
                                                     uint8_t *dest)
{
    last_addr  = addr;
    last_count = count;
    if (trigger_device_failure) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (trigger_addr_error)     return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    memset(dest, 0xAB, (count + 7u) / 8u);
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_handler_read_discrete_inputs)
{
    setup_slave(&slave, &config);
    config.read_discrete_inputs = mock_read_discrete_inputs;
    slave.config                = config;
    last_addr = last_count      = 0;
    trigger_addr_error          = false;
    trigger_device_failure      = false;
}

TEST_TEAR_DOWN(modbus_handler_read_discrete_inputs) {}

/* --- valid request -------------------------------------------------------- */

/*
 * Read 16 discrete inputs (0x0010) starting at address 0x0100.
 * Response: [addr][0x02][byte_count=2][0xAB][0xAB][crc×2]  — 7 bytes total.
 */
TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_valid)
{
    uint8_t pdu[] = {0x02, 0x01, 0x00, 0x00, 0x10};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(7, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02,            g_tx_buf[2]); /* byte count */
    TEST_ASSERT_EQUAL_HEX8(0xAB,            g_tx_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0xAB,            g_tx_buf[4]);
    TEST_ASSERT_TRUE(response_crc_valid());

    TEST_ASSERT_EQUAL_HEX16(0x0100, last_addr);
    TEST_ASSERT_EQUAL(16, last_count);
}

/* --- frame length validation ---------------------------------------------- */

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_frame_too_short)
{
    uint8_t pdu[] = {0x02, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x02, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- null callback -------------------------------------------------------- */

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_null_callback)
{
    slave.config.read_discrete_inputs = NULL;
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x02, MODBUS_EX_ILLEGAL_FUNCTION);
}

/* --- count validation ----------------------------------------------------- */

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_count_zero)
{
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x02, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_count_exceeds_max)
{
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x07, 0xD1}; /* count = 2001 */
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x02, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_count_at_max)
{
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x07, 0xD0}; /* count = 2000 */
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x02, g_tx_buf[1]);
    TEST_ASSERT_TRUE(response_crc_valid());
    TEST_ASSERT_EQUAL(2000, last_count);
}

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_count_at_min)
{
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x00, 0x01}; /* count = 1 */
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x02, g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, g_tx_buf[2]); /* byte count = 1 */
    TEST_ASSERT_TRUE(response_crc_valid());
}

/* --- callback exceptions -------------------------------------------------- */

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_callback_addr_error)
{
    trigger_addr_error = true;
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x02, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

TEST(modbus_handler_read_discrete_inputs, test_read_discrete_inputs_callback_device_failure)
{
    trigger_device_failure = true;
    uint8_t pdu[] = {0x02, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x02, MODBUS_EX_SLAVE_DEVICE_FAILURE);
}
