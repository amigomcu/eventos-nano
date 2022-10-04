/*
 * 1-Wire bus, EventOS Input & Output Framework
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

#ifndef __EIO_1_WIRE_H__
#define __EIO_1_WIRE_H__

/* include ------------------------------------------------------------------ */
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
enum
{
    EIO_1WIRE_OK = 0,
    EIO_1WIRE_ERROR,
};

enum
{
    EIO_1WIRE_PIN_MODE_OUT_OD = 0,
    EIO_1WIRE_PIN_MODE_IN,

    EIO_1WIRE_PIN_MODE_MAX
};

/* public typedef ----------------------------------------------------------- */
typedef struct eio_1wire_attribute
{
    void *user_data;
    uint8_t *buff_send;
    uint8_t *buff_receive;
    uint8_t size_send;
    uint8_t size_receice;
} eio_1wire_attribute_t;

typedef struct eio_pin_tag
{
    eio_obj_t super;
    const struct eio_pin_ops *ops;

    bool status;
    uint8_t mode;
} eio_pin_t;

struct eio_pin_ops
{
    void (* set_mode)(eio_pin_t * const me, uint8_t mode);
    void (* set_status)(eio_pin_t * const me, bool status);
    bool (* get_status)(eio_pin_t * const me);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_1wire_register(eio_pin_t * const me,
                                const char *name,
                                eio_1wire_attribute_t *attribute);

/* For the upper layers */
eio_err_t eio_1wire_reset(eio_pin_t * const me);
eio_err_t eio_1wire_write(eio_pin_t * const me,
                            uint8_t byte_send[], uint8_t size_send,
                            uint8_t size_receive);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_1_WIRE_H__ */

/* ----------------------------- end of file -------------------------------- */