#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libdill.h>

#include "bp.h"

#include "io_util.h"
#include "logging.h"

#define N_ROWS (3)
#define N_COLS (3)
#define N_POS (N_COLS * N_ROWS)
#define NUM_LINES (8)
#define MAX_EV ((2 * N_POS) + 3)

#define EV_ALL_X_MOVES ((1UL << (N_POS)) - 1)
#define EV_ALL_O_MOVES (EV_ALL_X_MOVES << N_POS)
#define EV_ALL_MOVES (EV_ALL_X_MOVES | EV_ALL_O_MOVES)
#define EV_TERMINAL (EV_O_WIN | EV_X_WIN | EV_DRAW)

#define EV_DEFS()          \
    EV_DEF(X, 0, 0, 0)     \
    EV_DEF(X, 0, 0, 1)     \
    EV_DEF(X, 0, 0, 2)     \
    EV_DEF(X, 0, 1, 0)     \
    EV_DEF(X, 0, 1, 1)     \
    EV_DEF(X, 0, 1, 2)     \
    EV_DEF(X, 0, 2, 0)     \
    EV_DEF(X, 0, 2, 1)     \
    EV_DEF(X, 0, 2, 2)     \
    EV_DEF(O, N_POS, 0, 0) \
    EV_DEF(O, N_POS, 0, 1) \
    EV_DEF(O, N_POS, 0, 2) \
    EV_DEF(O, N_POS, 1, 0) \
    EV_DEF(O, N_POS, 1, 1) \
    EV_DEF(O, N_POS, 1, 2) \
    EV_DEF(O, N_POS, 2, 0) \
    EV_DEF(O, N_POS, 2, 1) \
    EV_DEF(O, N_POS, 2, 2)

#define LINES()   \
    LINE(0, 1, 2) \
    LINE(3, 4, 5) \
    LINE(6, 7, 8) \
    LINE(0, 3, 6) \
    LINE(1, 4, 7) \
    LINE(2, 5, 8) \
    LINE(0, 4, 8) \
    LINE(2, 4, 6)

typedef struct
{
    ev_t const *o_ev;
    ev_t const *x_ev;
} ev_p_pair;

#define EV_DEF(_t, _o, _y, _x) static const ev_t EV_##_t##_##_y##_##_x = 1UL << ((N_COLS * _y) + _x + _o);
EV_DEFS();
#undef EV_DEF

static const ev_t EV_O_WIN = 1UL << (2 * N_POS);
static const ev_t EV_X_WIN = 1UL << ((2 * N_POS) + 1);
static const ev_t EV_DRAW = 1UL << ((2 * N_POS) + 2);

#define EV_DEF(_t, _o, _y, _x) "EV_" #_t "_" #_y "_" #_x,
static char const *const ev_to_str_lut[] = {
    EV_DEFS() "EV_O_WIN",
    "EV_X_WIN",
    "EV_DRAW"};

#define LINE(_p1, _p2, _p3) ((1UL << _p1) | (1UL << _p2) | (1UL << _p3)),
static const ev_t x_lines[NUM_LINES] = {
    LINES()};
#undef LINE

#define LINE(_p1, _p2, _p3) (((1UL << _p1) | (1UL << _p2) | (1UL << _p3)) << N_POS),
static const ev_t o_lines[NUM_LINES] = {
    LINES()};
#undef LINE

static const ev_t forks22[] = {
    EV_X_1_2 | EV_X_2_0,
    EV_X_2_1 | EV_X_0_2,
    EV_X_1_2 | EV_X_2_1,
};

static const ev_t forks22_d = EV_O_2_2 | EV_O_0_2 | EV_O_2_0;

static const ev_t forks02[] = {
    EV_X_1_2 | EV_X_0_0,
    EV_X_0_1 | EV_X_2_2,
    EV_X_1_2 | EV_X_0_1,
};

static const ev_t forks02_d = EV_O_0_2 | EV_O_0_0 | EV_O_2_2;

static const ev_t forks20[] = {
    EV_X_1_0 | EV_X_2_2,
    EV_X_2_1 | EV_X_0_0,
    EV_X_2_1 | EV_X_1_0,
};

static const ev_t forks20_d = EV_O_2_0 | EV_O_0_0 | EV_O_2_2;

static const ev_t forks00[] = {
    EV_X_0_1 | EV_X_2_0,
    EV_X_1_0 | EV_X_0_2,
    EV_X_0_1 | EV_X_1_0,
};

static const ev_t forks00_d = EV_O_0_0 | EV_O_0_2 | EV_O_2_0;

static const ev_t forks_diag[] = {
    EV_X_0_2 | EV_X_2_0,
    EV_X_0_0 | EV_X_2_2};

static const ev_t forks_diag_d = EV_O_0_1 | EV_O_1_0 | EV_O_1_2 | EV_O_2_1;

static int ext_ev_ch;

const char *ev_to_str(ev_t ev)
{
    size_t i;

    for (i = 0; i < MAX_EV; i++)
    {
        if (ev & (1UL << i))
        {

            break;
        }
    }

    return ev_to_str_lut[i];
}

static void bt_square_taken(bt_ctx_t *ctx, void *user_ctx)
{
    ev_t blocked = EV_NONE;

    while (true)
    {
        ev_t ev = bt_sync(ctx, EV_NONE, EV_ALL_MOVES, blocked);
        blocked |= ev;
        if (ev < (1UL << N_POS))
        {
            blocked |= ev << N_POS;
        }
        else
        {
            blocked |= ev >> N_POS;
        }
    }
}

static void bt_enforce_turns(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_NONE, EV_ALL_X_MOVES, EV_ALL_O_MOVES);
        bt_sync(ctx, EV_NONE, EV_ALL_O_MOVES, EV_ALL_X_MOVES);
    }
}

static void bt_end_of_game(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_NONE, EV_TERMINAL, EV_NONE);
        bt_sync(ctx, EV_NONE, EV_NONE, EV_ALL_MOVES);
    }
}

static void bt_detect_draw(bt_ctx_t *ctx, void *user_ctx)
{
    for (size_t i = 0; i < N_POS; i++)
    {
        bt_sync(ctx, EV_NONE, EV_ALL_MOVES, EV_NONE);
    }
    bt_sync(ctx, EV_DRAW, EV_NONE, EV_NONE);
}

static void bt_detect_x_win(bt_ctx_t *ctx, void *user_ctx)
{
    ev_t line = *(ev_t *)user_ctx;

    for (size_t i = 0; i < N_ROWS; i++)
    {
        bt_sync(ctx, EV_NONE, line, EV_NONE);
    }
    bt_sync(ctx, EV_X_WIN, EV_NONE, EV_NONE);
}

static void bt_detect_o_win(bt_ctx_t *ctx, void *user_ctx)
{
    ev_t line = *(ev_t *)user_ctx;

    for (size_t i = 0; i < N_ROWS; i++)
    {
        bt_sync(ctx, EV_NONE, line, EV_NONE);
    }
    bt_sync(ctx, EV_O_WIN, EV_NONE, EV_NONE);
}

static void bt_center_preference(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_O_1_1, EV_NONE, EV_NONE);
    }
}

static void bt_corner_preference(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_O_0_0 | EV_O_0_2 | EV_O_2_0 | EV_O_2_2,
                EV_NONE, EV_NONE);
    }
}

static void bt_side_preference(bt_ctx_t *ctx, void *user_ctx)
{
    while (true)
    {
        bt_sync(ctx, EV_O_0_1 | EV_O_1_0 | EV_O_1_2 | EV_O_2_1,
                EV_NONE, EV_NONE);
    }
}

static void bt_add_third_o(bt_ctx_t *ctx, void *user_ctx)
{
    ev_t line = *(ev_t *)user_ctx;

    for (size_t i = 0; i < 2; i++)
    {
        bt_sync(ctx, EV_NONE, line, EV_NONE);
    }
    bt_sync(ctx, line, EV_NONE, EV_NONE);
}

static void bt_prevent_third_x(bt_ctx_t *ctx, void *user_ctx)
{
    ev_p_pair *lines = (ev_p_pair *)user_ctx;

    for (size_t i = 0; i < 2; i++)
    {
        bt_sync(ctx, EV_NONE, *lines->x_ev, EV_NONE);
    }
    bt_sync(ctx, *lines->o_ev, EV_NONE, EV_NONE);
}

static void bt_block_fork(bt_ctx_t *ctx, void *user_ctx)
{
    ev_p_pair *forks = (ev_p_pair *)user_ctx;

    for (size_t i = 0; i < 2; i++)
    {
        bt_sync(ctx, EV_NONE, *forks->x_ev, EV_NONE);
    }
    bt_sync(ctx, *forks->o_ev, EV_NONE, EV_NONE);
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
    ev_t ev = EV_NONE;

    switch (key)
    {
    case 'q':
        ev = EV_X_0_0;
        break;

    case 'w':
        ev = EV_X_0_1;
        break;

    case 'e':
        ev = EV_X_0_2;
        break;

    case 'a':
        ev = EV_X_1_0;
        break;

    case 's':
        ev = EV_X_1_1;
        break;

    case 'd':
        ev = EV_X_1_2;
        break;

    case 'z':
        ev = EV_X_2_0;
        break;

    case 'x':
        ev = EV_X_2_1;
        break;

    case 'c':
        ev = EV_X_2_2;
        break;

    default:
        break;
    }
    return ev;
}

static void render_clbk(ev_t ev)
{
    static char board[N_COLS][N_ROWS] = {{'_', '_', '_'},
                                         {'_', '_', '_'},
                                         {'_', '_', '_'}};

    if (ev & EV_ALL_MOVES)
    {
        char s = 'X';
        if (ev & EV_ALL_O_MOVES)
        {
            ev >>= N_POS;
            s = 'O';
        }

        for (size_t y = 0; y < N_COLS; y++)
        {
            for (size_t x = 0; x < N_ROWS; x++)
            {
                if (ev == (1UL << ((N_COLS * y) + x)))
                {
                    board[y][x] = s;
                }
                printf("[%c]", board[y][x]);
            }
            printf("\n");
        }
    }
}

int main(int argc, char *argv[])
{
    bt_init_t bthreads[] = {{bt_square_taken, NULL},
                            {bt_enforce_turns, NULL},
                            {bt_end_of_game, NULL},
                            {bt_detect_x_win, (void *)&x_lines[0]},
                            {bt_detect_x_win, (void *)&x_lines[1]},
                            {bt_detect_x_win, (void *)&x_lines[2]},
                            {bt_detect_x_win, (void *)&x_lines[3]},
                            {bt_detect_x_win, (void *)&x_lines[4]},
                            {bt_detect_x_win, (void *)&x_lines[5]},
                            {bt_detect_x_win, (void *)&x_lines[6]},
                            {bt_detect_x_win, (void *)&x_lines[7]},
                            {bt_detect_o_win, (void *)&o_lines[0]},
                            {bt_detect_o_win, (void *)&o_lines[1]},
                            {bt_detect_o_win, (void *)&o_lines[2]},
                            {bt_detect_o_win, (void *)&o_lines[3]},
                            {bt_detect_o_win, (void *)&o_lines[4]},
                            {bt_detect_o_win, (void *)&o_lines[5]},
                            {bt_detect_o_win, (void *)&o_lines[6]},
                            {bt_detect_o_win, (void *)&o_lines[7]},
                            {bt_detect_draw, NULL},
                            {bt_add_third_o, (void *)&o_lines[0]},
                            {bt_add_third_o, (void *)&o_lines[1]},
                            {bt_add_third_o, (void *)&o_lines[2]},
                            {bt_add_third_o, (void *)&o_lines[3]},
                            {bt_add_third_o, (void *)&o_lines[4]},
                            {bt_add_third_o, (void *)&o_lines[5]},
                            {bt_add_third_o, (void *)&o_lines[6]},
                            {bt_add_third_o, (void *)&o_lines[7]},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[0], .x_ev = &x_lines[0]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[1], .x_ev = &x_lines[1]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[2], .x_ev = &x_lines[2]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[3], .x_ev = &x_lines[3]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[4], .x_ev = &x_lines[4]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[5], .x_ev = &x_lines[5]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[6], .x_ev = &x_lines[6]}},
                            {bt_prevent_third_x, &(ev_p_pair){.o_ev = &o_lines[7], .x_ev = &x_lines[7]}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks22[0], .o_ev = &forks22_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks22[1], .o_ev = &forks22_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks22[2], .o_ev = &forks22_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks02[0], .o_ev = &forks02_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks02[1], .o_ev = &forks02_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks02[2], .o_ev = &forks02_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks20[0], .o_ev = &forks20_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks20[1], .o_ev = &forks20_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks20[2], .o_ev = &forks20_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks00[0], .o_ev = &forks00_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks00[1], .o_ev = &forks00_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks00[2], .o_ev = &forks00_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks_diag[0], .o_ev = &forks_diag_d}},
                            {bt_block_fork, &(ev_p_pair){.x_ev = &forks_diag[1], .o_ev = &forks_diag_d}},
                            {bt_center_preference, NULL},
                            {bt_corner_preference, NULL},
                            {bt_side_preference, NULL}};
    const size_t n = sizeof(bthreads) / sizeof(bthreads[0]);

    logging_init();

    ext_ev_ch = prepare_ext_event_pipeline(key_decoder);

    bp_ctx_t *bp_ctx = bp_new(bthreads, n, external_ev_clbk, render_clbk);
    log_assert(bp_ctx);

    LOG(INFO, "Starting");
    bp_run(bp_ctx);

    bp_destroy(&bp_ctx);

    LOG(INFO, "Stopping");
    exit(EXIT_SUCCESS);
}
