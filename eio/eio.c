/*
 * EventOS Input & Output Framework
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
#include <stdlib.h>
#include <string.h>
#if (EIO_EVENT_MODE == 0)
#include "eventos.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* private typedef ---------------------------------------------------------- */
static eio_obj_t *list[EIO_RT_LEVEL_MAX] =
{
    NULL, NULL,
};
static uint32_t time_eio_ms = UINT32_MAX;
static uint16_t time_eio_us = UINT16_MAX;

/* public function ---------------------------------------------------------- */
void eio_poll(uint8_t rt_level)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(rt_level < EIO_RT_LEVEL_MAX);

    /* Get the time for the 1st time. */
    if (time_eio_ms == UINT32_MAX)
    {
        time_eio_ms = eio_port_system_time();
        return;
    }

    /* Time adjusting, and clear us-level time counting. */
    if (rt_level == EIO_RT_LEVEL_US && time_eio_us == UINT16_MAX)
    {
        if (time_eio_ms != eio_port_system_time())
        {
            time_eio_us = 0;
        }
        else
        {
            return;
        }
    }

    /* The non-realtime polling is executed while time_ms changes. */
    /* The beginning section of time is ignored. */
    if (rt_level == EIO_RT_LEVEL_MS &&
        (time_eio_ms == eio_port_system_time()) ||
        (time_eio_ms <= EIO_TIME_START_IGNORE))
    {
        return;
    }

    /* All the IO object are all accessed. */
    eio_obj_t *obj = list[rt_level];
    while (obj != NULL)
    {
        /* Prevent the object block being covered in a wrong way. */
        EIO_ASSERT_NAME(obj->magic == EIO_MAGIC_NUMBER, obj->name);

        if (obj->enabled && obj->ops->poll != NULL)
        {
            uint32_t time = eio_port_system_time();
            while (time >= obj->time_next)
            {
                obj->time_next += obj->attribute.period;
                if (obj->ops != NULL && obj->ops->poll != NULL)
                {
                    obj->ops->poll(obj);
                }
            }
        }

        obj = obj->next;
    }
}

eio_obj_t *eio_find(const char *name)
{
    eio_obj_t *ret_obj = NULL;

    for (uint8_t i = 0; i < EIO_RT_LEVEL_MAX; i++)
    {
        eio_obj_t *obj = list[i];
        while (obj != NULL)
        {
            if (strcmp(obj->name, name) == 0)
            {
                ret_obj = obj;
                break;
            }
            obj = obj->next;
        }
        if (ret_obj != NULL)
        {
            break;
        }
    }

    return ret_obj;
}

void eio_register(eio_obj_t * const me,
                    const char *name,
                    eio_obj_attribute_t *attribute)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(attribute != NULL, name);
    EIO_ASSERT_NAME(attribute->rt_level < EIO_RT_LEVEL_MAX, name);
    EIO_ASSERT_NAME(attribute->type < EIO_TYPE_MAX, name);
    EIO_ASSERT_NAME(attribute->period != 0, name);

    /* Check the name is existent in the list. */
    eio_obj_t *obj = eio_find(name);
    EIO_ASSERT_NAME(obj == NULL, name);

    /* Initialize the IO object block. */
    me->name = name;
    me->magic = EIO_MAGIC_NUMBER;
    memcpy(&me->attribute, attribute, sizeof(eio_obj_attribute_t));
    me->enabled = false;

    /* Disable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(attribute->rt_level, false);

    /* Add to the list. */
    me->next = list[attribute->rt_level];
    list[attribute->rt_level] = me;

    /* Enable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(attribute->rt_level, true);
}

void eio_attach_event(eio_obj_t * const me, eio_event_t *e)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(e != NULL, me->name);

    /* Disable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, false);

    /* Check the event ID is existent in the list. */
    eio_event_t *evt = me->elist;
    while (evt != NULL)
    {
        EIO_ASSERT_NAME(evt->eio_event_id != e->eio_event_id);
        evt = evt->next;
    }

    /* Add to the event list. */
    e->next = me->elist;
    me->elist = e->next;

    /* Enable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, true);
}

void eio_event_publish(eio_obj_t * const me, uint8_t eio_event_id)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(me != NULL);

    /* Disable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, false);

    /* Check the event ID is existent in the list. */
    eio_event_t *evt = me->elist;
    while (evt != NULL)
    {
        if (evt->eio_event_id == eio_event_id)
        {
#if (EIO_EVENT_MODE == 0)
            eos_event_pub_topic(evt->e_topic);
#else
            EIO_ASSERT_NAME(evt->callback != NULL);
            evt->callback(me);
#endif
            break;
        }
        evt = evt->next;
    }

    /* Enable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, true);
}

#if (EIO_EVENT_MODE == 0)
void eio_event_set_topic(eio_obj_t * const me,
                            uint8_t eio_event_id, uint32_t topic)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(me != NULL);

    /* Disable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, false);

    /* Check the event ID is existent in the list. */
    eio_event_t *evt = me->elist;
    while (evt != NULL)
    {
        if (evt->eio_event_id == eio_event_id)
        {
            evt->e_topic = topic;
            break;
        }
        evt = evt->next;
    }

    /* Enable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, true);
}
#else
void eio_event_set_callback(eio_obj_t * const me,
                            uint8_t eio_event_id,
                            eio_callback_t callback)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(callback != NULL);

    /* Disable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, false);

    /* Check the event ID is existent in the list. */
    eio_event_t *evt = me->elist;
    while (evt != NULL)
    {
        if (evt->eio_event_id == eio_event_id)
        {
            evt->callback = callback;
            break;
        }
        evt = evt->next;
    }

    /* Enable the interrupt of the polling timer ISR. */
    eio_port_rt_isr_enable(me->attribute.rt_level, true);
}
#endif

eio_err_t eio_open(eio_obj_t * const me)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(me != NULL);

    eio_err_t ret = EIO_OK;

    if (me->enabled == false)
    {
        EIO_ASSERT_NAME(me->ops != NULL, me->name);
        EIO_ASSERT_NAME(me->ops->open != NULL, me->name);
        me->enabled = true;
        ret = me->ops->open(me);
    }

    return ret;
}

eio_err_t eio_close(eio_obj_t * const me)
{
    /* Check the parameters are valid. */
    EIO_ASSERT(me != NULL);

    eio_err_t ret = EIO_OK;

    if (me->enabled == true)
    {
        EIO_ASSERT_NAME(me->ops != NULL, me->name);
        EIO_ASSERT_NAME(me->ops->close != NULL, me->name);
        me->enabled = false;
        ret = me->ops->close(false);
    }

    return ret;
}

int32_t eio_read(eio_obj_t *me, uint32_t pos, void *buff, uint32_t size)
{
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(buff != NULL);

    int32_t ret = EIO_OK;

    if (me->enabled == false)
    {
        ret = EIO_ERR_DISABLED;
    }
    else
    {
        EIO_ASSERT_NAME(me->ops != NULL, me->name);
        EIO_ASSERT_NAME(me->ops->read != NULL, me->name);
        ret = me->ops->read(me, pos, buff, size);
    }

    return ret;
}

int32_t eio_write(eio_obj_t *me, uint32_t pos, const void *buff, uint32_t size)
{
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(buff != NULL);

    int32_t ret = EIO_OK;

    if (me->enabled == false)
    {
        ret = EIO_ERR_DISABLED;
    }
    else
    {
        EIO_ASSERT_NAME(me->ops != NULL, me->name);
        EIO_ASSERT_NAME(me->ops->write != NULL, me->name);
        ret = me->ops->write(me, pos, buff, size);
    }

    return ret;
}

void eio_get_time(eio_time_t *time)
{
    time->ms = time_eio_ms;
    time->us = time_eio_us;
}

uint32_t eio_time_diff_ms(eio_time_t *time_current, eio_time_t *time_last)
{
    uint64_t _time_current = time_current->ms * 1000 + time_current->us;
    uint64_t _time_last = time_last->ms * 1000 + time_last->us;

    return (uint32_t)((_time_current - _time_last) / 1000);
}

uint32_t eio_time_diff_us(eio_time_t *time_current, eio_time_t *time_last)
{
    uint64_t _time_current = time_current->ms * 1000 + time_current->us;
    uint64_t _time_last = time_last->ms * 1000 + time_last->us;

    return (uint32_t)((_time_current - _time_last));
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
