#ifndef __BP_H__
#define __BP_H__

#include <stddef.h>
#include <stdint.h>

#define BT_SYNC(_ctx, _req, _waiting, _blocked) \
    ({ev_t ev; ev = bt_sync(_ctx, _req, _waiting, _blocked);\
                        if(ev & bt_cancel_evs(_ctx)) {goto bt_end;}; ev; })

#define BT_ON_CANCEL() bt_end:

typedef uint32_t ev_t;

typedef struct _bt_ctx_t bt_ctx_t;
typedef void (*bt_thread_t)(bt_ctx_t *ctx, void *user_ctx);

typedef struct
{
    bt_thread_t thread;
    ev_t cancel_evs;
    void *user_ctx;
} bt_init_t;

typedef ev_t (*external_ev_clbk_t)(void);
typedef void (*render_clbk_t)(ev_t ev);

typedef struct _bp_ctx_t bp_ctx_t;

static const ev_t EV_NONE = 0;

bp_ctx_t *bp_new(const bt_init_t *const bt_init, size_t n, external_ev_clbk_t ext_ev_clbk, render_clbk_t render_clbk);
ev_t bt_sync(bt_ctx_t *ctx, ev_t req, ev_t waiting, ev_t blocked);
ev_t bt_cancel_evs(bt_ctx_t *ctx);
void bp_run(bp_ctx_t *bp_ctx);
void bp_destroy(bp_ctx_t **bp_ctx_p);

#endif // __BP_H__