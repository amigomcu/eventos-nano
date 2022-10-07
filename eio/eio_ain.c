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

/* include ------------------------------------------------------------------ */
#include "eio_ain.h"

#ifdef __cplusplus
extern "C" {
#endif

/* private function prototype ----------------------------------------------- */
/* Interface functions. */
static void _ain_poll(eio_obj_t * const me);

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);

/* private variables -------------------------------------------------------- */
static const struct eio_ops ain_ops =
{
    NULL,
    NULL,
    NULL,
    NULL,
    _ain_poll,
};

static eio_obj_attribute_t pin_obj_attribute =
{
    0, EIO_TYPE_AIN, EIO_RT_LEVEL_100US, NULL
};

/* public function ---------------------------------------------------------- */
eio_err_t eio_ain_register(eio_ain_t * const me,
                            const char *name,
                            eio_ain_attribute_t *attribute)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(me != NULL, name);
    EIO_ASSERT_NAME(attribute != NULL, name);

    /* Set the ain attribute. */
    me->mode = attribute->mode;

    /* Set the ops of the super class. */
    if (me->mode == EIO_AIN_MODE_HARD_TRIG)
    {
        me->mv = UINT16_MAX;
        me->super.ops = NULL;
    }
    else
    {
        me->super.ops = (struct eio_ops *)&ain_ops;
        EIO_ASSERT_NAME(me->ops != NULL, name);
        EIO_ASSERT_NAME(me->ops->get_value != NULL, name);

        /* Read the initial status for the input pin. */
        me->mv = me->ops->get_value(me);
        for (uint16_t i = 0; i < me->buff_size; i ++)
        {
            me->buff[i] = me->mv;
        }
        me->current = 0;
    }

    /* Register the Analog Input object to the EIO framwork. */
    pin_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &pin_obj_attribute);

    return EIO_OK;
}

void eio_ain_isr(eio_ain_t * const me, uint16_t value)
{
    EIO_ASSERT_NAME(me->mode == EIO_AIN_MODE_HARD_TRIG, me->super.name);

    if (me->mv == UINT16_MAX)
    {
        me->mv = value;
        for (uint16_t i = 0; i < me->buff_size; i ++)
        {
            me->buff[i] = me->mv;
        }
        me->current = 0;
    }
    else
    {
        me->mv = value;
        me->buff[me->current ++] = value;
    }
}

void eio_ain_set_peroid(eio_obj_t * const me, uint32_t time_us)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->name);
    me->attribute.period = time_us;
}

uint16_t eio_ain_get_current_value(eio_obj_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);

    /* ain type cast. */
    eio_ain_t *ain = (eio_ain_t *)me;

    return ain->mv;
}

uint16_t eio_ain_get_average_value(eio_obj_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);

    /* ain type cast. */
    eio_ain_t *ain = (eio_ain_t *)me;
    uint32_t sum = 0;
    for (uint16_t i = 0; i < ain->buff_size; i ++)
    {
        sum += ain->buff[i];
    }

    return (uint16_t)(sum / ain->buff_size);
}

/* private function --------------------------------------------------------- */
static void _ain_poll(eio_obj_t * const me)
{
    EIO_ASSERT_NAME(me->ops != NULL, me->name);
    EIO_ASSERT_NAME(me->ops->poll != NULL, me->name);

    /* ain type cast. */
    eio_ain_t *ain = (eio_ain_t *)me;

    /* Polling */
    ain->mv = ain->ops->get_value(ain);
    ain->buff[ain->current ++] = ain->mv;
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
