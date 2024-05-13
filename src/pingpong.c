#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bp.h"

#include "logging.h"

static const ev_t EV_PING = 1UL << 1;
static const ev_t EV_PONG = 1UL << 2;

const char *ev_to_str(ev_t ev)
{
    if (ev == EV_PING)
    {
        return "EV_PING";
    }
    else if (ev == EV_PONG)
    {
        return "EV_PONG";
    }
    else
    {
        return "Unknown";
    }
}

static void bt_ping(bt_ctx_t *ctx, void *user_ctx)
{
    for (size_t i = 0; i < 10; i++)
    {
        bt_sync(ctx, EV_PING, EV_NONE, EV_NONE);
        bt_sync(ctx, EV_NONE, EV_PONG, EV_NONE);
    }
}

static void bt_pong(bt_ctx_t *ctx, void *user_ctx)
{
    for (size_t i = 0; i < 10; i++)
    {
        bt_sync(ctx, EV_NONE, EV_PING, EV_NONE);
        bt_sync(ctx, EV_PONG, EV_NONE, EV_NONE);
    }
}

int main(int argc, char *argv[])
{
    bt_init_t bthreads[] = {{bt_ping, NULL}, {bt_pong, NULL}};
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
