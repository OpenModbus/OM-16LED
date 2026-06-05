#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

TEST_GROUP(modbus_handler_read_write_multiple_registers);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static uint16_t       last_read_addr;
static uint16_t       last_read_count;
static uint16_t       last_write_addr;
static uint16_t       last_write_count;
static const uint8_t *last_write_data;
static bool           trigger_addr_error;
static bool           trigger_device_failure;

static ModbusExceptionCode mock_read_write_multiple_registers(
    uint16_t read_addr,  uint16_t read_count,
    uint16_t write_addr, uint16_t write_count,
    const uint8_t *write_data, uint8_t *read_data)
{
    last_read_addr  = read_addr;
    last_read_count = read_count;
    last_write_addr = write_addr;
    last_write_count = write_count;
    last_write_data = write_data;
    if (trigger_device_failure) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (trigger_addr_error)     return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    for (uint16_t i = 0; i < read_count; i++)
        modbus_be16_set(&read_data[i * 2u], (uint16_t)(i + 1u));
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_handler_read_write_multiple_registers)
{
    setup_slave(&slave, &config);
    config.read_write_multiple_registers = mock_read_write_multiple_registers;
    slave.config                         = config;
    last_read_addr = last_read_count     = 0;
    last_write_addr = last_write_count   = 0;
    last_write_data                      = NULL;
    trigger_addr_error                   = false;
    trigger_device_failure               = false;
}

TEST_TEAR_DOWN(modbus_handler_read_write_multiple_registers) {}

/* --- valid request -------------------------------------------------------- */

/*
 * Read 2 registers from 0x0000; write 1 register to 0x0010 = 0x1234.
 * Response: [addr][0x17][byte_count=4][0x00][0x01][0x00][0x02][crc×2]  — 9 bytes.
 */
TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_valid)
{
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x02,   /* read addr=0, count=2 */
        0x00, 0x10, 0x00, 0x01,   /* write addr=0x0010, count=1 */
        0x02,                      /* write byte count = 2 */
        0x12, 0x34                 /* write data */
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(9, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x17,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x04,            g_tx_buf[2]); /* byte count = 4 */
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01,            g_tx_buf[4]); /* register 0 = 1 */
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x02,            g_tx_buf[6]); /* register 1 = 2 */
    TEST_ASSERT_TRUE(response_crc_valid());

    TEST_ASSERT_EQUAL(0x0000, last_read_addr);
    TEST_ASSERT_EQUAL(2,      last_read_count);
    TEST_ASSERT_EQUAL(0x0010, last_write_addr);
    TEST_ASSERT_EQUAL(1,      last_write_count);
    TEST_ASSERT_NOT_NULL(last_write_data);
    TEST_ASSERT_EQUAL_HEX16(0x1234, modbus_be16_get(last_write_data));
}

/* --- frame length validation ---------------------------------------------- */

/* PDU has FC + read_addr(2) + read_count(2) + write_addr(2) + write_count(2)
 * only — missing byte_count and write data */
TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_frame_too_short)
{
    uint8_t pdu[] = {0x17, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- null callback -------------------------------------------------------- */

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_null_callback)
{
    slave.config.read_write_multiple_registers = NULL;
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x01,
        0x02, 0x00, 0x01
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_FUNCTION);
}

/* --- read count validation ------------------------------------------------ */

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_read_count_zero)
{
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x00,   /* read count = 0 */
        0x00, 0x00, 0x00, 0x01,
        0x02, 0x00, 0x01
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_read_count_exceeds_max)
{
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x7E,   /* read count = 126, max is 125 */
        0x00, 0x00, 0x00, 0x01,
        0x02, 0x00, 0x01
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- write count validation ----------------------------------------------- */

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_write_count_zero)
{
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00,   /* write count = 0 */
        0x00
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_write_count_exceeds_max)
{
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x7A,   /* write count = 122, max is 121 */
        0xF4, 0x00                 /* byte_count = 244 */
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- byte count validation ------------------------------------------------ */

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_invalid_byte_count)
{
    /* write count = 2 requires 4 bytes, but byte_count = 3 */
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x02,
        0x03, 0x12, 0x34, 0x56    /* byte_count = 3 instead of 4 */
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- callback exceptions -------------------------------------------------- */

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_callback_addr_error)
{
    trigger_addr_error = true;
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x01,
        0x02, 0x00, 0x01
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

TEST(modbus_handler_read_write_multiple_registers, test_read_write_multiple_registers_callback_device_failure)
{
    trigger_device_failure = true;
    uint8_t pdu[] = {
        0x17,
        0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x01,
        0x02, 0x00, 0x01
    };
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x17, MODBUS_EX_SLAVE_DEVICE_FAILURE);
}
