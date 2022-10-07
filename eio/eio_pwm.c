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
 * 2022-10-07     Dog           the first version
 */

/* include ------------------------------------------------------------------ */
#include "eio.h"
#include "eio_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* private typedef ---------------------------------------------------------- */
enum
{
    EIO_PWM_STATE_OFF = 0,
    EIO_PWM_STATE_HIGH,
    EIO_PWM_STATE_LOW,
};

/* private function prototype ----------------------------------------------- */
/* Interface functions. */
static eio_err_t _pwm_open(eio_obj_t * const me);
static eio_err_t _pwm_close(eio_obj_t * const me);
static void _pwm_poll(eio_obj_t * const me);

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);
extern void eio_callback_function_null(eio_obj_t * const me);

/* private variables -------------------------------------------------------- */
static const struct eio_ops pwm_ops_soft =
{
    _pwm_open,
    _pwm_close,
    NULL,
    NULL,
    _pwm_poll,
};

static const struct eio_ops pwm_ops_hard =
{
    _pwm_open,
    _pwm_close,
    NULL,
    NULL,
    NULL,
};

static eio_obj_attribute_t pwm_obj_attribute =
{
    EIO_PWM_POLL_PERIOD_US, EIO_TYPE_PWM, EIO_RT_LEVEL_10US, NULL
};

/* public function ---------------------------------------------------------- */
eio_err_t eio_pwm_register(eio_pwm_t * const me,
                            const char *name,
                            eio_pwm_attribute_t *attribute)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(me != NULL, name);
    EIO_ASSERT_NAME(attribute != NULL, name);
    EIO_ASSERT_NAME(me->ops != NULL, name);
    
    /* Set the PWM attribute. */
    me->config.frequency = 0;
    me->config.duty = 0;
    me->mode = attribute->mode;

    /* Set the ops of the super class. */
    if (me->mode == EIO_PWM_MODE_SOFT)
    {
        me->super.ops = (struct eio_ops *)&pwm_ops_soft;

        /* Check the driver interface functions are valid. */
        EIO_ASSERT_NAME(me->ops->set_status != NULL, name);
        EIO_ASSERT_NAME(me->ops->config == NULL, name);
        EIO_ASSERT_NAME(me->ops->enable == NULL, name);

        /* Set the initial status. */
        me->ops->set_status(me, false);
    }
    else
    {
        me->super.ops = (struct eio_ops *)&pwm_ops_hard;

        /* Check the driver interface functions are valid. */
        EIO_ASSERT_NAME(me->ops->set_status == NULL, name);
        EIO_ASSERT_NAME(me->ops->config != NULL, name);
        EIO_ASSERT_NAME(me->ops->enable != NULL, name);

        /* Set the initial status. */
        me->ops->enable(me, false);
    }

    /* Register the pwm to the EIO framwork. */
    pwm_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &pwm_obj_attribute);

    return EIO_OK;
}

void eio_pwm_isr(eio_pwm_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->super.name);
    EIO_ASSERT_NAME(me->mode == EIO_PWM_MODE_HARD, me->super.name);

    /* Pulse counting. */
    me->pulse_count ++;
    if (me->pulse_count >= me->config.pulse_max)
    {
        me->state = EIO_PWM_STATE_OFF;
        me->ops->enable(me, false);
    }
}

void eio_pwm_config(eio_obj_t * const me, eio_pwm_config_t *config)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->name);

    /* pwm type cast. */
    eio_pwm_t *pwm = (eio_pwm_t *)me;
    
    /* Set frequency and duty. */
    EIO_ASSERT_NAME(pwm->ops != NULL, me->name);

    pwm->config.frequency = config->frequency;
    pwm->config.duty = config->duty;
    pwm->config.pulse_max = config->pulse_max;

    pwm->time_peroid = 1000000 / config->frequency;
    pwm->time_high_target = pwm->time_peroid * config->duty / 100;
}

/* private function --------------------------------------------------------- */
static eio_err_t _pwm_enable(eio_obj_t * const me, bool status)
{
    /* PWM type cast. */
    eio_pwm_t *pwm = (eio_pwm_t *)me;
    EIO_ASSERT_NAME(pwm->ops != NULL, me->name);
    EIO_ASSERT_NAME(pwm->config.frequency != 0, me->name);

    /* PWM soft mode. */
    if (pwm->mode == EIO_PWM_MODE_SOFT)
    {
        if (status)
        {
            pwm->state = EIO_PWM_STATE_HIGH;
            pwm->ops->set_status(pwm, true);
        }
        else
        {
            pwm->state = EIO_PWM_STATE_OFF;
            pwm->ops->set_status(pwm, false);
            pwm->time_count = 0;
        }
    }
    /* PWM hard mode. */
    else
    {
        if (status)
        {
            pwm->ops->config(pwm, pwm->config.frequency, pwm->config.duty);
        }
        pwm->ops->enable(pwm, status);
    }

    return EIO_OK;
}

static eio_err_t _pwm_open(eio_obj_t * const me)
{
    return _pwm_enable(me, true);
}

static eio_err_t _pwm_close(eio_obj_t * const me)
{
    return _pwm_enable(me, false);
}

static void _pwm_poll(eio_obj_t * const me)
{
    /* PWM type cast. */
    eio_pwm_t *pwm = (eio_pwm_t *)me;
    EIO_ASSERT_NAME(pwm->mode == EIO_PWM_MODE_SOFT, me->name);

    if (pwm->super.enabled == true)
    {
        if (pwm->state == EIO_PWM_STATE_HIGH)
        {
            pwm->time_count += me->attribute.period;
            if (pwm->time_count >= pwm->time_high_target)
            {
                pwm->state = EIO_PWM_STATE_LOW;
                pwm->ops->set_status(pwm, false);
            }
        }
        else
        {
            pwm->time_count += me->attribute.period;
            if (pwm->time_count >= pwm->time_peroid)
            {
                pwm->time_count = 0;

                if (pwm->pulse_count < pwm->config.pulse_max)
                {
                    pwm->state = EIO_PWM_STATE_HIGH;
                    pwm->ops->set_status(pwm, true);
                    pwm->pulse_count ++;
                }
                else
                {
                    pwm->state = EIO_PWM_STATE_OFF;
                    pwm->ops->set_status(pwm, false);
                    pwm->super.ops->close(me);
                }
            }
        }
    }
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
