#include "app.h"
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "modbus_slave.h"
#include "modbus_bytes.h"
#include "config.h"
#include "brightness_lut.h"

#define PWM_NUM_CH       16U
#define PWM_MAX          4095U
#define REG_CONFIG_BASE  16U  /* Modbus address where config registers begin */

/* ---------- private variables --------------------------------------------- */
static ModbusSlave       modbus;
static uint8_t           uart_rx_buf[1];
static volatile uint8_t  timer_counter;
static uint16_t          pwm[PWM_NUM_CH];          /* Modbus targets (0–4095)        */
static uint32_t          current_pwm_fp[PWM_NUM_CH]; /* Q16.16 animated current value */
static uint32_t          last_tick;
static Config            cfg;
static volatile bool     pending_reboot;

/* ---------- PWM channel map ----------------------------------------------- */
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
} PwmMap;

/* Maps Modbus register address (0–15) to timer + channel.
   Follows schematic pin assignment: CH1–CH4 → TIM3, CH5–CH8 → TIM4,
   CH9–CH12 → TIM2 (reversed), CH13–CH16 → TIM1 (reversed). */
static const PwmMap pwm_map[PWM_NUM_CH] = {
    { &htim3, TIM_CHANNEL_1 },  /* CH1  — PC6  */
    { &htim3, TIM_CHANNEL_2 },  /* CH2  — PC7  */
    { &htim3, TIM_CHANNEL_3 },  /* CH3  — PC8  */
    { &htim3, TIM_CHANNEL_4 },  /* CH4  — PC9  */
    { &htim4, TIM_CHANNEL_1 },  /* CH5  — PB6  */
    { &htim4, TIM_CHANNEL_2 },  /* CH6  — PB7  */
    { &htim4, TIM_CHANNEL_3 },  /* CH7  — PB8  */
    { &htim4, TIM_CHANNEL_4 },  /* CH8  — PB9  */
    { &htim2, TIM_CHANNEL_4 },  /* CH9  — PA3  */
    { &htim2, TIM_CHANNEL_3 },  /* CH10 — PA2  */
    { &htim2, TIM_CHANNEL_2 },  /* CH11 — PA1  */
    { &htim2, TIM_CHANNEL_1 },  /* CH12 — PA0  */
    { &htim1, TIM_CHANNEL_4 },  /* CH13 — PC3  */
    { &htim1, TIM_CHANNEL_3 },  /* CH14 — PC2  */
    { &htim1, TIM_CHANNEL_2 },  /* CH15 — PC1  */
    { &htim1, TIM_CHANNEL_1 },  /* CH16 — PC0  */
};

/* ---------- PWM output ---------------------------------------------------- */

/* Apply the current animated value for channel ch.
   Passes through the CIE L* LUT then scales by per-channel max brightness. */
static void pwm_apply_channel(uint8_t ch)
{
    uint16_t current = (uint16_t)(current_pwm_fp[ch] >> 16);
    /* Cap perceived brightness first, then apply CIE L* LUT for physical output.
       This ensures the curve shape is preserved at any max_brightness setting. */
    uint16_t capped  = (uint16_t)((uint32_t)current * cfg.max_brightness[ch] / PWM_MAX);
    uint16_t ccr     = brightness_lut[capped];
    __HAL_TIM_SET_COMPARE(pwm_map[ch].htim, pwm_map[ch].channel, ccr);
}

/* Step all channels toward their targets based on elapsed time.
   Ramp step uses Q16.16 fixed-point: 4095 full-scale units across ramp_time_ms. */
static void ramp_update(void)
{
    uint32_t now   = HAL_GetTick();
    uint32_t delta = now - last_tick;
    if (delta == 0) return;
    last_tick = now;

    for (uint8_t ch = 0; ch < PWM_NUM_CH; ch++) {
        uint32_t target_fp = (uint32_t)pwm[ch] << 16;
        if (current_pwm_fp[ch] == target_fp) continue;

        if (cfg.ramp_time_ms[ch] == 0) {
            current_pwm_fp[ch] = target_fp;
        } else {
            /* step_fp = 4095 * 65536 * delta / ramp_time_ms  (Q16.16 units) */
            uint32_t step_fp = (uint32_t)((uint64_t)PWM_MAX * 65536ULL * delta
                                          / cfg.ramp_time_ms[ch]);
            if (current_pwm_fp[ch] < target_fp) {
                uint32_t remaining = target_fp - current_pwm_fp[ch];
                current_pwm_fp[ch] = (remaining <= step_fp) ? target_fp
                                                             : current_pwm_fp[ch] + step_fp;
            } else {
                uint32_t remaining = current_pwm_fp[ch] - target_fp;
                current_pwm_fp[ch] = (remaining <= step_fp) ? target_fp
                                                             : current_pwm_fp[ch] - step_fp;
            }
        }
        pwm_apply_channel(ch);
    }
}

/* ---------- Modbus callbacks ---------------------------------------------- */
static ModbusExceptionCode mb_read_holding_registers(uint16_t addr, uint16_t count, uint8_t *dest)
{
    if (addr + count > REG_CONFIG_BASE + CONFIG_NUM_REGS) return MODBUS_EX_ILLEGAL_DATA_ADDRESS;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t reg = addr + i;
        uint16_t val = (reg < REG_CONFIG_BASE) ? pwm[reg]
                                               : config_get_reg(&cfg, reg - REG_CONFIG_BASE);
        modbus_be16_set(&dest[i * 2], val);
    }
    return MODBUS_EX_NONE;
}

static ModbusExceptionCode mb_write_single_register(uint16_t addr, uint16_t value)
{
    if (addr < REG_CONFIG_BASE) {
        if (value > PWM_MAX) return MODBUS_EX_ILLEGAL_DATA_VALUE;
        pwm[addr] = value;
        return MODBUS_EX_NONE;
    }

    uint16_t cfg_addr = addr - REG_CONFIG_BASE;
    if (cfg_addr >= CONFIG_NUM_REGS)           return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    if (!config_validate_reg(cfg_addr, value)) return MODBUS_EX_ILLEGAL_DATA_VALUE;
    config_set_reg(&cfg, cfg_addr, value);
    if (!config_save(&cfg))                    return MODBUS_EX_SLAVE_DEVICE_FAILURE;
    if (cfg_addr >= CONFIG_REG_COMM_BASE)      pending_reboot = true;
    if (cfg_addr < 16U)                        pwm_apply_channel((uint8_t)cfg_addr);
    return MODBUS_EX_NONE;
}

static ModbusExceptionCode mb_write_multiple_registers(uint16_t addr, uint16_t count, const uint8_t *src)
{
    if (addr + count > REG_CONFIG_BASE + CONFIG_NUM_REGS) return MODBUS_EX_ILLEGAL_DATA_ADDRESS;

    /* Validate pass */
    for (uint16_t i = 0; i < count; i++) {
        uint16_t reg = addr + i;
        uint16_t val = modbus_be16_get(&src[i * 2]);
        if (reg < REG_CONFIG_BASE) {
            if (val > PWM_MAX) return MODBUS_EX_ILLEGAL_DATA_VALUE;
        } else {
            if (!config_validate_reg(reg - REG_CONFIG_BASE, val)) return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
    }

    /* Apply pass */
    bool cfg_changed  = false;
    bool comm_changed = false;
    for (uint16_t i = 0; i < count; i++) {
        uint16_t reg = addr + i;
        uint16_t val = modbus_be16_get(&src[i * 2]);
        if (reg < REG_CONFIG_BASE) {
            pwm[reg] = val;
        } else {
            uint16_t cfg_addr = reg - REG_CONFIG_BASE;
            config_set_reg(&cfg, cfg_addr, val);
            cfg_changed = true;
            if (cfg_addr >= CONFIG_REG_COMM_BASE) comm_changed = true;
            if (cfg_addr < 16U)                   pwm_apply_channel((uint8_t)cfg_addr);
        }
    }

    if (cfg_changed) {
        if (!config_save(&cfg)) return MODBUS_EX_SLAVE_DEVICE_FAILURE;
        if (comm_changed) pending_reboot = true;
    }
    return MODBUS_EX_NONE;
}

/* ---------- RS-485 transmit ------------------------------------------------ */
static void mb_transmit(const uint8_t *data, uint16_t length)
{
    HAL_UART_AbortReceive(&huart4);
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_SET);
    HAL_UART_Transmit(&huart4, (uint8_t *)data, length, HAL_MAX_DELAY);
    while (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_TC) == RESET);
    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
    __HAL_UART_CLEAR_FLAG(&huart4, UART_FLAG_RXNE);
    HAL_UART_Receive_IT(&huart4, uart_rx_buf, 1);
}

/* ---------- HAL callbacks ------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART4) return;

    __HAL_TIM_SET_COUNTER(&htim6, 0);
    timer_counter = 0;

    modbus_slave_rx_byte(&modbus, uart_rx_buf[0]);

    HAL_UART_Receive_IT(&huart4, uart_rx_buf, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != UART4) return;
    HAL_UART_Receive_IT(&huart4, uart_rx_buf, 1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM6) return;
    if (modbus.state != RECEPTION && modbus.state != CONTROL_AND_WAITING) return;

    timer_counter++;
    if (timer_counter == 3) modbus_slave_1_5t_elapsed(&modbus);
    if (timer_counter == 7) modbus_slave_3_5t_elapsed(&modbus);
}

/* ---------- public API ---------------------------------------------------- */
void app_init(void)
{
    cfg = config_load();
    config_apply(&cfg, &huart4, &htim6);

    for (uint8_t i = 0; i < PWM_NUM_CH; i++)
        HAL_TIM_PWM_Start(pwm_map[i].htim, pwm_map[i].channel);

    last_tick = HAL_GetTick();

    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);

    ModbusSlaveConfig modbus_cfg = {
        .address                  = cfg.slave_addr,
        .write                    = mb_transmit,
        .read_holding_registers   = mb_read_holding_registers,
        .write_single_register    = mb_write_single_register,
        .write_multiple_registers = mb_write_multiple_registers,
    };
    modbus_slave_init(&modbus, &modbus_cfg);

    HAL_TIM_Base_Start_IT(&htim6);
    HAL_UART_Receive_IT(&huart4, uart_rx_buf, 1);
}

void app_poll(void)
{
    ramp_update();

    if (modbus.frame_available)
        modbus_slave_poll(&modbus);

    if (pending_reboot)
        NVIC_SystemReset();
}
