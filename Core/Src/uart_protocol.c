/**
 * @file uart_protocol.c
 * @brief UART1 protocol handler: ring buffer + frame parser + debug print
 *
 * Protocol: ControlPCB -> STM32H743 Display driver board.
 * Frame: 0x44 0x4D + 2-byte length + 36 bytes data + 2-byte checksum.
 * Checksum: sum of all bytes before checksum, & 0xFFFF.
 * Timing: one frame every second at 115200 baud.
 *
 * Note: USART1 is already initialized by CubeMX (MX_USART1_UART_Init in main.c).
 *       This module only starts the RX interrupt.
 */

#include "uart_protocol.h"
#include "usart.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Ring buffer (global, accessible from ISR via uart_protocol_rx_byte)
 * --------------------------------------------------------------------------- */
uart_ringbuf_t g_uart_rb;

void uart_rb_init(uart_ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

int uart_rb_write(uart_ringbuf_t *rb, uint8_t data)
{
    uint16_t next = (uint16_t)((rb->head + 1) & (UART_RB_SIZE - 1));
    if (next == rb->tail)
        return -1;          /* buffer full */

    rb->buffer[rb->head] = data;
    rb->head = next;
    return 0;
}

int uart_rb_read(uart_ringbuf_t *rb, uint8_t *data)
{
    if (rb->head == rb->tail)
        return -1;          /* buffer empty */

    *data = rb->buffer[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1) & (UART_RB_SIZE - 1));
    return 0;
}

uint16_t uart_rb_available(const uart_ringbuf_t *rb)
{
    return (uint16_t)((rb->head - rb->tail) & (UART_RB_SIZE - 1));
}

/* ---------------------------------------------------------------------------
 * Frame parser state machine
 * --------------------------------------------------------------------------- */
typedef enum {
    STATE_IDLE,      /* waiting for first start byte 0x44 */
    STATE_START0,    /* received 0x44, waiting for 0x4D */
    STATE_LENGTH,    /* receiving 2-byte frame length */
    STATE_DATA,      /* receiving payload data */
    STATE_CHECKSUM   /* receiving 2-byte checksum */
} parse_state_t;

static parse_state_t g_state = STATE_IDLE;
static uint8_t  g_frame_buf[UART_FRAME_TOTAL_LEN];
static uint16_t g_frame_idx = 0;
static uint16_t g_frame_len = 0;

/* Single-byte receive buffer for UART1 IT mode (used by init + callback) */
static uint8_t g_uart1_rx_byte;

/* Diagnostic counters */
volatile uint32_t g_uart1_rx_irq_count = 0;   /* incremented in callback */
volatile uint32_t g_uart1_rb_overflow = 0;     /* ring buffer full drops */

/* Latest parsed sensor data (protected by critical section) */
static sensor_data_t g_sensor_data;
static int g_data_valid = 0;

/* ---------------------------------------------------------------------------
 * Checksum calculation
 * --------------------------------------------------------------------------- */
static uint16_t calc_checksum(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
        sum += data[i];
    return (uint16_t)(sum & 0xFFFF);
}

/* ---------------------------------------------------------------------------
 * Parse a complete frame and print to debug UART1
 * --------------------------------------------------------------------------- */
static void parse_frame(const uint8_t *buf)
{
    sensor_data_t sd;
    const uint8_t *d = buf + 4;   /* skip Start(2) + FrameLength(2) */

    sd.pm2_5             = ((uint16_t)d[0] << 8) | (uint16_t)d[1];
    sd.pm10              = ((uint16_t)d[2] << 8) | (uint16_t)d[3];
    sd.pm0_3             = ((uint16_t)d[4] << 8) | (uint16_t)d[5];
    sd.model_type        = d[6];
    sd.machine_mode      = d[7];
    /* d[8..9]  = reserved */
    sd.left_filter_type  = d[10];
    sd.right_filter_type = d[11];
    sd.left_hepa_life    = d[12];
    sd.left_carbon_life  = d[13];
    sd.right_hepa_life   = d[14];
    sd.right_carbon_life = d[15];
    sd.voc_level         = d[16];
    sd.screen_state      = d[17];
    sd.door_state        = d[18];
    sd.air_index         = d[19];
    sd.co2               = ((uint16_t)d[20] << 8) | (uint16_t)d[21];
    sd.tvoc              = ((uint16_t)d[22] << 8) | (uint16_t)d[23];
    sd.temperature       = ((uint16_t)d[24] << 8) | (uint16_t)d[25];
    sd.humidity          = ((uint16_t)d[26] << 8) | (uint16_t)d[27];
    sd.pressure          = ((uint16_t)d[28] << 8) | (uint16_t)d[29];
    sd.connection_type   = d[30];
    sd.lightsensor_state = d[31];
    sd.brightness_mode   = d[32];
    sd.lightsensor_level = d[33];
    /* d[34] = reserved */
    sd.sys_error_code    = d[35];

    /* Thread-safe copy to global (critical section) */
    portENTER_CRITICAL();
    memcpy(&g_sensor_data, &sd, sizeof(sensor_data_t));
    g_data_valid = 1;
    portEXIT_CRITICAL();

    /* Print to debug UART1 */
    printf("\r\n========== UART1 Frame Received ==========\r\n");
    printf("  PM2.5:             %u ug/m3\r\n",  sd.pm2_5);
    printf("  PM10:              %u ug/m3\r\n",  sd.pm10);
    printf("  PM0.3:             %u ug/m3\r\n",  sd.pm0_3);
    printf("  Model Type:        %u\r\n",         sd.model_type);
    printf("  Machine Mode:      %u\r\n",         sd.machine_mode);
    printf("  Left HEPA Life:    %u%%\r\n",      sd.left_hepa_life);
    printf("  Left Carbon Life:  %u%%\r\n",      sd.left_carbon_life);
    printf("  Right HEPA Life:   %u%%\r\n",      sd.right_hepa_life);
    printf("  Right Carbon Life: %u%%\r\n",      sd.right_carbon_life);
    printf("  VOC Level:         %u\r\n",         sd.voc_level);
    printf("  Screen State:      %u\r\n",         sd.screen_state);
    printf("  Door State:        %u\r\n",         sd.door_state);
    printf("  Air Index:         %u\r\n",         sd.air_index);
    printf("  CO2:               %u ppm\r\n",     sd.co2);
    printf("  TVOC:              %u ppb\r\n",     sd.tvoc);
    printf("  Temperature:       %u\r\n",         sd.temperature);
    printf("  Humidity:          %u\r\n",         sd.humidity);
    printf("  Pressure:          %u\r\n",         sd.pressure);
    printf("  Connection Type:   %u\r\n",         sd.connection_type);
    printf("  Lightsensor State: %u\r\n",         sd.lightsensor_state);
    printf("  Brightness Mode:   %u\r\n",         sd.brightness_mode);
    printf("  Lightsensor Level: %u\r\n",         sd.lightsensor_level);
    printf("  Sys Error Code:    %u\r\n",         sd.sys_error_code);
    printf("========================================\r\n");
}

/* ---------------------------------------------------------------------------
 * Feed one byte into the parser state machine
 * --------------------------------------------------------------------------- */
static void feed_byte(uint8_t byte)
{
    switch (g_state)
    {
    case STATE_IDLE:
        if (byte == UART_FRAME_START_0)
        {
            g_frame_buf[0] = byte;
            g_frame_idx = 1;
            g_state = STATE_START0;
        }
        break;

    case STATE_START0:
        if (byte == UART_FRAME_START_1)
        {
            g_frame_buf[1] = byte;
            g_frame_idx = 2;
            g_state = STATE_LENGTH;
        }
        else
        {
            g_state = STATE_IDLE;   /* bad sync */
        }
        break;

    case STATE_LENGTH:
        g_frame_buf[g_frame_idx++] = byte;
        if (g_frame_idx >= 4)
        {
            g_frame_len = ((uint16_t)g_frame_buf[2] << 8)
                        | (uint16_t)g_frame_buf[3];
            if (g_frame_len == UART_FRAME_LENGTH_VALUE)
                g_state = STATE_DATA;
            else
                g_state = STATE_IDLE;   /* invalid length */
        }
        break;

    case STATE_DATA:
        g_frame_buf[g_frame_idx++] = byte;
        if (g_frame_idx >= (uint16_t)(UART_FRAME_HEADER_LEN + g_frame_len - UART_FRAME_CHECKSUM_LEN))
            g_state = STATE_CHECKSUM;
        break;

    case STATE_CHECKSUM:
        g_frame_buf[g_frame_idx++] = byte;
        if (g_frame_idx >= UART_FRAME_TOTAL_LEN)
        {
            /* Verify 16-bit checksum */
            uint16_t calc_cs = calc_checksum(g_frame_buf,
                                             UART_FRAME_TOTAL_LEN - 2);
            uint16_t rx_cs   = ((uint16_t)g_frame_buf[UART_FRAME_TOTAL_LEN - 2] << 8)
                             | (uint16_t)g_frame_buf[UART_FRAME_TOTAL_LEN - 1];
            if (calc_cs == rx_cs)
                parse_frame(g_frame_buf);

            g_state = STATE_IDLE;
        }
        break;
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void uart_protocol_rx_byte(uint8_t byte)
{
    (void)uart_rb_write(&g_uart_rb, byte);
}

void uart_protocol_init(void)
{
    /* Initialise ring buffer */
    uart_rb_init(&g_uart_rb);

    /* USART1 is already initialized by CubeMX (MX_USART1_UART_Init in main.c).
     * Just start the RX interrupt. */
    g_uart1_rx_byte = 0;
    HAL_UART_Receive_IT(&huart1, &g_uart1_rx_byte, 1);
}

void uart_protocol_process(void)
{
    uint8_t byte;
    while (uart_rb_read(&g_uart_rb, &byte) == 0)
        feed_byte(byte);
}

void uart_protocol_get_data(sensor_data_t *out)
{
    portENTER_CRITICAL();
    memcpy(out, &g_sensor_data, sizeof(sensor_data_t));
    portEXIT_CRITICAL();
}

int uart_protocol_has_data(void)
{
    return g_data_valid;
}

/* ---------------------------------------------------------------------------
 * HAL UART RX complete callback (called from ISR context)
 * --------------------------------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* Increment diagnostic counter */
        g_uart1_rx_irq_count++;

        /* Write the received byte to ring buffer */
        if (uart_rb_write(&g_uart_rb, g_uart1_rx_byte) != 0)
            g_uart1_rb_overflow++;

        /* Re-arm for next byte */
        HAL_UART_Receive_IT(&huart1, &g_uart1_rx_byte, 1);
    }
}