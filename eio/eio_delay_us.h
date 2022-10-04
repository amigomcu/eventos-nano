/*
 * Us-level delay, EventOS Input & Output Framework
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
 * 2022-10-02     Dog           the first version
 */

#ifndef __EIO_DELAY_US_H__
#define __EIO_DELAY_US_H__

/* include ------------------------------------------------------------------ */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* public function ---------------------------------------------------------- */
void eio_delay_us(uint16_t time_us);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_PIN_H__ */

/* ----------------------------- end of file -------------------------------- */