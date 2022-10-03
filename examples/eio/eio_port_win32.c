/*
 * The port functions of EIO framework on WIN32 platform.
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

/* include ------------------------------------------------------------------ */
#include "eio.h"
#include <stdlib.h>

/* public function ---------------------------------------------------------- */
uint32_t eio_port_system_time(void)
{
    return 0;
}

void eio_port_rt_isr_enable(uint8_t rt_level, bool enable)
{
    (void)rt_level;
    (void)enable;
}

void *eio_port_malloc(uint32_t size)
{
    return malloc(size);
}

void eio_port_free(void *memory)
{
    free(memory);
}

/* ----------------------------- end of file -------------------------------- */
