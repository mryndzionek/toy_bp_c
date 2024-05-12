#ifndef __BP_H__
#define __BP_H__

#include <stddef.h>
#include <stdint.h>

typedef uint32_t ev_t;

typedef struct _bt_ctx_t bt_ctx_t;
typedef void (*bt_thread_t)(bt_ctx_t *ctx);
typedef ev_t (*external_ev_clbk_t)(void);

typedef struct _bp_ctx_t bp_ctx_t;

bp_ctx_t *bp_new(bt_thread_t *threads, size_t n, external_ev_clbk_t ext_ev_clbk);
ev_t bt_sync(bt_ctx_t *ctx, ev_t req, ev_t waiting, ev_t blocked);
void bp_run(bp_ctx_t *bp_ctx);
void bp_destroy(bp_ctx_t **bp_ctx_p);

#endif // __BP_H__