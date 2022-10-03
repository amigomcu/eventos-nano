/*
 * PIN, EventOS Input & Output Framework
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
 * 2022-10-02     Dog           the first version
 */

#ifndef __EIO_PIN_H__
#define __EIO_PIN_H__

/* include ------------------------------------------------------------------ */
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
enum
{
    EIO_PIN_MODE_OUT_OD = 0,
    EIO_PIN_MODE_OUT_PP,
    EIO_PIN_MODE_IN,
    EIO_PIN_MODE_IN_PULLUP,
    EIO_PIN_MODE_IN_PULLDOWN,

    EIO_PIN_MODE_MAX
};

/* public typedef ----------------------------------------------------------- */
typedef struct eio_pin_attribute
{
    void *user_data;
    uint8_t mode;
    uint8_t status_default;
} eio_pin_attribute_t;

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
eio_err_t eio_pin_register(eio_pin_t * const me,
                            const char *name,
                            eio_pin_attribute_t *attribute);

/* For the upper layers. */
void eio_pin_set_mode(eio_obj_t * const me, uint8_t mode);
bool eio_pin_get_status(eio_obj_t * const me);
void eio_pin_set_status(eio_obj_t * const me, bool status);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_PIN_H__ */

/* ----------------------------- end of file -------------------------------- */