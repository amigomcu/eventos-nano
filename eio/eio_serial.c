/*
 * Serial Port, EventOS Input & Output Framework
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
#include "eio_serial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* private define ----------------------------------------------------------- */


/* private typedef ---------------------------------------------------------- */

/* private function prototype ----------------------------------------------- */
/* Interface functions. */
static eio_err_t _serial_open(eio_obj_t * const me);
static eio_err_t _serial_close(eio_obj_t * const me);
static void _serial_poll(eio_obj_t * const me);

/* static function related with buffer. */
static uint16_t _buff_remaining(eio_serial_buff_t *buff);
static bool _buff_empty(eio_serial_buff_t *buff);

/* static function related with threshold for T1.5 and T3.5. */
static void _set_threshold_t15_t35(eio_serial_t * const me,
                                    eio_serial_config_t *config);

/* public function prototype ------------------------------------------------ */
/* extern function in EIO framework. */
extern void eio_register(eio_obj_t * const me,
                            const char *name,
                            eio_obj_attribute_t *attribute);

/* private variables -------------------------------------------------------- */
static const struct eio_ops serial_ops =
{
    _serial_open,
    _serial_close,
    NULL,
    NULL,
    _serial_poll,
};

static eio_obj_attribute_t serial_obj_attribute =
{
    100, EIO_TYPE_SERIAL, EIO_RT_LEVEL_US, NULL
};

/* public function ---------------------------------------------------------- */
eio_err_t eio_serial_register(eio_serial_t * const me,
                                const char *name,
                                eio_serial_attribute_t *attribute)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(me != NULL, name);
    EIO_ASSERT_NAME(attribute != NULL, name);
    EIO_ASSERT_NAME(me->ops != NULL, name);
    EIO_ASSERT_NAME(me->ops->config != NULL, name);

    /* Set the serial port attribute. */
    /* buffer tx */
    me->buff_tx.data = attribute->buff_tx;
    me->buff_tx.size = attribute->buff_size_tx;
    me->buff_tx.head = 0;
    me->buff_tx.tail = 0;
    /* buffer rx. */
    me->buff_rx.data = attribute->buff_rx;
    me->buff_rx.size = attribute->buff_size_rx;
    me->buff_rx.head = 0;
    me->buff_rx.tail = 0;
    /* RS485 related. */
    me->rs485 = attribute->rs485;
    me->sending = false;

    /* Configure serial port as default and disable the ISR. */
    me->ops->config(me, &(eio_serial_config_t)EIO_SERIAL_CONFIG_DEFAULT);
    me->ops->isr_enable(me, false);

    /* Calculate the bits threshold of T3.5 and T1.5. */
    _set_threshold_t15_t35(me, &(eio_serial_config_t)EIO_SERIAL_CONFIG_DEFAULT);

    /* Set the ops of the super class. */
    me->super.ops = (struct eio_ops *)&serial_ops;

    /* Register the serial port to the EIO framwork. */
    serial_obj_attribute.user_data = attribute->user_data;
    eio_register(&me->super, name, &serial_obj_attribute);

    return EIO_OK;
}

void eio_serial_isr_receive(eio_serial_t * const me, uint8_t byte)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(me->ops != NULL);
    EIO_ASSERT_NAME(me->ops->isr_enable != NULL);

    /* When serial is RS232, or when serial is RS485 and not sending, */
    if (!(me->rs485 && me->sending))
    {
        eio_serial_buff_t *buff = &me->buff_rx;

        me->ops->isr_enable(me, false);
        EIO_ASSERT_NAME(_buff_remaining(buff) > 0, me->super.name);
        buff->data[buff->head] = byte;
        buff->head = (buff->head + 1) % buff->size;
        me->ops->isr_enable(me, true);

        /* Update the receiving time. */
        eio_get_time(&me->time_receive);
        me->event_t15_sent = false;
        me->event_t35_sent = false;
    }
}

void eio_serial_isr_send_end(eio_serial_t * const me)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(me->ops != NULL);
    EIO_ASSERT_NAME(me->ops->isr_enable != NULL);
    EIO_ASSERT_NAME(me->ops->send != NULL);

    /* When serial is RS232, or when serial is RS485 and sending, */
    if (!(me->rs485 && !me->sending))
    {
        eio_serial_buff_t *buff = &me->buff_tx;
        uint8_t byte = 0;
        bool flag_send = false;

        me->ops->isr_enable(me, false);
        if (_buff_remaining(buff) > 0)
        {
            flag_send = true;
            byte = buff->data[buff->tail];
            buff->tail = (buff->tail + 1) % buff->size;
        }
        else
        {
            /* If RS485, set the rx state. */
            if (me->rs485)
            {
                EIO_ASSERT_NAME(me->ops->set_tx_state != NULL, me->super.name);
                me->ops->set_tx_state(me, false);
                me->sending = true;
            }
        }
        me->ops->isr_enable(me, true);
        if (flag_send)
        {
            me->ops->send(me, byte);
        }
    }
}

void eio_serial_config(eio_obj_t * const me, eio_serial_config_t *config)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT_NAME(config != NULL, me->name);

    /* serial type cast. */
    eio_serial_t *serial = (eio_serial_t *)me;
    EIO_ASSERT_NAME(serial->ops != NULL, me->name);
    EIO_ASSERT_NAME(serial->ops->config != NULL, me->name);

    /* Configure the serial port. */
    serial->ops->config(serial, config);

    /* Calculate the bits threshold of T3.5 and T1.5. */
    _set_threshold_t15_t35(serial, config);
}

int32_t eio_serial_write(eio_obj_t * const me, const void *buff, uint32_t size)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(buff != NULL);
    EIO_ASSERT(size > 0 && size < UINT16_MAX);
    
    /* serial type cast. */
    eio_serial_t *serial = (eio_serial_t *)me;
    EIO_ASSERT_NAME(serial->ops != NULL, me->name);

    /* If RS485, set the tx mode. */
    if (serial->rs485 && !serial->sending)
    {
        EIO_ASSERT_NAME(serial->ops->set_tx_state != NULL, me->name);
        serial->ops->set_tx_state(serial, true);
        serial->sending = true;
    }

    /* Check the remaining buffer memory space is enough. */
    eio_serial_buff_t *buffer = &serial->buff_tx;
    uint16_t remaining = _buff_remaining(buffer);
    EIO_ASSERT(remaining >= size);

    /* Put the data into buffer. */
    for (uint32_t i = 0; i < size; i ++)
    {
        buffer->data[buffer->head] = ((uint8_t *)buff)[i];
        buffer->head = (buffer->head + 1) % buffer->size;
    }

    return size;
}

int32_t eio_serial_read(eio_obj_t * const me, void *buff, uint32_t size)
{
    /* Check the parameters are valid or not. */
    EIO_ASSERT(me != NULL);
    EIO_ASSERT(buff != NULL);
    EIO_ASSERT(size > 0 && size < UINT16_MAX);

    /* serial type cast. */
    eio_serial_t *serial = (eio_serial_t *)me;
    EIO_ASSERT_NAME(serial->ops != NULL, me->name);

    /* Check the remaining buffer memory space is enough. */
    eio_serial_buff_t *buffer = &serial->buff_rx;
    uint16_t remaining = _buff_remaining(buffer);

    /* Calculate the return size and get data. */
    int32_t ret_size = remaining > size ? size : remaining;
    if (remaining > 0)
    {
        /* Get the data from buffer. */
        for (int32_t i = 0; i < ret_size; i ++)
        {
            ((uint8_t *)buff)[i] = buffer->data[buffer->tail];
            buffer->tail = (buffer->tail + 1) % buffer->size;
        }
    }

    return ret_size;
}

/* private function --------------------------------------------------------- */
static void _set_threshold_t15_t35(eio_serial_t * const me,
                                    eio_serial_config_t *config)
{
    if (config->baud_rate > 19200)
    {
        me->threshold_t15 = 750;                    /* 750us */
        me->threshold_t35 = 1750;                   /* 1750us */
    }
    else
    {
        /* Calculate the bits number of one byte. */
        /* Start bit, data bits, stop bits and parity bits. */
        uint8_t byte_bits = 1 + config->data_bits + (1 + config->stop_bits);
        if (config->parity != EIO_SERIAL_PARITY_NONE)
        {
            byte_bits ++;
        }

        /* 1000000 / (baud_rate / byte_bits) * 1.5 */
        me->threshold_t15 = 1000000 * byte_bits * 3 / config->baud_rate / 2;
        /* 1000000 / (baud_rate / byte_bits) * 3.5 */
        me->threshold_t15 = 1000000 * byte_bits * 7 / config->baud_rate / 2;
    }
}

static uint16_t _buff_remaining(eio_serial_buff_t *buff)
{
    return 0;
}

static bool _buff_empty(eio_serial_buff_t *buff)
{
    return 0;
}

static eio_err_t _serial_enable(eio_obj_t * const me, bool status)
{
    /* Serial type cast. */
    eio_serial_t *serial = (eio_serial_t *)me;
    EIO_ASSERT_NAME(serial->ops != NULL, me->name);
    EIO_ASSERT_NAME(serial->ops->isr_enable != NULL, me->name);

    /* Enable the interrupt. */
    serial->ops->isr_enable(serial, status);

    return EIO_OK;
}

static eio_err_t _serial_open(eio_obj_t * const me)
{
    return _serial_enable(me, true);
}

static eio_err_t _serial_close(eio_obj_t * const me)
{
    return _serial_enable(me, false);
}

static void _serial_poll(eio_obj_t * const me)
{
    /* Serial type cast. */
    eio_serial_t *serial = (eio_serial_t *)me;

    /* When data received, Get the difference between the current time and last
       time. */
    if (!serial->event_t15_sent && !serial->event_t35_sent)
    {
        eio_time_t time_current;
        uint32_t time_diff;
        if (!_buff_empty(&serial->buff_rx))
        {
            eio_get_time(&time_current);
            time_diff = eio_time_diff_us(&time_current, &serial->time_receive);
            if (time_diff >= serial->threshold_t15 && !serial->event_t15_sent)
            {
                eio_event_publish(me, EIO_SERIAL_EVT_RECEIVE_T15);
                serial->event_t15_sent = true;
            }
            if (time_diff >= serial->threshold_t35 && !serial->event_t35_sent)
            {
                eio_event_publish(me, EIO_SERIAL_EVT_RECEIVE_T35);
                serial->event_t35_sent = true;
            }
        }
    }
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
