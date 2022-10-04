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

/* private function prototype ----------------------------------------------- */
/* Interface functions. */
static eio_err_t _can_open(eio_obj_t * const me);
static eio_err_t _can_close(eio_obj_t * const me);

/* static function related with buffer. */
static uint8_t _buff_remaining(eio_can_buff_t * const buff);
static bool _buff_empty(eio_can_buff_t * const buff);

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);

/* private variables -------------------------------------------------------- */
static const struct eio_ops can_ops =
{
    _can_open,
    _can_close,
    NULL,
    NULL,
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

    /* Configure CAN port as default and disable the ISR. */
    const eio_can_config_t config_default =
    {
        (uint8_t)EIO_CAN_BAUDRATE_125K,
        (uint8_t)EIO_CAN_MODE_NORMAL,
    };
    me->ops->config(me, &config_default);
    me->config = config_default;
    me->ops->isr_enable(me, false);

    /* Set the ops of the super class. */
    me->super.ops = (struct eio_ops *)&can_ops;

    /* Register the can port to the EIO framwork. */
    can_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &can_obj_attribute);

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

    }
}

void eio_can_config(eio_obj_t * dev, eio_can_config_t *config)
{
    eio_can_t * can = (eio_can_t *)dev;
    can->config.baud_rate = config->baud_rate;
    can->config.mode = config->mode;
    can->ops->config(can, config);
}

void eio_can_send(eio_obj_t * dev, const eio_can_msg_t * msg)
{
    eio_can_t * can = (eio_can_t *)dev;
    int ret = m_queue_push(&can->queue_tx, (void *)msg, 1);
    M_ASSERT_INFO(ret == Queue_OK, "line: %d, ret: %d.", __LINE__, ret);
}

int eio_can_recv(eio_obj_t * dev, eio_can_msg_t * msg)
{
    eio_can_t * can = (eio_can_t *)dev;
    if (m_queue_empty(&can->queue_rx) == true)
        return eio_Err_Empty;

    devf_port_irq_disable();
    int ret = m_queue_pull_pop(&can->queue_rx, msg, 1);
    devf_port_irq_enable();
    M_ASSERT_INFO(ret == 1, "line: %d, ret: %d.", __LINE__, ret);

    return eio_OK;
}

void eio_can_set_evt(eio_obj_t * dev, int evt_recv, int evt_send_end)
{
    ((eio_can_t *)dev)->evt_recv = evt_recv;
    ((eio_can_t *)dev)->evt_send_end = evt_send_end;
}

void eio_can_add_filter(eio_obj_t * dev, eio_can_filter_t * filter)
{
    eio_can_t * can = (eio_can_t *)dev;

    M_ASSERT(can->count_filter >= CAN_FILTER_NUM);

    int i = can->count_filter;
    can->filter[i].id = filter->id;
    can->filter[i].mask = filter->mask;
    can->filter[i].ide = filter->ide;
    can->filter[i].mode = filter->mode;
    can->filter[i].rtr = filter->rtr;
}

void eio_can_config_filter(eio_obj_t * dev)
{
    eio_can_t * can = (eio_can_t *)dev;
    can->ops->config_filter(can);
}

void eio_can_clear_filter(eio_obj_t * dev)
{
    eio_can_t * can = (eio_can_t *)dev;
    can->count_filter = 0;
}

// private function ------------------------------------------------------------
static void eio_can_poll(eio_obj_t * dev, uint64_t time_system_ms)
{
    (void)time_system_ms;
    
    eio_can_t * can = (eio_can_t *)dev;
    can->send_busy = can->ops->send_busy(can);
    if (can->send_busy == true) {
        return;
    }
    
    eio_can_msg_t msg;
    int ret;
    while (m_queue_empty(&can->queue_tx) == false) {
        ret = m_queue_pull(&can->queue_tx, (void *)&msg, 1);
        M_ASSERT_INFO(ret == 1, "Can Queue_Tx Pull. Line: %d, ret: %d.", __LINE__, ret);
        if (can->ops->send(can, &msg) == false) {
            break;
        }
        ret = m_queue_pop(&can->queue_tx, 1);
        M_ASSERT_INFO(ret == 1, "Can Queue_Tx Pop. Line: %d, ret: %d.", __LINE__, ret);
    }
}

static eio_err_t eio_can_enable(eio_obj_t * dev, bool enable)
{
    eio_can_t * can = (eio_can_t *)dev;

    return can->ops->enable(can, enable);
}
