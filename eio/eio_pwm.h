/*
 * PWM, EventOS Input & Output Framework
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

#ifndef __EIO_PWM_H__
#define __EIO_PWM_H__

/* include ------------------------------------------------------------------ */
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public config ------------------------------------------------------------ */
#define EIO_PWM_POLL_PERIOD_US                  (10)

/* public define ------------------------------------------------------------ */
enum
{
    EIO_PWM_MODE_SOFT = 0,
    EIO_PWM_MODE_HARD,

    EIO_PWM_MODE_MAX
};

/* public typedef ----------------------------------------------------------- */
typedef struct eio_pwm_attribute
{
    void *user_data;
    uint8_t mode;
} eio_pwm_attribute_t;

typedef struct eio_pwm_config
{
    uint32_t frequency;
    uint8_t duty;
    uint32_t pulse_max;
} eio_pwm_config_t;

typedef struct eio_pwm
{
    eio_obj_t super;
    const struct eio_pwm_ops *ops;

    eio_pwm_config_t config;
    uint8_t mode;

    bool status;                                /* PWM PIN status */
    uint8_t state;
    uint32_t time_count;
    uint32_t time_high_target;
    uint32_t time_peroid;
    uint32_t pulse_count;
} eio_pwm_t;

struct eio_pwm_ops
{
    /* In soft mode, this function must be completed in driver layer. */
    void (* set_status)(eio_pwm_t * const me, bool status);

    /* In hard mode, this following two function must be completed in driver layer. */
    void (* config)(eio_pwm_t * const me, uint32_t frequency, uint8_t duty);
    void (* enable)(eio_pwm_t * const me, bool status);
};

/* public function ---------------------------------------------------------- */
/* For the driver layer. */
eio_err_t eio_pwm_register(eio_pwm_t * const me,
                            const char *name,
                            eio_pwm_attribute_t *attribute);
void eio_pwm_isr(eio_pwm_t * const me);

/* For the upper layers. */
void eio_pwm_config(eio_obj_t * const me, eio_pwm_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_PWM_H__ */

/* ----------------------------- end of file -------------------------------- */
