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

#ifndef __EIO_H__
#define __EIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* include ------------------------------------------------------------------ */
#include <stdbool.h>
#include <stdint.h>

/* config ------------------------------------------------------------------- */
#define EIO_EVENT_MODE                  (1)             /* 0: Event, 1: Callback */
#define EIO_ENABLE_ASSERT               (1)

/* At the MCU starting time, the eio polling is disabled. */
#define EIO_TIME_START_IGNORE           (10)

/* assert ------------------------------------------------------------------- */
#if (EIO_ENABLE_ASSERT != 0)
#include <assert.h>
#define EIO_ASSERT                      assert          /* See Note 1. */
#define EIO_ASSERT_NAME
#else
#define EIO_ASSERT(test_)               ((void)0)
#define EIO_ASSERT_NAME(test_, name_)   ((void)0)
#endif

/* public define ------------------------------------------------------------ */
#define EIO_MAGIC_NUMBER                (0xdeadbeef)

enum
{
    EIO_RT_LEVEL_MS = 0,
    EIO_RT_LEVEL_US,

    EIO_RT_LEVEL_MAX
};

enum eio_type
{
    EIO_TYPE_PIN = 0,
    EIO_TYPE_SERIAL,
    EIO_TYPE_AIN,
    EIO_TYPE_AOUT,
    EIO_TYPE_PWM,
    EIO_TYPE_CAN,
    EIO_TYPE_SPI,
    EIO_TYPE_I2C,
    EIO_TYPE_SIMU_UART,                 /* Simulated uart */
    EIO_TYPE_SIMU_I2C,                  /* Simulated uart */
    EIO_TYPE_EEPROM,
    EIO_TYPE_FLASH,
    EIO_TYPE_1WIRE,

    EIO_TYPE_MAX
};

typedef enum eio_err_tag
{
    EIO_OK              = 0,            /* There is no error */
    EIO_ERROR           = -1,           /* A generic error happens */
    EIO_ERR_TIMEOUT     = -2,           /* Timed out */
    EIO_ERR_FULL        = -3,           /* The resource is full */
    EIO_ERR_EMPTY       = -4,           /* The resource is empty */
    EIO_ERR_BUSY        = -5,           /* Busy */
    EIO_ERR_WrongArg    = -6,           /* Wrong Arguments */
    EIO_ERR_WRONG_CMD   = -7,           /* Wrong Cmd */
    EIO_ERR_EndlessLoop = -8,           /* Endless Loop */
    EIO_ERR_REPEATED    = -9,           /* Repeated */
    EIO_ERR_NonExistent = -10,          /* Not existent */
    EIO_ERR_DISABLED    = -11,          /* Disabled */
    EIO_ERR_OPEN_FAIL   = -12,          /* Opening failure */
} eio_err_t;

/* public typedef ----------------------------------------------------------- */
struct eio_obj;
typedef void (* eio_callback_t)(struct eio_obj *const me);

typedef struct eio_time
{
    uint32_t ms;
    uint16_t us;
} eio_time_t;

typedef struct eio_obj_attribute
{
    uint32_t period;
    uint8_t type;                       /* IO object type, see enum eio_type. */
    uint8_t rt_level;                   /* Realtime level, see enum eio_rt_level. */
    void *user_data;
} eio_obj_attribute_t;

typedef struct eio_event
{
    struct eio_event *next;
#if (EIO_EVENT_MODE == 0)
    uint32_t e_topic;
#else
    eio_callback_t callback;
#endif
    uint8_t eio_event_id;
} eio_event_t;

typedef struct eio_obj
{
    uint32_t magic;

    /* public */
    struct eio_obj *next;
    eio_event_t *elist;
    const char *name;
    eio_obj_attribute_t attribute;

    /* private */
    uint32_t time_next;                 /* The time for polling next time. */
    uint16_t time_us_count;             /* Us level time counting. */
    bool enabled;

    /* interface */
    struct eio_ops *ops;
} eio_obj_t;

struct eio_ops
{
    eio_err_t (* open)(eio_obj_t * const me);
    eio_err_t (* close)(eio_obj_t * const me);
    int32_t (* read)(eio_obj_t * const me,
                        uint32_t pos, void *buff, uint32_t size);
    int32_t (* write)(eio_obj_t * const me,
                        uint32_t pos, const void *buff, uint32_t size);
    void (* poll)(eio_obj_t *me);
};

/* public function ---------------------------------------------------------- */
/* EIO basic functions. */
void eio_poll(uint8_t rt_level);
eio_obj_t *eio_find(const char *name);

/* Event related functions. */
void eio_attach_event(eio_obj_t * const me, eio_event_t *e);
void eio_event_publish(eio_obj_t * const me, uint8_t eio_event_id);
#if (EIO_EVENT_MODE == 0)
void eio_event_set_topic(eio_obj_t * const me,
                            uint8_t eio_event_id, uint32_t topic);
#else
void eio_event_set_callback(eio_obj_t * const me,
                            uint8_t eio_event_id,
                            eio_callback_t callback);
#endif

/* EIO general functions. */
eio_err_t eio_open(eio_obj_t * const me);
eio_err_t eio_close(eio_obj_t * const me);
int32_t eio_read(eio_obj_t *me, uint32_t pos, void *buff, uint32_t size);
int32_t eio_write(eio_obj_t *me, uint32_t pos, const void *buff, uint32_t size);

/* EIO general functions. */
void eio_get_time(eio_time_t *time);
uint32_t eio_time_diff_ms(eio_time_t *time_current, eio_time_t *time_last);
uint32_t eio_time_diff_us(eio_time_t *time_current, eio_time_t *time_last);

/* public port function ----------------------------------------------------- */
uint32_t eio_port_system_time(void);
void eio_port_rt_isr_enable(uint8_t rt_level, bool enable);
void *eio_port_malloc(uint32_t size);
void eio_port_free(void *memory);

#ifdef __cplusplus
}
#endif

#endif /* __EIO_H__ */

/* notes -------------------------------------------------------------------- */
/* Note 1
The macro should be defined by users using thire own assert function. But the 
macro has default definition that's supported by the elog module, the log module
for EventOS platform. 
*/

/* ----------------------------- end of file -------------------------------- */
