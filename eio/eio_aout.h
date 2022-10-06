/*
 * Analog output, EventOS Input & Output Framework
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
 * 2022-10-06     Dog           the first version
 */

#ifndef __EIO_AOUT_H__
#define __EIO_AOUT_H__

/* include ------------------------------------------------------------------ */
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public typedef ----------------------------------------------------------- */
typedef struct eio_aout_attribute
{
    void *user_data;
    uint16_t mv_init;
} eio_aout_attribute_t;

typedef struct eio_aout
{
    eio_obj_t super;
    const struct eio_aout_ops *ops;

    uint16_t mv;
} eio_aout_t;

struct eio_aout_ops
{
    void (* set_value)(eio_aout_t * const me, uint16_t mv);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_aout_register(eio_aout_t * const me,
                            const char *name,
                            eio_aout_attribute_t *attribute);

/* For the upper layers. */
void eio_aout_set_value(eio_obj_t * const me, uint16_t mv);
uint16_t eio_aout_get_value(eio_obj_t * const me);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_AOUT_H__ */

/* ----------------------------- end of file -------------------------------- */
