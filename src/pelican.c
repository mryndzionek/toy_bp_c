#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdill.h>

#include "bp.h"

#include "io_util.h"
#include "logging.h"

static const ev_t EV_CARS_YELLOW = 1UL << 0;
static const ev_t EV_PEDS_WALK = 1UL << 1;
static const ev_t EV_CARS_GREEN = 1UL << 2;

static const ev_t EV_LIGHT_TIMEOUT = 1UL << 4;
static const ev_t EV_PEDS_BUTTON = 1UL << 5;

static const ev_t EV_PEDS_OFF = 1UL << 6;

static const ev_t EV_FAILURE = 1UL << 7;
static const ev_t EV_CANCEL = 1UL << 8;
static const ev_t EV_OPERATIONAL = 1UL << 9;

static const ev_t EV_ALARM_ON = 1UL << 10;
static const ev_t EV_ALARM_OFF = 1UL << 11;

static int ext_ev_ch;

const char *ev_to_str(ev_t ev)
{
    if (ev == EV_CARS_YELLOW)
    {
        return "EV_CARS_YELLOW";
    }
    else if (ev == EV_PEDS_WALK)
    {
        return "EV_PEDS_WALK";
    }
    else if (ev == EV_CARS_GREEN)
    {
        return "EV_CARS_GREEN";
    }
    else if (ev == EV_PEDS_WALK)
    {
        return "EV_PEDS_WALK";
    }
    else if (ev == EV_LIGHT_TIMEOUT)
    {
        return "EV_LIGHT_TIMEOUT";
    }
    else if (ev == EV_PEDS_BUTTON)
    {
        return "EV_PEDS_BUTTON";
    }
    else if (ev == EV_PEDS_OFF)
    {
        return "EV_PEDS_OFF";
    }
    else if (ev == EV_FAILURE)
    {
        return "EV_FAILURE";
    }
    else if (ev == EV_CANCEL)
    {
        return "EV_CANCEL";
    }
    else if (ev == EV_OPERATIONAL)
    {
        return "EV_OPERATIONAL";
    }
    else if (ev == EV_ALARM_ON)
    {
        return "EV_ALARM_ON";
    }
    else if (ev == EV_ALARM_OFF)
    {
        return "EV_ALARM_OFF";
    }
    else
    {
        return "Unknown";
    }
}

// the main behavior of the pelican crossing
// is cycling the lights in order
static void bt_cycle(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        while (true)
        {
            BT_SYNC(ctx, EV_CARS_GREEN, EV_NONE, EV_NONE);
            BT_SYNC(ctx, EV_CARS_YELLOW, EV_NONE, EV_NONE);
            BT_SYNC(ctx, EV_PEDS_WALK, EV_NONE, EV_NONE);

            for (size_t i = 0; i < 3; i++)
            {
                // Flash pedestrian light
                BT_SYNC(ctx, EV_PEDS_OFF, EV_NONE, EV_NONE);
                BT_SYNC(ctx, EV_PEDS_WALK, EV_NONE, EV_NONE);
            }
            BT_SYNC(ctx, EV_PEDS_OFF, EV_NONE, EV_NONE);
        }

        BT_ON_CANCEL();
    }
}

// the light changes are interspersed by time delays
static void bt_intersperse(bt_ctx_t *ctx, void *user_ctx)
{
    int tmr_h = -1;
    int64_t tmout;

    while (true)
    {
        while (true)
        {
            // on EV_CARS_YELLOW, or EV_PEDS_WALK, or EV_PEDS_OFF
            ev_t ev = BT_SYNC(ctx, EV_NONE, EV_CARS_YELLOW | EV_PEDS_WALK | EV_PEDS_OFF, EV_NONE);
            if (ev == EV_CARS_YELLOW)
            {
                tmout = 3000;
            }
            else if (ev == EV_PEDS_OFF)
            {
                tmout = 500;
            }
            // start a timeout timer
            tmr_h = start_timer(EV_LIGHT_TIMEOUT, tmout);
            // wait for the timer event while blocking the successive events
            BT_SYNC(ctx, EV_NONE, EV_LIGHT_TIMEOUT, EV_PEDS_WALK | EV_PEDS_OFF | EV_CARS_GREEN);
        }

        BT_ON_CANCEL();
        stop_timer(tmr_h);
    }
}

// light cycling sequence is started by pressing the
// "pedestrian waiting" button and the next cycle can
// only start afte 'cars green' light
static void bt_trigger(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        while (true)
        {
            BT_SYNC(ctx, EV_NONE, EV_PEDS_BUTTON, EV_CARS_YELLOW);
            BT_SYNC(ctx, EV_NONE, EV_CARS_GREEN, EV_PEDS_BUTTON);
        }

        BT_ON_CANCEL();
    }
}

// makes sure cars have guaranteed minimal time
// of green light
static void bt_min_cars_green(bt_ctx_t *ctx, void *user_ctx)
{
    int tmr_h = -1;

    while (true)
    {
        while (true)
        {
            // after 'cars green' block 'cars yellow'
            // for 5s
            BT_SYNC(ctx, EV_NONE, EV_CARS_GREEN, EV_NONE);
            tmr_h = start_timer(EV_LIGHT_TIMEOUT, 5000);
            BT_SYNC(ctx, EV_NONE, EV_LIGHT_TIMEOUT, EV_CARS_YELLOW);
        }

        BT_ON_CANCEL();
        stop_timer(tmr_h);
    }
}

static void bt_failure(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        BT_SYNC(ctx, EV_NONE, EV_FAILURE, EV_NONE);
        BT_SYNC(ctx, EV_CANCEL, EV_NONE, EV_NONE);
        BT_SYNC(ctx, EV_NONE, EV_FAILURE, EV_CARS_GREEN);
        BT_SYNC(ctx, EV_OPERATIONAL, EV_NONE, EV_NONE);
    }

    BT_ON_CANCEL();
}

static void bt_failure_blink(bt_ctx_t *ctx, void *user_ctx)
{
    int tmr_h = -1;

    while (true)
    {
        BT_SYNC(ctx, EV_NONE, EV_CANCEL, EV_NONE);
        while (true)
        {
            BT_SYNC(ctx, EV_ALARM_ON, EV_NONE, EV_NONE);
            tmr_h = start_timer(EV_LIGHT_TIMEOUT, 1000);
            BT_SYNC(ctx, EV_NONE, EV_LIGHT_TIMEOUT, EV_NONE);
            BT_SYNC(ctx, EV_ALARM_OFF, EV_NONE, EV_NONE);
            tmr_h = start_timer(EV_LIGHT_TIMEOUT, 1000);
            BT_SYNC(ctx, EV_NONE, EV_LIGHT_TIMEOUT, EV_NONE);
        }

        BT_ON_CANCEL();
        stop_timer(tmr_h);
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

static ev_t key_decoder(char key)
{
    if (key == 'a')
    {
        return EV_FAILURE;
    }
    else
    {
        return EV_PEDS_BUTTON;
    }
}

int main(int argc, char *argv[])
{
    const bt_init_t bthreads[] = {{bt_cycle, EV_CANCEL, .user_ctx = NULL},
                                  {bt_intersperse, EV_CANCEL, .user_ctx = NULL},
                                  {bt_trigger, EV_CANCEL, .user_ctx = NULL},
                                  {bt_min_cars_green, EV_CANCEL, .user_ctx = NULL},
                                  {bt_failure, .user_ctx = NULL},
                                  {bt_failure_blink, EV_OPERATIONAL, .user_ctx = NULL}};
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
