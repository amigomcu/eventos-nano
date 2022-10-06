/*
 * CAN bus, EventOS Input & Output Framework
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
 * 2022-10-04     Dog           the first version
 */

/* include ------------------------------------------------------------------ */
#include "eio_can.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* private function prototype ----------------------------------------------- */
/* Interface functions. */
static eio_err_t _can_open(eio_obj_t * const me);
static eio_err_t _can_close(eio_obj_t * const me);
static int32_t _can_read(eio_obj_t * const me,
                            uint32_t pos, void *buff, uint32_t size);
static int32_t _can_write(eio_obj_t * const me,
                            uint32_t pos, const void *buff, uint32_t size);

/* static function related with buffer. */
static uint8_t _buff_remaining(eio_can_buff_t * const buff);
static bool _buff_empty(eio_can_buff_t * const buff);

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);
extern void eio_callback_function_null(eio_obj_t * const me);

/* private variables -------------------------------------------------------- */
static const struct eio_ops can_ops =
{
    _can_open,
    _can_close,
    _can_read,
    _can_write,
    NULL,
};

static eio_obj_attribute_t can_obj_attribute =
{
    1000, EIO_TYPE_CAN, EIO_RT_LEVEL_SUPERLOOP, NULL
};

/* public function ---------------------------------------------------------- */
eio_err_t eio_can_register(eio_can_t * const me,
                            const char * name,
                            eio_can_attribute_t *attribute)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(me != NULL, name);
    EIO_ASSERT_NAME(attribute != NULL, name);
    EIO_ASSERT_NAME(me->ops != NULL, name);
    EIO_ASSERT_NAME(me->ops->config != NULL, name);
    EIO_ASSERT_NAME(me->ops->isr_enable != NULL, name);

    /* Set the can port attribute. */
    /* Sending buffer. */
    me->buff_send.msg = attribute->buff_send;
    me->buff_send.size = attribute->buff_size_send;
    me->buff_send.head = 0;
    me->buff_send.tail = 0;
    /* Receiving buffer. */
    me->buff_receive.msg = attribute->buff_receive;
    me->buff_receive.size = attribute->buff_size_receive;
    me->buff_receive.head = 0;
    me->buff_receive.tail = 0;

    /* Configure CAN bus as default and disable the ISR. */
    const eio_can_config_t config_default =
    {
        (uint8_t)EIO_CAN_BAUDRATE_125K,
        (uint8_t)EIO_CAN_MODE_NORMAL,
    };
    me->ops->config(me, (eio_can_config_t *)&config_default);
    me->config = config_default;
    me->ops->isr_enable(me, false);

    /* Set the ops of the super class. */
    me->super.ops = (struct eio_ops *)&can_ops;

    /* Register the CAN bus to the EIO framwork. */
    can_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &can_obj_attribute);

    /* Register events to the CAN bus object. */
    for (uint8_t i = 0; i < EIO_CAN_EVT_MAX; i ++)
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

void eio_can_isr_receive(eio_can_t * const me, eio_can_msg_t *msg)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(msg != NULL, me->super.name);
    EIO_ASSERT_NAME(me->ops != NULL, me->super.name);
    EIO_ASSERT_NAME(me->ops->isr_enable != NULL, me->super.name);

    /* Check the filter. */
    bool valid = false;
    if (me->ops->config_filter == NULL)         /* If soft filter. */
    {
        eio_can_filter_t *filter = me->filter_list;
        while (filter != NULL)
        {
            if ((filter->id & filter->mask) == (msg->id & filter->mask))
            {
                valid = true;
                break;
            }
            filter = filter->next;
        }
    }

    if (valid)
    {
        /* Push the CAN message into buffer. */
        eio_can_buff_t *buff = &me->buff_receive;
        me->ops->isr_enable(me, false);
        EIO_ASSERT_NAME(_buff_remaining(buff) > 0, me->super.name);
        buff->msg[buff->head] = *msg;
        buff->head = (buff->head + 1) % buff->size;
        me->ops->isr_enable(me, true);

        /* Send the event of message received. */
        eio_event_publish(&me->super, EIO_CAN_EVT_MESSAGE_RECEIVED);
    }
}

void eio_can_isr_send_end(eio_can_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(me->ops != NULL);
    EIO_ASSERT_NAME(me->ops->isr_enable != NULL);
    EIO_ASSERT_NAME(me->ops->send != NULL);

    while (1)
    {
        eio_can_buff_t *buff = &me->buff_send;
        if (_buff_empty(buff))
        {
            me->sending = false;
            break;
        }

        /* Get one message from the sending buffer and send it. */
        me->ops->send(me, &buff->msg[buff->tail]);
        buff->tail = (buff->tail + 1) % buff->size;

        /* Check the CAN bus is busy. */
        if (me->ops->send_busy == NULL)
        {
            break;
        }
        if (me->ops->send_busy(me))
        {
            break;
        }
    }
}

void eio_can_config(eio_obj_t * const me, eio_can_config_t *config)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(config != NULL, me->name);

    /* CAN type cast. */
    eio_can_t *can = (eio_can_t *)me;
    EIO_ASSERT_NAME(can->ops != NULL, me->name);
    EIO_ASSERT_NAME(can->ops->config != NULL, me->name);

    /* If configurations of CAN bus are changed, */
    if (memcmp(config, &can->config, sizeof(eio_can_config_t)) != 0)
    {
        /* Configure the CAN bus. */
        can->ops->config(can, config);
        memcpy(&can->config, config, sizeof(eio_can_config_t));
    }
}

void eio_can_send(eio_obj_t * const me, const eio_can_msg_t *msg)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(msg != NULL);
    
    /* CAN type cast. */
    eio_can_t *can = (eio_can_t *)me;
    EIO_ASSERT_NAME(can->ops != NULL, me->name);

    /* Check the remaining buffer memory space is enough. */
    eio_can_buff_t *buffer = &can->buff_send;
    uint16_t remaining = _buff_remaining(buffer);
    EIO_ASSERT(remaining >= 1);

    /* Put the data into buffer. */
    buffer->msg[buffer->head] = *msg;
    buffer->head = (buffer->head + 1) % buffer->size;

    /* If not sending, send the 1st message. */
    if (!can->sending)
    {
        eio_can_isr_send_end(can);
    }
}

bool eio_can_receive(eio_obj_t * const me, eio_can_msg_t *msg)
{
    bool ret = false;

    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(msg != NULL);

    /* CAN type cast. */
    eio_can_t *can = (eio_can_t *)me;
    EIO_ASSERT_NAME(can->ops != NULL, me->name);

    /* Check the remaining buffer memory space is enough. */
    eio_can_buff_t *buffer = &can->buff_receive;

    /* Calculate the return size and get data. */
    ret = !_buff_empty(buffer);
    if (ret)
    {
        /* Get the data from buffer. */
        *msg = buffer->msg[buffer->tail];
        buffer->tail = (buffer->tail + 1) % buffer->size;
    }

    return ret;
}

eio_err_t eio_can_config_filter(eio_obj_t * const me, eio_can_filter_t *filter)
{
    eio_err_t ret = EIO_OK;

    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(filter != NULL);

    /* CAN type cast. */
    eio_can_t *can = (eio_can_t *)me;
    EIO_ASSERT_NAME(can->ops != NULL, me->name);

    /* Set the hardware CAN filter. */
    if (can->ops->config_filter != NULL)
    {
        ret = can->ops->config_filter(can, filter);
    }

    /* Add to the list. */
    if (ret == EIO_OK)
    {
        filter->next = can->filter_list;
        can->filter_list = filter;
    }

    return ret;
}

/* private function --------------------------------------------------------- */
static uint8_t _buff_remaining(eio_can_buff_t * const buff)
{
    return 0;
}

static bool _buff_empty(eio_can_buff_t * const buff)
{
    return 0;
}

static eio_err_t _can_enable(eio_obj_t * const me, bool status)
{
    /* can type cast. */
    eio_can_t *can = (eio_can_t *)me;
    EIO_ASSERT_NAME(can->ops != NULL, me->name);
    EIO_ASSERT_NAME(can->ops->isr_enable != NULL, me->name);

    /* Enable the interrupt. */
    can->ops->isr_enable(can, status);

    return EIO_OK;
}

static eio_err_t _can_open(eio_obj_t * const me)
{
    return _can_enable(me, true);
}

static eio_err_t _can_close(eio_obj_t * const me)
{
    return _can_enable(me, false);
}

static int32_t _can_read(eio_obj_t * const me,
                            uint32_t pos, void *buff, uint32_t size)
{
    (void)pos;

    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME((size % sizeof(eio_can_msg_t)) == 0, me->name);
    EIO_ASSERT_NAME(size != 0, me->name);
    
    for (uint32_t i = 0; i < (size / sizeof(eio_can_msg_t)); i ++)
    {
        eio_can_receive(me, (buff + sizeof(eio_can_msg_t) * i));
    }
    
    return size;
}

static int32_t _can_write(eio_obj_t * const me,
                            uint32_t pos, const void *buff, uint32_t size)
{
    (void)pos;

    /* Check the parameters are valid or not. */
    EIO_ASSERT_NAME((size % sizeof(eio_can_msg_t)) == 0, me->name);
    EIO_ASSERT_NAME(size != 0, me->name);
    
    for (uint32_t i = 0; i < (size / sizeof(eio_can_msg_t)); i ++)
    {
        eio_can_send(me, (buff + sizeof(eio_can_msg_t) * i));
    }
    
    return size;
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
