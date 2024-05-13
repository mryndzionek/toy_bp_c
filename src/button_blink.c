#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <termios.h>

#include <libdill.h>

#include "bp.h"

#include "io_util.h"
#include "logging.h"

static const ev_t EV_LED_ON = 1UL << 0;
static const ev_t EV_LED_OFF = 1UL << 1;
static const ev_t EV_TIMEOUT = 1UL << 2;
static const ev_t EV_BUTTON = 1UL << 3;

static int ext_ev_ch;
static int tmr_hndl;

const char *ev_to_str(ev_t ev)
{
    if (ev == EV_LED_ON)
    {
        return "EV_LED_ON";
    }
    else if (ev == EV_LED_OFF)
    {
        return "EV_LED_OFF";
    }
    else if (ev == EV_TIMEOUT)
    {
        return "EV_TIMEOUT";
    }
    else if (ev == EV_BUTTON)
    {
        return "EV_BUTTON";
    }
    else
    {
        return "Unknown";
    }
}

static ev_t external_ev_clbk(void)
{
    int rc;
    ev_t ev;

    rc = chrecv(ext_ev_ch, &ev, sizeof(ev_t), -1);
    log_assert(rc == 0);

    return ev;
}

static void bt_blink(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_LED_ON, EV_NONE, EV_NONE);
        tmr_hndl = start_timer(EV_TIMEOUT, 1000);
        bt_sync(ctx, EV_NONE, EV_TIMEOUT, EV_NONE);

        bt_sync(ctx, EV_LED_OFF, EV_NONE, EV_NONE);
        tmr_hndl = start_timer(EV_TIMEOUT, 1000);
        bt_sync(ctx, EV_NONE, EV_TIMEOUT, EV_NONE);
    }
}

static void bt_button(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_NONE, EV_BUTTON, EV_NONE);
        stop_timer(tmr_hndl);
        bt_sync(ctx, EV_NONE, EV_BUTTON, EV_TIMEOUT);
        tmr_hndl = start_timer(EV_TIMEOUT, 1000);
    }
}

static ev_t key_decoder(char key)
{
    return EV_BUTTON;
}

int main(int argc, char *argv[])
{
    const bt_init_t bthreads[] = {{bt_blink, NULL}, {bt_button, NULL}};
    const size_t n = sizeof(bthreads) / sizeof(bthreads[0]);

    logging_init();

    ext_ev_ch = prepare_ext_event_pipeline(key_decoder);

    bp_ctx_t *bp_ctx = bp_new(bthreads, n, external_ev_clbk, NULL);
    log_assert(bp_ctx);

    LOG(INFO, "Starting");
    bp_run(bp_ctx);
    bp_destroy(&bp_ctx);

    LOG(INFO, "Stopping");
    exit(EXIT_SUCCESS);
}
