#include "unity_fixture.h"
#include "modbus_slave.h"
#include "test_helpers.h"

#include <string.h>

TEST_GROUP(modbus_integration);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

/* Tracks whether the write callback was invoked */
static bool write_cb_called;

static ModbusExceptionCode mock_read_holding_registers(uint16_t addr, uint16_t count,
                                                       uint8_t *dest)
{
    if (addr > 1000) return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    for (uint16_t i = 0; i < count; i++)
        modbus_be16_set(&dest[i * 2u], (uint16_t)(500u + addr + i));
    return MODBUS_EX_NONE;
}

static ModbusExceptionCode mock_write_single_register(uint16_t addr, uint16_t value)
{
    (void)addr; (void)value;
    write_cb_called = true;
    return MODBUS_EX_NONE;
}

TEST_SETUP(modbus_integration)
{
    setup_slave(&slave, &config);
    config.read_holding_registers = mock_read_holding_registers;
    config.write_single_register  = mock_write_single_register;
    slave.config                  = config;
    write_cb_called               = false;
}

TEST_TEAR_DOWN(modbus_integration) {}

/* --- core pipeline -------------------------------------------------------- */

/*
 * Valid read holding registers request:
 *   addr=1, FC=0x03, start=0, count=2
 * Response should be addr + FC + byte_count(4) + reg0(500) + reg1(501) + CRC.
 */
TEST(modbus_integration, test_complete_frame_processing)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x02};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));

    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(TEST_SLAVE_ADDR, g_tx_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03,            g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x04,            g_tx_buf[2]); /* byte count */
    TEST_ASSERT_EQUAL_HEX16(500, modbus_be16_get(&g_tx_buf[3]));
    TEST_ASSERT_EQUAL_HEX16(501, modbus_be16_get(&g_tx_buf[5]));
    TEST_ASSERT_TRUE(response_crc_valid());
}

TEST(modbus_integration, test_response_crc_valid)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_TRUE_MESSAGE(response_crc_valid(), "CRC in transmitted response is invalid");
}

/* --- frame rejection ------------------------------------------------------ */

TEST(modbus_integration, test_frame_invalid_crc)
{
    /* Build an otherwise valid frame but corrupt the CRC */
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x12, 0x34};

    helpers_reset_tx();
    for (int i = 0; i < 8; i++)
        modbus_slave_rx_byte(&slave, frame[i]);
    modbus_slave_1_5t_elapsed(&slave);
    modbus_slave_3_5t_elapsed(&slave);
    modbus_slave_poll(&slave);

    TEST_ASSERT_FALSE_MESSAGE(g_tx_called, "corrupted frame must not generate a response");
}

TEST(modbus_integration, test_wrong_address_frame)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, 0x02, pdu, sizeof(pdu)); /* wrong address */
    TEST_ASSERT_FALSE_MESSAGE(g_tx_called, "frame for a different address must be ignored");
}

/* --- broadcast ------------------------------------------------------------ */

TEST(modbus_integration, test_broadcast_read_ignored)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01}; /* read holding registers */
    send_request(&slave, 0x00, pdu, sizeof(pdu));    /* broadcast address */
    TEST_ASSERT_FALSE_MESSAGE(g_tx_called, "broadcast read must not generate a response");
}

TEST(modbus_integration, test_broadcast_write_executes_no_response)
{
    uint8_t pdu[] = {0x06, 0x00, 0x00, 0x00, 0x01}; /* write single register */
    send_request(&slave, 0x00, pdu, sizeof(pdu));

    /* Write callback must have been invoked */
    TEST_ASSERT_TRUE_MESSAGE(write_cb_called, "broadcast write must execute the callback");
    /* But no response must be transmitted */
    TEST_ASSERT_FALSE_MESSAGE(g_tx_called, "broadcast frame must not generate a response");
}

/* --- exception handling --------------------------------------------------- */

TEST(modbus_integration, test_unknown_function_code)
{
    /* FC 0x41 is not implemented */
    uint8_t pdu[] = {0x41, 0x00, 0x00};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    assert_exception_response(0x41, MODBUS_EX_ILLEGAL_FUNCTION);
}

TEST(modbus_integration, test_exception_response_crc_valid)
{
    uint8_t pdu[] = {0x41, 0x00, 0x00}; /* unknown FC → exception */
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_TRUE_MESSAGE(response_crc_valid(), "exception response CRC is invalid");
}

/* --- state reset after poll ----------------------------------------------- */

TEST(modbus_integration, test_frame_len_reset_after_poll)
{
    uint8_t pdu[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu, sizeof(pdu));
    TEST_ASSERT_EQUAL_MESSAGE(0, slave.frame_len,
        "frame_len must be 0 after poll() so the next frame starts clean");
}

/* --- consecutive frames --------------------------------------------------- */

TEST(modbus_integration, test_consecutive_frames)
{
    /* First frame */
    uint8_t pdu1[] = {0x03, 0x00, 0x00, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu1, sizeof(pdu1));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_TRUE(response_crc_valid());

    /* Second frame — slave must be ready to receive it */
    uint8_t pdu2[] = {0x03, 0x00, 0x01, 0x00, 0x01};
    send_request(&slave, TEST_SLAVE_ADDR, pdu2, sizeof(pdu2));
    TEST_ASSERT_TRUE(g_tx_called);
    TEST_ASSERT_EQUAL_HEX8(0x03, g_tx_buf[1]);
    TEST_ASSERT_EQUAL_HEX16(501, modbus_be16_get(&g_tx_buf[3])); /* addr 1 → value 501 */
    TEST_ASSERT_TRUE(response_crc_valid());
}
