#ifndef __IO_UTIL_H__
#define __IO_UTIL_H__

#include <stddef.h>
#include <stdint.h>

#include "bp.h"

typedef ev_t (*key_decoder_t)(char key);

int prepare_ext_event_pipeline(key_decoder_t key_decoder);
int start_timer(ev_t ev, int64_t ms);
void stop_timer(int hndl);

#endif // __IO_UTIL_H__