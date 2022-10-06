/*
 * Analog input, EventOS Input & Output Framework
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

#ifndef __EIO_AIN_H__
#define __EIO_AIN_H__

/* include ------------------------------------------------------------------ */
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
enum
{
    EIO_AIN_MODE_SOFT_TRIG = 0,
    EIO_AIN_MODE_HARD_TRIG,

    EIO_AIN_MODE_MAX
};

/* public typedef ----------------------------------------------------------- */
typedef struct eio_ain_attribute
{
    void *user_data;
    uint8_t mode;
} eio_ain_attribute_t;

typedef struct eio_ain
{
    eio_obj_t super;
    const struct eio_ain_ops *ops;
    uint8_t mode;
    uint16_t mv;
    uint16_t *buff;
    uint16_t buff_size;
    uint16_t current;
} eio_ain_t;

struct eio_ain_ops
{
    uint16_t (* get_value)(eio_ain_t * const me);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_ain_register(eio_ain_t * const me,
                            const char *name,
                            eio_ain_attribute_t *attribute);
void eio_ain_isr(eio_ain_t * const me, uint16_t value);

/* For the upper layers. */
void eio_ain_set_peroid(eio_obj_t * const me, uint32_t time_us);
uint16_t eio_ain_get_current_mv(eio_obj_t * const me);
uint16_t eio_ain_get_average_mv(eio_obj_t * const me);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_ain_H__ */

/* ----------------------------- end of file -------------------------------- */
