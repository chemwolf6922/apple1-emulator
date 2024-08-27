#include <assert.h>
#include "tev.h"
#include "terminal.h"
#include <stdio.h>

static void stop_test(void* ctx)
{
    terminal_t* term = (terminal_t*)ctx;
    term->close();
}

static void on_input(char c, void* ctx)
{
    terminal_t* term = (terminal_t*)ctx;
    term->write(c);
}

int main(int argc, char const *argv[])
{
    tev_handle_t tev = tev_create_ctx();
    assert(tev);
    terminal_t* term = terminal_get_singleton(tev);
    assert(term);
    term->callbacks.on_data = on_input;
    term->callbacks.on_data_ctx = term;
    tev_set_timeout(tev, stop_test, term, 10000);
    
    tev_main_loop(tev);

    tev_free_ctx(tev);

    return 0;
}

