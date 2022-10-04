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

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);

/* private variables -------------------------------------------------------- */
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
    me->super.ops = NULL;

    /* Register the serial port to the EIO framwork. */
    pin_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &pin_obj_attribute);

    return EIO_OK;
}

void eio_pin_set_mode(eio_obj_t * const me, uint8_t mode)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(mode < EIO_PIN_MODE_MAX);

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
    EIO_ASSERT(me != NULL);

    /* pin type cast. */
    eio_pin_t *pin = (eio_pin_t *)me;
    
    /* Set mode. */
    EIO_ASSERT_NAME(pin->ops != NULL, me->name);
    EIO_ASSERT_NAME(pin->ops->get_status != NULL, me->name);
    pin->status = pin->ops->get_status(pin);

    return pin->status;
}

void eio_pin_set_status(eio_obj_t * const me, bool status)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);

    /* pin type cast. */
    eio_pin_t *pin = (eio_pin_t *)me;
    
    /* Set mode. */
    EIO_ASSERT_NAME(pin->ops != NULL, me->name);
    EIO_ASSERT_NAME(pin->ops->set_status != NULL, me->name);

    pin->status = status;
    pin->ops->set_status(pin, status);
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
