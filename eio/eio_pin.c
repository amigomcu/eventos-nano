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
 * 2022-10-01     Dog           the first version
 */

/* include ------------------------------------------------------------------ */
#include "eio.h"
#include "eio_pin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* private function prototype ----------------------------------------------- */
/* Interface functions. */
static void _pin_poll(eio_obj_t * const me);

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);
extern void eio_callback_function_null(eio_obj_t * const me);

/* private variables -------------------------------------------------------- */
static const struct eio_ops pin_ops =
{
    NULL,
    NULL,
    NULL,
    NULL,
    _pin_poll,
};

static eio_obj_attribute_t pin_obj_attribute =
{
    0, EIO_TYPE_PIN, EIO_RT_LEVEL_MS, NULL
};

/* public function ---------------------------------------------------------- */
eio_err_t eio_pin_register(eio_pin_t * const me,
                            const char *name,
                            eio_pin_attribute_t *attribute)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(me != NULL, name);
    EIO_ASSERT_NAME(attribute != NULL, name);
    EIO_ASSERT_NAME(me->ops != NULL, name);
    EIO_ASSERT_NAME(me->ops->set_mode != NULL, name);
    
    /* Set the pin attribute. */
    me->mode = attribute->mode;

    /* Set pin mode and set the default status if the pin is output mode. */
    me->ops->set_mode(me, me->mode);
    if (me->mode == EIO_PIN_MODE_OUT_OD || me->mode == EIO_PIN_MODE_OUT_PP)
    {
        EIO_ASSERT_NAME(me->ops->set_status != NULL, name);
        me->ops->set_status(me, attribute->status_default);
    }

    /* Set the ops of the super class. */
    if (me->mode == EIO_PIN_MODE_OUT_OD || me->mode == EIO_PIN_MODE_OUT_PP)
    {
        me->super.ops = NULL;
    }
    else
    {
        me->super.ops = (struct eio_ops *)&pin_ops;
        EIO_ASSERT_NAME(me->ops->get_status != NULL, name);

        /* Read the initial status for the input pin. */
        me->status = me->ops->get_status(me);
        me->status_backup = me->status;
        for (uint8_t i = 0; i < EIO_PIN_FILTER_DEPTH; i ++)
        {
            me->status_filter[i] = me->status;
        }
        me->index_filter = 0;
    }

    /* Register the PIN to the EIO framwork. */
    pin_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &pin_obj_attribute);

    /* Register events to the PIN object. */
    for (uint8_t i = 0; i < EIO_PIN_EVT_MAX; i ++)
    {
        me->events[i].next = NULL;
#if (EIO_EVENT_MODE == 0)
        me->events[i].e_topic = 0;
#else
        me->events[i].callback = eio_callback_function_null;
        me->events[i].eio_event_id = i;
#endif
        eio_attach_event(&me->super, &me->events[i]);
    }

    return EIO_OK;
}

void eio_pin_set_mode(eio_obj_t * const me, uint8_t mode)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->name);
    EIO_ASSERT_NAME(mode < EIO_PIN_MODE_MAX, me->name);

    /* pin type cast. */
    eio_pin_t *pin = (eio_pin_t *)me;
    
    /* Set mode. */
    EIO_ASSERT_NAME(pin->ops != NULL, me->name);
    EIO_ASSERT_NAME(pin->ops->set_mode != NULL, me->name);

    pin->mode = mode;
    pin->ops->set_mode(pin, mode);
}

bool eio_pin_get_status(eio_obj_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->name);

    /* pin type cast. */
    eio_pin_t *pin = (eio_pin_t *)me;

    return pin->status;
}

void eio_pin_set_status(eio_obj_t * const me, bool status)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->name);

    /* pin type cast. */
    eio_pin_t *pin = (eio_pin_t *)me;
    
    /* Set mode. */
    EIO_ASSERT_NAME(pin->ops != NULL, me->name);
    EIO_ASSERT_NAME(pin->ops->set_status != NULL, me->name);

    pin->status = status;
    pin->ops->set_status(pin, status);
}

/* private function --------------------------------------------------------- */
static void _pin_poll(eio_obj_t * const me)
{
    /* PIN type cast. */
    eio_pin_t *pin = (eio_pin_t *)me;
    EIO_ASSERT_NAME(pin->ops->get_status != NULL, me->name);

    /* Debounce. */
    pin->status_filter[pin->index_filter] = pin->ops->get_status(pin);
    pin->index_filter = (pin->index_filter + 1) % EIO_PIN_FILTER_DEPTH;
    bool stable = true;
    for (uint8_t i = 0; i < (EIO_PIN_FILTER_DEPTH - 1); i ++)
    {
        if (pin->status_filter[i] != pin->status_filter[i + 1])
        {
            stable = false;
            break;
        }
    }
    if (stable)
    {
        pin->status = pin->status_filter[0];
    }

    /* If events are attached, */
    if (!eio_event_list_empty(me))
    {
        if (pin->status != pin->status_backup)
        {
            pin->status_backup = pin->status;

            if (pin->status == true)
            {
                eio_event_publish(me, EIO_PIN_EVT_HIGH_EDGE);
            }
            else
            {
                eio_event_publish(me, EIO_PIN_EVT_LOW_EDGE);
            }
        }
    }
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
