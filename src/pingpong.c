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

static void bt_ping(bt_ctx_t *ctx)
{
    for (size_t i = 0; i < 10; i++)
    {
        bt_sync(ctx, EV_PING, 0, 0);
        bt_sync(ctx, 0, EV_PONG, 0);
    }
}

static void bt_pong(bt_ctx_t *ctx)
{
    for (size_t i = 0; i < 10; i++)
    {
        bt_sync(ctx, 0, EV_PING, 0);
        bt_sync(ctx, EV_PONG, 0, 0);
    }
}

int main(int argc, char *argv[])
{
    bt_thread_t bthreads[] = {bt_ping, bt_pong};
    const size_t n = sizeof(bthreads) / sizeof(bthreads[0]);

    logging_init();

    bp_ctx_t *bp_ctx = bp_new(bthreads, n, NULL);
    log_assert(bp_ctx);

    LOG(INFO, "Starting");
    bp_run(bp_ctx);

    bp_destroy(&bp_ctx);

    LOG(INFO, "Stopping");
    exit(EXIT_SUCCESS);
}
