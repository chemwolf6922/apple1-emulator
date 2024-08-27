#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "terminal.h"

typedef struct
{
    terminal_t iface;
    tev_handle_t tev;
    struct termios old_tty;
} terminal_impl_t;

static void terminal_close();
static void terminal_write(unsigned char c);
static void on_input(void* ctx);
static int set_raw_mode(struct termios* old_tty);
static void restore_termios(struct termios* old_tty);

terminal_impl_t* this = NULL;

static const char* valid_key_codes = 
    " 01!2\"3#4$5%6&7'8(9):*;+,<-=.>/?ABCDEF\nGHIJKLMN^OP@QRSTUVWXYZ\e";
static unsigned char is_valid_key[256] = {0};

terminal_t* terminal_get_singleton(tev_handle_t tev)
{
    if (this)
        return &this->iface;   
    this = malloc(sizeof(terminal_impl_t));
    if (!this)
        return NULL;
    memset(this, 0, sizeof(terminal_impl_t));
    this->tev = tev;
    this->iface.close = terminal_close;
    this->iface.write = terminal_write;
    if (set_raw_mode(&this->old_tty) != 0)
    {
        free(this);
        this = NULL;
        return NULL;
    }
    if (tev_set_read_handler(tev, STDIN_FILENO, on_input, NULL) != 0)
    {
        restore_termios(&this->old_tty);
        free(this);
        this = NULL;
        return NULL;
    }
    /** Build the valid key table */
    memset(is_valid_key, 0, sizeof(is_valid_key));
    for (int i = 0; i < strlen(valid_key_codes); i++)
        is_valid_key[valid_key_codes[i]] = 1;
    return &this->iface;
}

static void terminal_close()
{
    if (!this)
        return;
    tev_set_read_handler(this->tev, STDIN_FILENO, NULL, NULL);
    restore_termios(&this->old_tty);
    free(this);
    this = NULL;
}

static void terminal_write(unsigned char c)
{
    if ((c == (0x80 | '\r')) || (c == (0x80 | '\n')))
    {
        write(STDOUT_FILENO, "\n", 1);
        return;
    }
    /** Lift 0 - 0x1F to 0x40 - 0x5F */ 
    c = c & 0x3F;
    if (!(c & 0x20))
        c |= 0x40;
    write(STDOUT_FILENO, &c, 1);
}

static void on_input(void* ctx)
{
    /** @link{http://cini.classiccmp.org/pdf/Apple/Apple%20II%20Reference%20Manual%20-%20Woz.pdf} */
    /** @todo handle <- -> arrow keys (escape code) */
    /** @todo handle ctrl keys */
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return;
    /** convert lower case to upper case */
    if (c >= 'a' && c <= 'z')
        c -= ('a' - 'A');
    /** filter out invalid key codes */
    if (!is_valid_key[c])
        return;
    /** Set the 7th bit */
    c |= 0x80;
    if (this->iface.callbacks.on_data)
        this->iface.callbacks.on_data(c, this->iface.callbacks.on_data_ctx);
}

static int set_raw_mode(struct termios* old_tty)
{
    struct termios tty;
    if (tcgetattr(STDIN_FILENO, &tty) != 0)
        return -1;
    *old_tty = tty;
    tty.c_lflag &= ~(ICANON | ECHO);
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty) != 0)
        return -1;
    return 0;
}

static void restore_termios(struct termios* old_tty)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, old_tty);
}
