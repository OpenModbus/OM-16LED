#include "unity_fixture.h"
#include "modbus_slave.h"

#include <string.h>

TEST_GROUP(modbus_slave_rx);

static ModbusSlave slave;
static ModbusSlaveConfig config;

static void mock_write(const uint8_t *data, uint16_t length) {
    (void)(data);
    (void)(length);
}

TEST_SETUP(modbus_slave_rx) {
    memset(&slave, 0, sizeof(slave));
    memset(&config, 0, sizeof(config));
    config.address = 0x01;
    config.write = mock_write;
    modbus_slave_init(&slave, &config);
}

TEST_TEAR_DOWN(modbus_slave_rx) {}

/* --- receive path --------------------------------------------------------- */

TEST(modbus_slave_rx, test_rx_first_byte_transition) {
    TEST_ASSERT_EQUAL(IDLE, slave.state);

    modbus_slave_rx_byte(&slave, 0x01);

    TEST_ASSERT_EQUAL(RECEPTION, slave.state);
#ifdef MODBUS_DOUBLE_BUFFER
    TEST_ASSERT_EQUAL(1, slave._rx_frame_len);
    TEST_ASSERT_EQUAL(0x01, slave._rx_frame[0]);
    TEST_ASSERT_TRUE(slave._rx_frame_ok);
#else
    TEST_ASSERT_EQUAL(1, slave.frame_len);
    TEST_ASSERT_EQUAL(0x01, slave.frame[0]);
    TEST_ASSERT_TRUE(slave.frame_ok);
#endif
}

TEST(modbus_slave_rx, test_rx_multiple_bytes) {
    modbus_slave_rx_byte(&slave, 0x01);
    modbus_slave_rx_byte(&slave, 0x03);
    modbus_slave_rx_byte(&slave, 0x00);
    modbus_slave_rx_byte(&slave, 0x00);

    TEST_ASSERT_EQUAL(RECEPTION, slave.state);
#ifdef MODBUS_DOUBLE_BUFFER
    TEST_ASSERT_EQUAL(4, slave._rx_frame_len);
    TEST_ASSERT_EQUAL(0x01, slave._rx_frame[0]);
    TEST_ASSERT_EQUAL(0x03, slave._rx_frame[1]);
    TEST_ASSERT_EQUAL(0x00, slave._rx_frame[2]);
    TEST_ASSERT_EQUAL(0x00, slave._rx_frame[3]);
#else
    TEST_ASSERT_EQUAL(4, slave.frame_len);
    TEST_ASSERT_EQUAL(0x01, slave.frame[0]);
    TEST_ASSERT_EQUAL(0x03, slave.frame[1]);
    TEST_ASSERT_EQUAL(0x00, slave.frame[2]);
    TEST_ASSERT_EQUAL(0x00, slave.frame[3]);
#endif
}

TEST(modbus_slave_rx, test_rx_frame_overflow) {
    for (int i = 0; i < MODBUS_MAX_FRAME_LENGTH; i++)
        modbus_slave_rx_byte(&slave, i & 0xFF);

#ifdef MODBUS_DOUBLE_BUFFER
    TEST_ASSERT_EQUAL(MODBUS_MAX_FRAME_LENGTH, slave._rx_frame_len);
    TEST_ASSERT_TRUE(slave._rx_frame_ok);
#else
    TEST_ASSERT_EQUAL(MODBUS_MAX_FRAME_LENGTH, slave.frame_len);
    TEST_ASSERT_TRUE(slave.frame_ok);
#endif

    modbus_slave_rx_byte(&slave, 0xFF);

#ifdef MODBUS_DOUBLE_BUFFER
    TEST_ASSERT_FALSE(slave._rx_frame_ok);
#else
    TEST_ASSERT_FALSE(slave.frame_ok);
#endif
    TEST_ASSERT_EQUAL(CONTROL_AND_WAITING, slave.state);
}

#ifndef MODBUS_DOUBLE_BUFFER
TEST(modbus_slave_rx, test_rx_ignore_during_processing) {
    slave.processing_frame = true;

    modbus_slave_rx_byte(&slave, 0x01);

    TEST_ASSERT_EQUAL(IDLE, slave.state);
    TEST_ASSERT_EQUAL(0, slave.frame_len);
}
#endif

/* --- 1.5t timer ----------------------------------------------------------- */

TEST(modbus_slave_rx, test_1_5t_timer_transition) {
    modbus_slave_rx_byte(&slave, 0x01);
    TEST_ASSERT_EQUAL(RECEPTION, slave.state);

    modbus_slave_1_5t_elapsed(&slave);

    TEST_ASSERT_EQUAL(CONTROL_AND_WAITING, slave.state);
}

TEST(modbus_slave_rx, test_1_5t_timer_ignore_other_states) {
    slave.state = IDLE;
    modbus_slave_1_5t_elapsed(&slave);
    TEST_ASSERT_EQUAL(IDLE, slave.state);

    slave.state = CONTROL_AND_WAITING;
    modbus_slave_1_5t_elapsed(&slave);
    TEST_ASSERT_EQUAL(CONTROL_AND_WAITING, slave.state);
}

/* --- 3.5t timer ----------------------------------------------------------- */

TEST(modbus_slave_rx, test_3_5t_timer_valid_frame) {
    slave.state    = CONTROL_AND_WAITING;
    slave.frame_ok = true;

    modbus_slave_3_5t_elapsed(&slave);

    TEST_ASSERT_EQUAL(IDLE, slave.state);
    TEST_ASSERT_TRUE(slave.frame_available);
}

TEST(modbus_slave_rx, test_3_5t_timer_invalid_frame) {
    slave.state = CONTROL_AND_WAITING;
#ifdef MODBUS_DOUBLE_BUFFER
    slave._rx_frame_ok = false;
#else
    slave.frame_ok = false;
#endif

    modbus_slave_3_5t_elapsed(&slave);

    TEST_ASSERT_EQUAL(IDLE, slave.state);
    TEST_ASSERT_FALSE(slave.frame_available);
}

TEST(modbus_slave_rx, test_3_5t_timer_ignore_other_states) {
    slave.state           = IDLE;
    slave.frame_available = false;
    modbus_slave_3_5t_elapsed(&slave);
    TEST_ASSERT_EQUAL(IDLE, slave.state);
    TEST_ASSERT_FALSE(slave.frame_available);

    slave.state = RECEPTION;
    modbus_slave_3_5t_elapsed(&slave);
    TEST_ASSERT_EQUAL(RECEPTION, slave.state);
    TEST_ASSERT_FALSE(slave.frame_available);
}
