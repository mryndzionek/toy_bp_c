#include "io_util.h"

#include <termios.h>

#include <libdill.h>

#include "logging.h"

static int ext_ev_ch[2];
static key_decoder_t g_key_dec;

static coroutine void key_press_handler(int out_ch)
{
    int ret;

    static struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (true)
    {
        ret = fdin(STDIN_FILENO, -1);
        if (ret < 0)
        {
            break;
        }

        int c = getc(stdin);
        ev_t ev = g_key_dec(c);
        ret = chsend(out_ch, &ev, sizeof(ev_t), -1);
        if (ret < 0)
        {
            break;
        }
    }
    ret = chdone(out_ch);
    log_assert(ret == 0);

    fdclean(STDIN_FILENO);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

static coroutine void timeout_handler(int out_ch, ev_t ev, int64_t ms)
{
    int rc = msleep(now() + ms);
    if (rc == 0)
    {
        rc = chsend(out_ch, &ev, sizeof(ev_t), -1);
        log_assert(rc == 0);
    }
}

int start_timer(ev_t ev, int64_t ms)
{
    LOG(DEBUG, "starting timer");
    int h = go(timeout_handler(ext_ev_ch[0], ev, ms));
    log_assert(h >= 0);
    return h;
}

int stop_timer(int hndl)
{
    LOG(DEBUG, "stopping timer");
    int rc = hclose(hndl);
    return rc;
}

int prepare_ext_event_pipeline(key_decoder_t key_decoder)
{
    int rc;

    g_key_dec = key_decoder;
    rc = chmake(ext_ev_ch);
    log_assert(rc == 0);
    log_assert(ext_ev_ch[0] >= 0);
    log_assert(ext_ev_ch[1] >= 0);

    int kh = go(key_press_handler(ext_ev_ch[0]));
    log_assert(kh >= 0);

    return ext_ev_ch[1];
}
