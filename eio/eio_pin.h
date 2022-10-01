
#ifndef __DEV_SWITCH_H__
#define __DEV_SWITCH_H__

// include ---------------------------------------------------------------------
#include "eio.h"

#ifdef __cplusplus
extern "C" {
#endif

// enum ------------------------------------------------------------------------
enum
{
    PIN_MODE_OUT_OD = 0,
    PIN_MODE_OUT_PP,
    PIN_MODE_IN,
    PIN_MODE_IN_PULLUP,
    PIN_MODE_IN_PULLDOWN,

    PIN_MODE_MAX
};

// data struct -----------------------------------------------------------------
typedef struct eio_pin_tag
{
    eio_obj_t super;
    const struct eio_pin_ops *ops;

    bool status;
    uint8_t mode;
} eio_pin_t;

struct eio_pin_ops
{
    void (* set_status)(eio_pin_t *me, bool status);
    bool (* get_status)(eio_pin_t *me);
};

// api -------------------------------------------------------------------------
eio_err_t eio_pin_register(eio_pin_t *me, const char *name, void *user_data);
void eio_pin_set_mode(eio_obj_t *me, uint8_t mode);
bool eio_pin_get_status(eio_obj_t *me);
void eio_pin_set_status(eio_obj_t *me, bool status);

#ifdef __cplusplus
}
#endif

#endif

// Note ------------------------------------------------------------------------