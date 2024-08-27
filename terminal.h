#pragma once

#include "tev.h"

/** Singleton library */

typedef struct terminal_s terminal_t;
struct terminal_s
{
    void (*close)();
    void (*clear)();
    void (*write)(unsigned char c);
    struct
    {
        void (*on_data)(char c, void* ctx);
        void* on_data_ctx;
    } callbacks;
};

terminal_t* terminal_get_singleton(tev_handle_t tev);
