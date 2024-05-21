#include "bp.h"

#include <stdbool.h>
#include <time.h>
#include <stdlib.h>

#include <libdill.h>

#include "logging.h"

extern const char *ev_to_str(ev_t ev);

struct _bt_ctx_t
{
    bt_init_t init;
    uint8_t id;
    int ch_in;
    int ch_out;
    bool done;
};

struct _bp_ctx_t
{
    bt_ctx_t *bt_ctx;
    int *ch_out;
    int bundle;
    size_t n;
    int ch_in;
    external_ev_clbk_t ext_ev_clbk;
    render_clbk_t render_clbk;
};

typedef struct
{
    uint8_t id;
    ev_t req;
    ev_t waiting;
    ev_t blocked;
} ev_msg_t;

static coroutine void bt_thread(bt_ctx_t *ctx)
{
    ctx->init.thread(ctx, ctx->init.user_ctx);

    ctx->done = true;
    ev_msg_t done = {0};
    done.id = ctx->id;

    LOG(DEBUG, "Ending  id: %d", ctx->id);
    int rc = chsend(ctx->ch_out, &done, sizeof(ev_msg_t), -1);
    log_assert(rc == 0);
}

static ev_t choose_random(ev_t allowed)
{
    ev_t chosen = EV_NONE;

    // count bits
    size_t n_bits = 0;
    for (int j = sizeof(ev_t) * 8 - 1; j >= 0; j--)
    {
        if (allowed & (1UL << j))
        {
            n_bits++;
        }
    }

    // get a random number from range [0;n_bits)
    int r = rand() % n_bits;

    n_bits = 0;
    for (int j = sizeof(ev_t) * 8 - 1; j >= 0; j--)
    {
        if (allowed & (1UL << j))
        {
            // choose random bit
            if (n_bits == r)
            {
                chosen = 1UL << j;
                break;
            }
            n_bits++;
        }
    }

    return chosen;
}

bp_ctx_t *bp_new(const bt_init_t *const bt_init, size_t n, external_ev_clbk_t ext_ev_clbk, render_clbk_t render_clbk)
{
    int rc;
    int ch_shared[2];
    int ch_tmp[2];

    srand(time(NULL));

    bp_ctx_t *bp_ctx = (bp_ctx_t *)malloc(sizeof(bp_ctx_t));
    if (!bp_ctx)
    {
        return NULL;
    }

    bp_ctx->bt_ctx = (bt_ctx_t *)malloc(n * sizeof(bt_ctx_t));
    if (!bp_ctx->bt_ctx)
    {
        free(bp_ctx);
        return NULL;
    }

    bp_ctx->ch_out = (int *)malloc(n * sizeof(int));
    if (!bp_ctx->ch_out)
    {
        free(bp_ctx->bt_ctx);
        free(bp_ctx);
        return NULL;
    }

    rc = chmake(ch_shared);
    log_assert(rc >= 0);

    for (size_t i = 0; i < n; i++)
    {
        rc = chmake(ch_tmp);
        log_assert(rc >= 0);

        bp_ctx->ch_in = ch_shared[1];
        bp_ctx->bt_ctx[i].ch_in = ch_tmp[1];
        bp_ctx->bt_ctx[i].ch_out = ch_shared[0];
        bp_ctx->bt_ctx[i].init = bt_init[i];
        bp_ctx->bt_ctx[i].id = i;
        bp_ctx->ch_out[i] = ch_tmp[0];
        bp_ctx->bt_ctx->done = false;
    }

    bp_ctx->n = n;
    bp_ctx->ext_ev_clbk = ext_ev_clbk;
    bp_ctx->render_clbk = render_clbk;

    return bp_ctx;
}

ev_t bt_sync(bt_ctx_t *ctx, ev_t req, ev_t waiting, ev_t blocked)
{
    ev_t ev;
    ev_msg_t ev_msg = {.id = ctx->id, .req = req, .waiting = waiting, .blocked = blocked};

    int rc = chsend(ctx->ch_out, &ev_msg, sizeof(ev_msg_t), -1);
    log_assert(rc == 0);
    rc = chrecv(ctx->ch_in, &ev, sizeof(ev_t), -1);
    log_assert(rc == 0);

    return ev;
}

ev_t bt_cancel_evs(bt_ctx_t *ctx)
{
    return ctx->init.cancel_evs;
}

void bp_run(bp_ctx_t *bp_ctx)
{
    int ret;
    ev_t blocked = 0;
    ev_t requested = 0;
    ev_t waiting = 0;

    ev_msg_t ev_msg;
    ev_msg_t ev_msgs[bp_ctx->n];

    bp_ctx->bundle = bundle();

    // initialize the event table and
    // start the b-thread coroutines
    for (size_t i = 0; i < bp_ctx->n; i++)
    {
        ev_msgs[i].id = i;
        ev_msgs[i].req = 0;
        ev_msgs[i].waiting = 0;
        ev_msgs[i].blocked = 0;

        ret = bundle_go(bp_ctx->bundle, bt_thread(&bp_ctx->bt_ctx[i]));
        log_assert(ret == 0);
    }

    size_t req_num = bp_ctx->n;

    while (true)
    {
        // gather bids
        for (size_t j = 0; j < req_num; j++)
        {
            int rc = chrecv(bp_ctx->ch_in, &ev_msg, sizeof(ev_msg_t), -1);
            log_assert(rc == 0);
            LOG(DEBUG, "Received bid:  id: %d, req: %08x, waiting: %08x, blocked: %08x", ev_msg.id, ev_msg.req, ev_msg.waiting, ev_msg.blocked);

            size_t i = ev_msg.id;
            ev_msgs[i].req = ev_msg.req;
            ev_msgs[i].waiting = ev_msg.waiting | bp_ctx->bt_ctx[i].init.cancel_evs;
            ev_msgs[i].blocked = ev_msg.blocked;

            requested |= ev_msgs[i].req;
            blocked |= ev_msgs[i].blocked;
            waiting |= ev_msgs[i].waiting;
        }

        LOG(DEBUG, "Requested: %08x", requested);
        LOG(DEBUG, "Waiting: %08x", waiting);
        LOG(DEBUG, "Blocked: %08x", blocked);

        ev_t allowed = requested & (~blocked);
        LOG(DEBUG, "Allowed: %08x", allowed);

        ev_t chosen = 0;

        if (!allowed)
        {
            if (bp_ctx->ext_ev_clbk && waiting)
            {
                while (true)
                {
                    // this in equivalent to requesting the external event
                    allowed = bp_ctx->ext_ev_clbk() & (~blocked);
                    if (allowed)
                    {
                        LOG(DEBUG, "Allowed (external): %d", allowed);
                        for (int j = sizeof(ev_t) * 8 - 1; j >= 0; j--)
                        {
                            if (allowed & (1UL << j))
                            {
                                chosen = 1UL << j;
                                LOG(INFO, "Selected (external): %s", ev_to_str(chosen));
                                if (bp_ctx->render_clbk)
                                {
                                    bp_ctx->render_clbk(chosen);
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            else
            {
                // we are done
                break;
            }
        }

        req_num = 0;

        // find allowed event requested by highest priority (lowest index) b-thread
        for (size_t i = 0; i < bp_ctx->n; i++)
        {
            if (allowed & (ev_msgs[i].req))
            {
                allowed &= (ev_msgs[i].req);

                chosen = choose_random(allowed);

                LOG(INFO, "Selected: %s", ev_to_str(chosen));
                if (bp_ctx->render_clbk)
                {
                    bp_ctx->render_clbk(chosen);
                }
                break;
            }
        }

        log_assert(chosen);
        blocked = 0;
        requested = 0;
        waiting = 0;

        // propagate the chosen request event to waiting and requesting b-threads
        for (size_t i = 0; i < bp_ctx->n; i++)
        {
            if (chosen & (ev_msgs[i].waiting | ev_msgs[i].req))
            {
                if (!bp_ctx->bt_ctx[i].done)
                {
                    LOG(DEBUG, "Sending wait to id: %ld", i);
                    int rc = chsend(bp_ctx->ch_out[i], &chosen, sizeof(ev_t), -1);
                    log_assert(rc == 0);
                    ev_msgs[i].waiting &= ~chosen;
                    req_num++;
                }
            }
            else
            {
                requested |= ev_msgs[i].req;
                waiting |= ev_msgs[i].waiting;
                blocked |= ev_msgs[i].blocked;
            }
        }
    }
}

void bp_destroy(bp_ctx_t **bp_ctx_p)
{
    log_assert(bp_ctx_p);
    if (*bp_ctx_p)
    {
        bp_ctx_t *bp_ctx = *bp_ctx_p;
        free(bp_ctx->ch_out);
        free(bp_ctx->bt_ctx);
        free(bp_ctx);
        *bp_ctx_p = NULL;
    }
}
