#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bp.h"

#include "logging.h"

static const ev_t EV_ADD_HOT = 1UL << 0;
static const ev_t EV_ADD_COLD = 1UL << 1;

const char *ev_to_str(ev_t ev)
{
    if (ev == EV_ADD_COLD)
    {
        return "EV_ADD_COLD";
    }
    else if (ev == EV_ADD_HOT)
    {
        return "EV_ADD_HOT";
    }
    else
    {
        return "Unknown";
    }
}

static void bt_cold(bt_ctx_t *ctx, void *user_ctx)
{
    bt_sync(ctx, EV_ADD_COLD, EV_NONE, EV_NONE);
    bt_sync(ctx, EV_ADD_COLD, EV_NONE, EV_NONE);
    bt_sync(ctx, EV_ADD_COLD, EV_NONE, EV_NONE);
}

static void bt_hot(bt_ctx_t *ctx, void *user_ctx)
{
    bt_sync(ctx, EV_ADD_HOT, EV_NONE, EV_NONE);
    bt_sync(ctx, EV_ADD_HOT, EV_NONE, EV_NONE);
    bt_sync(ctx, EV_ADD_HOT, EV_NONE, EV_NONE);
}

static void bt_interleave(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, 0, EV_ADD_HOT, EV_ADD_COLD);
        bt_sync(ctx, 0, EV_ADD_COLD, EV_ADD_HOT);
    }
}

int main(int argc, char *argv[])
{
    const bt_init_t bthreads[] = {{bt_hot, NULL},
                                  {bt_cold, NULL},
                                  {bt_interleave, NULL}};
    const size_t n = sizeof(bthreads) / sizeof(bthreads[0]);

    logging_init();

    bp_ctx_t *bp_ctx = bp_new(bthreads, n, NULL, NULL);
    log_assert(bp_ctx);

    LOG(INFO, "Starting");
    bp_run(bp_ctx);

    bp_destroy(&bp_ctx);

    LOG(INFO, "Stopping");
    exit(EXIT_SUCCESS);
}
