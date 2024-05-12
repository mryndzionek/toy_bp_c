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
    else
    {
        return "Unknown";
    }
}

// the main behavior of the pelican crossing
// is cycling the lights in order
static void bt_cycle(bt_ctx_t *ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_CARS_GREEN, 0, 0);
        bt_sync(ctx, EV_CARS_YELLOW, 0, 0);
        bt_sync(ctx, EV_PEDS_WALK, 0, 0);

        for (size_t i = 0; i < 3; i++)
        {
            // Flash pedestrian light
            bt_sync(ctx, EV_PEDS_OFF, 0, 0);
            bt_sync(ctx, EV_PEDS_WALK, 0, 0);
        }
        bt_sync(ctx, EV_PEDS_OFF, 0, 0);
    }
}

// the light changes are interspersed by time delays
static void bt_intersperse(bt_ctx_t *ctx)
{
    int64_t tmout;

    while (true)
    {
        // on EV_CARS_YELLOW, or EV_PEDS_WALK, or EV_PEDS_OFF
        ev_t ev = bt_sync(ctx, 0, EV_CARS_YELLOW | EV_PEDS_WALK | EV_PEDS_OFF, 0);
        if (ev == EV_CARS_YELLOW)
        {
            tmout = 3000;
        }
        else if (ev == EV_PEDS_OFF)
        {
            tmout = 500;
        }
        // start a timeout timer
        start_timer(EV_LIGHT_TIMEOUT, tmout);
        // wait for the timer event while blocking the successive events
        bt_sync(ctx, 0, EV_LIGHT_TIMEOUT, EV_PEDS_WALK | EV_PEDS_OFF | EV_CARS_GREEN);
    }
}

// light cycling sequence is started by pressing the
// "pedestrian waiting" button and the next cycle can
// only start afte 'cars green' light
static void bt_trigger(bt_ctx_t *ctx)
{
    while (true)
    {
        bt_sync(ctx, 0, EV_PEDS_BUTTON, EV_CARS_YELLOW);
        bt_sync(ctx, 0, EV_CARS_GREEN, EV_PEDS_BUTTON);
    }
}

// makes sure cars have guaranteed minimal time
// of green light
static void bt_min_cars_green(bt_ctx_t *ctx)
{
    while (true)
    {
        // after 'cars green' block 'cars yellow'
        // for 5s
        bt_sync(ctx, 0, EV_CARS_GREEN, 0);
        start_timer(EV_LIGHT_TIMEOUT, 5000);
        bt_sync(ctx, 0, EV_LIGHT_TIMEOUT, EV_CARS_YELLOW);
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

int main(int argc, char *argv[])
{
    bt_thread_t bthreads[] = {bt_cycle, bt_intersperse, bt_trigger, bt_min_cars_green};
    const size_t n = sizeof(bthreads) / sizeof(bthreads[0]);

    logging_init();

    ext_ev_ch = prepare_ext_event_pipeline(EV_PEDS_BUTTON);

    bp_ctx_t *bp_ctx = bp_new(bthreads, n, external_ev_clbk);
    log_assert(bp_ctx);

    LOG(INFO, "Starting");
    bp_run(bp_ctx);
    bp_destroy(&bp_ctx);

    LOG(INFO, "Stopping");
    exit(EXIT_SUCCESS);
}
