#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

TEST_GROUP(modbus_handler_mask_write_register);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static uint16_t last_addr;
static uint16_t last_and_mask;
static uint16_t last_or_mask;
static bool     trigger_addr_error;
static bool     trigger_device_failure;

static ModbusExceptionCode mock_mask_write_register(uint16_t addr, uint16_t and_mask,
                                                    uint16_t or_mask)
{
    last_addr     = addr;
    last_and_mask = and_mask;
    last_or_mask  = or_mask;
    if (trigger_device_failure) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (trigger_addr_error)     return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_handler_mask_write_register)
{
    setup_slave(&slave, &config);
    config.mask_write_register   = mock_mask_write_register;
    slave.config                 = config;
    last_addr = last_and_mask    = 0;
    last_or_mask                 = 0;
    trigger_addr_error           = false;
    trigger_device_failure       = false;
}

TEST_TEAR_DOWN(modbus_handler_mask_write_register) {}

/* --- valid request -------------------------------------------------------- */

/*
 * Mask write register 0x0004 with AND=0x00F2, OR=0x0025.
 * Response echoes the full request PDU:
 *   [addr][0x16][0x00][0x04][0x00][0xF2][0x00][0x25][crc×2]  — 10 bytes total.
 */
TEST(modbus_handler_mask_write_register, test_mask_write_register_valid)
{
    uint8_t pdu[] = {0x16, 0x00, 0x04, 0x00, 0xF2, 0x00, 0x25};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(10, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x16,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x04,            g_tx_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0xF2,            g_tx_buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00,            g_tx_buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x25,            g_tx_buf[7]);
    TEST_ASSERT_TRUE(response_crc_valid());

    TEST_ASSERT_EQUAL_HEX16(0x0004, last_addr);
    TEST_ASSERT_EQUAL_HEX16(0x00F2, last_and_mask);
    TEST_ASSERT_EQUAL_HEX16(0x0025, last_or_mask);
}

/* AND=0xFFFF (no bits cleared), OR=0x0000 (no bits set) — identity operation. */
TEST(modbus_handler_mask_write_register, test_mask_write_register_zero_masks)
{
    uint8_t pdu[] = {0x16, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL(10, g_tx_len);
    TEST_ASSERT_EQUAL_HEX8(0xFF, g_tx_buf[4]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, g_tx_buf[5]);
    TEST_ASSERT_EQUAL_HEX8(0x00, g_tx_buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x00, g_tx_buf[7]);
    TEST_ASSERT_TRUE(response_crc_valid());
}

/* --- frame length validation ---------------------------------------------- */

/* PDU has FC + reg_addr(2) + and_mask(2) only — missing or_mask */
TEST(modbus_handler_mask_write_register, test_mask_write_register_frame_too_short)
{
    uint8_t pdu[] = {0x16, 0x00, 0x00, 0xFF, 0xFF};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x16, MODBUS_EX_ILLEGAL_DATA_VALUE);
}

/* --- null callback -------------------------------------------------------- */

TEST(modbus_handler_mask_write_register, test_mask_write_register_null_callback)
{
    slave.config.mask_write_register = NULL;
    uint8_t pdu[] = {0x16, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x16, MODBUS_EX_ILLEGAL_FUNCTION);
}

/* --- callback exceptions -------------------------------------------------- */

TEST(modbus_handler_mask_write_register, test_mask_write_register_callback_addr_error)
{
    trigger_addr_error = true;
    uint8_t pdu[] = {0x16, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x16, MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

TEST(modbus_handler_mask_write_register, test_mask_write_register_callback_device_failure)
{
    trigger_device_failure = true;
    uint8_t pdu[] = {0x16, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x16, MODBUS_EX_SLAVE_DEVICE_FAILURE);
}
