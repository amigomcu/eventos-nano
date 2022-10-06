/*
 * Serial port, EventOS Input & Output Framework
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

#ifndef __EIO_SERAIL_H__
#define __EIO_SERAIL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* include ------------------------------------------------------------------ */
#include "eio.h"

/* public define ------------------------------------------------------------ */
enum
{
    EIO_SERIAL_EVT_RECEIVE_IDLE = 0,
    EIO_SERIAL_EVT_RECEIVE_T35 = EIO_SERIAL_EVT_RECEIVE_IDLE,
    EIO_SERIAL_EVT_RECEIVE_T15,

    EIO_SERIAL_EVT_MAX
};

enum
{
    EIO_SERIAL_STOPBITS_1 = 0,
    EIO_SERIAL_STOPBITS_2,
};

enum
{
    EIO_SERIAL_PARITY_NONE = 0,
    EIO_SERIAL_PARITY_ODD,
    EIO_SERIAL_PARITY_EVEN,
};

/* public typedef ----------------------------------------------------------- */
/* Default config for serial_configure structure. */
#define EIO_SERIAL_CONFIG_DEFAULT                                              \
{                                                                              \
    115200,                                         /* 115200 bits/s */        \
    8,                                              /* 8 databits */           \
    EIO_SERIAL_STOPBITS_1,                          /* 1 stopbit */            \
    EIO_SERIAL_PARITY_NONE,                         /* No parity  */           \
    0                                                                          \
}

typedef struct eio_serial_attribute
{
    void *user_data;
    void *buff_tx;
    void *buff_rx;
    uint32_t buff_size_tx               : 16;
    uint32_t buff_size_rx               : 16;
    bool rs485;
} eio_serial_attribute_t;

typedef struct eio_serial_buff
{
    uint8_t *data;
    uint16_t size;
    uint16_t head;
    uint16_t tail;
} eio_serial_buff_t;

typedef struct eio_serial_config
{
    uint32_t baud_rate;
    uint32_t data_bits                  :4;
    uint32_t stop_bits                  :2;
    uint32_t parity                     :2;
    uint32_t reserved                   :24;
} eio_serial_config_t;

typedef struct eio_serial
{
    eio_obj_t super;

    const struct io_serial_ops *ops;
    eio_serial_config_t config;
    eio_serial_buff_t buff_tx;
    eio_serial_buff_t buff_rx;
    eio_time_t time_receive;
    uint32_t threshold_t35;
    uint32_t threshold_t15;
    bool event_t35_sent;
    bool event_t15_sent;
    eio_event_t events[EIO_SERIAL_EVT_MAX];

    /* Only for RS485 mode. */
    bool rs485;
    bool sending;
} eio_serial_t;

struct io_serial_ops
{
    void (* set_tx_state)(eio_serial_t * const me, bool status);
    void (* config)(eio_serial_t * const me, eio_serial_config_t *config);
    void (* isr_enable)(eio_serial_t * const me, bool status);
    int32_t (* send)(eio_serial_t * const me, uint8_t byte);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_serial_register(eio_serial_t * const me,
                                const char *name,
                                eio_serial_attribute_t *attribute);
void eio_serial_isr_receive(eio_serial_t * const me, uint8_t byte);
void eio_serial_isr_send_end(eio_serial_t * const me);

/* For the upper layers. */
void eio_serial_config(eio_obj_t * const me, eio_serial_config_t *config);
int32_t eio_serial_write(eio_obj_t * const me, const void *buff, uint32_t size);
int32_t eio_serial_read(eio_obj_t * const me, void *buff, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_SERIAL_H__ */

/* ----------------------------- end of file -------------------------------- */
