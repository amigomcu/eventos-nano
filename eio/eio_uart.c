/*
 * UART, EventOS Input & Output Framework
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
#include "eio_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* private define ----------------------------------------------------------- */


/* private typedef ---------------------------------------------------------- */
static eio_err_t _uart_open(eio_obj_t * const me);
static eio_err_t _uart_close(eio_obj_t * const me);
static eio_err_t _uart_poll(eio_obj_t * const me);

/* public function ---------------------------------------------------------- */
eio_err_t eio_uart_reg(eio_uart_t * const uart,
                        const char *name,
                        eio_uart_attribute_t *attribute)
{
    EIO_ASSERT(uart != NULL);
    EIO_ASSERT(name != NULL);
    EIO_ASSERT_NAME(attribute != NULL, name);
}

void eio_uart_isr_receive(eio_uart_t * const uart, uint8_t byte)
{

}

void eio_uart_isr_send_end(eio_uart_t * const uart)
{

}

void eio_uart_config(eio_obj_t * const me, eio_uart_config_t *config)
{

}

int32_t eio_uart_write(eio_obj_t * const me, const void *buff, uint32_t size)
{

}

int32_t eio_uart_read(eio_obj_t * const me, void *buff, uint32_t size)
{

}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
