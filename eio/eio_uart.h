/*
 * UART, EventOS Input & Output Framework
 * Copyright (c) 2022, EventOS Team, <event-os@outlook.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * https://www.event-os.cn
 * https://github.com/event-os/eventos
 * https://gitee.com/event-os/eventos
 * 
 * Change Logs:
 * Date           Author        Notes
 * 2022-10-01     Dog           the first version
 */

#ifndef __EIO_UART_H__
#define __EIO_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* include ------------------------------------------------------------------ */
#include "eio.h"

/* public define ------------------------------------------------------------ */
enum
{

};

enum
{
    EIO_UART_EVT_SEND_END = 0,
    EIO_UART_EVT_SEND_IDLE,
    EIO_UART_EVT_RECEIVE_END,
    EIO_UART_EVT_RECEIVE_IDLE,

    EIO_UART_EVT_MAX
};

enum
{
    EIO_UART_STOPBITS_1 = 0,
    EIO_UART_STOPBITS_2,
};

enum
{
    EIO_UART_PARITY_NONE = 0,
    EIO_UART_PARITY_ODD,
    EIO_UART_PARITY_EVEN,
};

/* public typedef ----------------------------------------------------------- */
/* Default config for uart_configure structure. */
#define EIO_UART_CONFIG_DEFAULT                                                \
{                                                                              \
    115200,                                         /* 115200 bits/s */        \
    8,                                              /* 8 databits */           \
    EIO_UART_STOPBITS_1,                            /* 1 stopbit */            \
    EIO_UART_PARITY_NONE,                           /* No parity  */           \
    0,                                              /* No FIFO  */             \
    0,                                              /* RS232 Mode */           \
    0                                                                          \
}

typedef struct eio_uart_attribute
{
    void *user_data;
    void *buff_send;
    void *buff_recv;
    uint32_t buff_size_send             : 16;
    uint32_t buff_size_send             : 16;
    bool rs485;
} eio_uart_attribute_t;

typedef struct eio_uart_config
{
    uint32_t baud_rate;
    uint32_t data_bits                  :4;
    uint32_t stop_bits                  :2;
    uint32_t parity                     :2;
    uint32_t rs485                      :1;
    uint32_t reserved                   :7;
} eio_uart_config_t;

typedef struct eio_uart
{
    eio_obj_t super;

    const struct io_uart_ops * ops;
    eio_uart_config_t config;
    uint8_t byte_bits;

    uint32_t time_expect_read;                      /* Expected time to read */
    uint32_t time_wait_rx;                          /* Uart reading wait time */
    uint32_t time_send;                             /* Expected time to send */
} eio_uart_t;

struct io_uart_ops
{
    void (* set_tx_mode)(eio_uart_t * const me);
    eio_err_t (* config)(eio_uart_t * const me, eio_uart_config_t *config);
    void (* isr_enable)(eio_uart_t * const me, bool status);
    int32_t (* send)(eio_uart_t * const me, uint8_t byte);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_uart_register(eio_uart_t * const me,
                            const char *name,
                            eio_uart_attribute_t *attribute);
void eio_uart_isr_receive(eio_uart_t * const me, uint8_t byte);
void eio_uart_isr_send_end(eio_uart_t * const me);

/* For the upper layers. */
void eio_uart_config(eio_obj_t * const me, eio_uart_config_t *config);
int32_t eio_uart_write(eio_obj_t * const me, const void *buff, uint32_t size);
int32_t eio_uart_read(eio_obj_t * const me, void *buff, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_UART_H__ */

/* ----------------------------- end of file -------------------------------- */
