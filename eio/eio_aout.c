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
 * 2022-10-07     Dog           the first version
 */

/* include ------------------------------------------------------------------ */
#include "eio_aout.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);

/* private variables -------------------------------------------------------- */
static eio_obj_attribute_t aout_obj_attribute =
{
    0, EIO_TYPE_AOUT, EIO_RT_LEVEL_100US, NULL
};

/* public function ---------------------------------------------------------- */
eio_err_t eio_aout_register(eio_aout_t * const me,
                            const char *name,
                            eio_aout_attribute_t *attribute)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(me != NULL, name);
    EIO_ASSERT_NAME(attribute != NULL, name);

    /* Set the aout attribute. */
    EIO_ASSERT_NAME(me->ops != NULL, name);
    EIO_ASSERT_NAME(me->ops->set_value != NULL, name);

    /* Set the initial value for the output pin. */
    me->mv = attribute->mv_init;
    me->ops->set_value(me, me->mv);

    /* Register the Analog Output object to the EIO framwork. */
    me->super.ops = NULL;
    aout_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &aout_obj_attribute);

    return EIO_OK;
}

void eio_aout_set_peroid(eio_obj_t * const me, uint32_t time_us)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME(me != NULL, me->name);
    me->attribute.period = time_us;
}

void eio_aout_set_value(eio_obj_t * const me, uint16_t mv)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);

    /* aout type cast. */
    eio_aout_t *aout = (eio_aout_t *)me;
    aout->ops->set_value(aout, mv);
    aout->mv = mv;
}

uint16_t eio_aout_get_value(eio_obj_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);

    /* aout type cast. */
    eio_aout_t *aout = (eio_aout_t *)me;

    return aout->mv;
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
