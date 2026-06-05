#include "unity_fixture.h"
#include "modbus_slave.h"

#include <string.h>

TEST_GROUP(modbus_slave_init);

static ModbusSlave       slave;
static ModbusSlaveConfig config;

static void mock_write(const uint8_t *data, uint16_t length)
{
    (void)data; (void)length;
}

TEST_SETUP(modbus_slave_init)
{
    memset(&slave,  0, sizeof(slave));
    memset(&config, 0, sizeof(config));
    config.address = 0x01;
    config.write   = mock_write;
}

TEST_TEAR_DOWN(modbus_slave_init) {}

/* --- happy path ----------------------------------------------------------- */

TEST(modbus_slave_init, test_slave_init_success)
{
    int result = modbus_slave_init(&slave, &config);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(config.address, slave.config.address);
    TEST_ASSERT_EQUAL(IDLE, slave.state);
    TEST_ASSERT_EQUAL(0, slave.frame_len);
    TEST_ASSERT_TRUE(slave.frame_ok);
    TEST_ASSERT_FALSE(slave.frame_available);
#ifndef MODBUS_DOUBLE_BUFFER
    TEST_ASSERT_FALSE(slave.processing_frame);
#endif
}

TEST(modbus_slave_init, test_slave_init_preserves_config)
{
    config.read_coils             = (ModbusReadCoilsCb)0x12345678;
    config.write_single_register  = (ModbusWriteSingleRegisterCb)0x87654321;

    TEST_ASSERT_EQUAL(0, modbus_slave_init(&slave, &config));
    TEST_ASSERT_EQUAL_PTR(config.read_coils,            slave.config.read_coils);
    TEST_ASSERT_EQUAL_PTR(config.write_single_register, slave.config.write_single_register);
}

/* --- null argument guards ------------------------------------------------- */

TEST(modbus_slave_init, test_slave_init_null_slave)
{
    TEST_ASSERT_EQUAL(-1, modbus_slave_init(NULL, &config));
}

TEST(modbus_slave_init, test_slave_init_null_config)
{
    TEST_ASSERT_EQUAL(-1, modbus_slave_init(&slave, NULL));
}

TEST(modbus_slave_init, test_slave_init_null_write_function)
{
    ModbusSlaveConfig bad = config;
    bad.write = NULL;
    TEST_ASSERT_EQUAL(-1, modbus_slave_init(&slave, &bad));
}

/* --- address validation --------------------------------------------------- */

TEST(modbus_slave_init, test_slave_init_address_zero)
{
    /* Address 0x00 is reserved for broadcast — must be rejected */
    config.address = 0x00;
    TEST_ASSERT_EQUAL(-1, modbus_slave_init(&slave, &config));
}

TEST(modbus_slave_init, test_slave_init_address_max_valid)
{
    /* 0xF7 (247) is the highest valid slave address per Modbus spec */
    config.address = 0xF7;
    TEST_ASSERT_EQUAL(0, modbus_slave_init(&slave, &config));
}

/* 0xF8–0xFF are reserved per Modbus spec and must be rejected */
TEST(modbus_slave_init, test_slave_init_address_reserved)
{
    config.address = 0xF8;
    TEST_ASSERT_EQUAL(-1, modbus_slave_init(&slave, &config));
}
