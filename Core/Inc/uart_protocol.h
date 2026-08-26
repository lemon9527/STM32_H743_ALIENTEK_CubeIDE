/* uart_protocol.h - UART1 protocol: ControlPCB -> Display driver board */
#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>

/* UART1 configuration (already initialized by CubeMX at 115200) */
#define UART1_BAUDRATE  115200

/* Frame constants */
#define UART_FRAME_START_0      0x44
#define UART_FRAME_START_1      0x4D
#define UART_FRAME_HEADER_LEN   4   /* Start(2) + FrameLength(2) */
#define UART_FRAME_DATA_LEN     36  /* All data fields */
#define UART_FRAME_CHECKSUM_LEN 2
#define UART_FRAME_TOTAL_LEN    42  /* 4 + 36 + 2 = 42 */
#define UART_FRAME_LENGTH_VALUE 38  /* Frame Length field value: 36 data + 2 checksum */

/* Ring buffer size (must be power of 2) */
#define UART_RB_SIZE  256

/* ---------------------------------------------------------------------------
 * Parsed sensor data structure (big-endian from protocol decoded to host-endian)
 * --------------------------------------------------------------------------- */
typedef struct {
    uint16_t pm2_5;             /* PM2.5 concentration (ug/m3)       */
    uint16_t pm10;              /* PM10 concentration (ug/m3)        */
    uint16_t pm0_3;             /* PM0.3 concentration (ug/m3)       */
    uint8_t  model_type;        /* Device model type                 */
    uint8_t  machine_mode;      /* Machine operating mode            */
    uint8_t  left_filter_type;  /* Left filter type                  */
    uint8_t  right_filter_type; /* Right filter type                 */
    uint8_t  left_hepa_life;    /* Left HEPA filter remaining life % */
    uint8_t  left_carbon_life;  /* Left carbon filter remaining life % */
    uint8_t  right_hepa_life;   /* Right HEPA filter remaining life % */
    uint8_t  right_carbon_life; /* Right carbon filter remaining life % */
    uint8_t  voc_level;         /* VOC level                         */
    uint8_t  screen_state;      /* Screen state                      */
    uint8_t  door_state;        /* Door state                        */
    uint8_t  air_index;         /* Air quality index                 */
    uint16_t co2;               /* CO2 concentration (ppm)           */
    uint16_t tvoc;              /* TVOC concentration (ppb)          */
    uint16_t temperature;       /* Temperature (raw value)           */
    uint16_t humidity;          /* Humidity (raw value)              */
    uint16_t pressure;          /* Pressure (raw value)              */
    uint8_t  connection_type;   /* Connection type                   */
    uint8_t  lightsensor_state; /* Light sensor state                */
    uint8_t  brightness_mode;   /* Brightness mode                   */
    uint8_t  lightsensor_level; /* Light sensor level                */
    uint8_t  sys_error_code;    /* System error code                 */
} sensor_data_t;

/* ---------------------------------------------------------------------------
 * Ring buffer (ISR-safe, single-producer single-consumer)
 * --------------------------------------------------------------------------- */
typedef struct {
    volatile uint8_t  buffer[UART_RB_SIZE];
    volatile uint16_t head;   /* write index (ISR puts data here) */
    volatile uint16_t tail;   /* read index  (task takes data from here) */
} uart_ringbuf_t;

/* ---------------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------------- */

/* Initialise ring buffer */
void uart_rb_init(uart_ringbuf_t *rb);

/* Write one byte to ring buffer.  Called from ISR.  Returns 0 on success. */
int uart_rb_write(uart_ringbuf_t *rb, uint8_t data);

/* Read one byte from ring buffer.  Called from task.  Returns 0 on success. */
int uart_rb_read(uart_ringbuf_t *rb, uint8_t *data);

/* Number of bytes currently available in ring buffer */
uint16_t uart_rb_available(const uart_ringbuf_t *rb);

/* Initialise ring buffer and start UART1 RX interrupt */
void uart_protocol_init(void);

/* Process incoming data from ring buffer.  Call this periodically from a task. */
void uart_protocol_process(void);

/* Get the latest parsed sensor data (thread-safe copy) */
void uart_protocol_get_data(sensor_data_t *out);

/* Check whether at least one valid frame has been received */
int uart_protocol_has_data(void);

/* Handle one received byte (called from ISR / UART callback) */
void uart_protocol_rx_byte(uint8_t byte);

/* Global ring buffer reference (accessible from ISR) */
extern uart_ringbuf_t g_uart_rb;

/* Diagnostic counters */
extern volatile uint32_t g_uart1_rx_irq_count;   /* RX interrupt callback count */
extern volatile uint32_t g_uart1_rb_overflow;     /* ring buffer overflow drops */

#endif /* UART_PROTOCOL_H */