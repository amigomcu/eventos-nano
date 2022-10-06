/*
 * CAN bus, EventOS Input & Output Framework
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
 * 2022-10-03     Dog           the first version
 */

#ifndef __EIO_CAN_H__
#define __EIO_CAN_H__

/* include ------------------------------------------------------------------ */
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
enum
{
    EIO_CAN_EVT_MESSAGE_RECEIVED = 0,

    EIO_CAN_EVT_MAX
};

enum
{
    EIO_CAN_BAUDRATE_1M = 0,
    EIO_CAN_BAUDRATE_500K,
    EIO_CAN_BAUDRATE_250K,
    EIO_CAN_BAUDRATE_125K,
    EIO_CAN_BAUDRATE_100K,

    EIO_CAN_BAUDRATE_MAX
};

enum
{
    EIO_CAN_MODE_NORMAL = 0,
    EIO_CAN_MODE_LOOPBACK,
};

enum
{
    EIO_CAN_FILTER_MODE_MASK = 0,
    EIO_CAN_FILTER_MODE_LIST,
};

/* public typedef ----------------------------------------------------------- */
typedef struct eio_can_config
{
    uint8_t baud_rate;
    uint8_t mode;
} eio_can_config_t;

typedef struct eio_can_msg
{
    uint32_t id             : 29;               /* CAN ID */
    uint32_t ide            : 1;                /* Extend CAN ID */
    uint32_t rtr            : 1;                /* Remote CAN Message */
    uint32_t reserve        : 1;
    uint8_t data[8];
    uint8_t length;
} eio_can_msg_t;

typedef struct eio_can_attribute
{
    void *user_data;
    eio_can_msg_t *buff_send;
    eio_can_msg_t *buff_receive;
    uint8_t buff_size_send;
    uint8_t buff_size_receive;
} eio_can_attribute_t;

typedef struct eio_can_buff
{
    eio_can_msg_t *msg;
    uint8_t size;
    uint8_t head;
    uint8_t tail;
} eio_can_buff_t;

typedef struct eio_can_filter
{
    struct eio_can_filter *next;
    uint32_t id             : 29;
    uint32_t mask           : 29;
    uint32_t mode           : 1;
    uint32_t ide            : 1;
    uint32_t rtr            : 1;
} eio_can_filter_t;

typedef struct eio_can
{
    eio_obj_t super;

    const struct eio_can_ops * ops;
    eio_can_buff_t buff_send;
    eio_can_buff_t buff_receive;
    eio_can_config_t config;
    eio_can_filter_t *filter_list;
    bool sending;
    eio_event_t events[EIO_CAN_EVT_MAX];
} eio_can_t;

struct eio_can_ops
{
    eio_err_t (* isr_enable)(eio_can_t * const me, bool enable);
    eio_err_t (* config)(eio_can_t * const me, eio_can_config_t * config);
    void (* send)(eio_can_t * const me, const eio_can_msg_t *msg);
    bool (* send_busy)(eio_can_t * const me);
    eio_err_t (* config_filter)(eio_can_t * const me, eio_can_filter_t *filter);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_can_register(eio_can_t * const me,
                            const char * name,
                            eio_can_attribute_t *attribute);
void eio_can_isr_receive(eio_can_t * const me, eio_can_msg_t *msg);
void eio_can_isr_send_end(eio_can_t * const me);

/* For the upper layers. */
void eio_can_config(eio_obj_t * const me, eio_can_config_t *config);
void eio_can_send(eio_obj_t * const me, const eio_can_msg_t *msg);
bool eio_can_receive(eio_obj_t * const me, eio_can_msg_t *msg);
eio_err_t eio_can_config_filter(eio_obj_t * const me, eio_can_filter_t *filter);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_CAN_H__ */

/* ----------------------------- end of file -------------------------------- */
