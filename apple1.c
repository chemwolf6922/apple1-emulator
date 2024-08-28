#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "tev.h"
#include "e6502.h"
#include "e6821.h"
#include "terminal.h"
#include "rom.h"

typedef struct
{
    tev_handle_t tev;
    terminal_t* term;
    e6502_t cpu;
    e6821_t pia;
    pthread_t cpu_thread;
    pthread_spinlock_t pia_lock;
    uint8_t ram[0x10000];
} apple1_t;

static void* cpu_thread(void* ctx);
static void pia_lock(void* ctx);
static void pia_unlock(void* ctx);
static void write_to_term(uint8_t data, void* ctx);
static void on_term_input(char c, void* ctx);
static uint8_t on_cpu_load(uint16_t addr, void* ctx);
static void on_cpu_store(uint16_t addr, uint8_t data, void* ctx);
static int load_rom(const rom_block_t* rom, uint8_t* dst, size_t size);

static apple1_t this;

int main(int argc, char const *argv[])
{
    memset(&this, 0, sizeof(apple1_t));
    this.tev = tev_create_ctx();
    if (!this.tev)
        return -1;
    this.term = terminal_get_singleton(this.tev);
    if (!this.term)
    {
        tev_free_ctx(this.tev);
        return -1;
    }
    if (pthread_spin_init(&this.pia_lock, PTHREAD_PROCESS_PRIVATE)!=0)
    {
        this.term->close();
        tev_free_ctx(this.tev);
        return -1;
    }

    e6821_init(&this.pia);
    e6502_init(&this.cpu);
    this.pia.callbacks.lock = pia_lock;
    this.pia.callbacks.unlock = pia_unlock;
    this.pia.callbacks.write_to_device_B = write_to_term;
    this.term->callbacks.on_data = on_term_input;
    this.cpu.cb.load = on_cpu_load;
    this.cpu.cb.store = on_cpu_store;

    for (int i = 0; i < sizeof(ROM_BLOCKS)/sizeof(rom_block_t); i++)
    {
        if (load_rom(&ROM_BLOCKS[i], this.ram, sizeof(this.ram)) != 0)
        {
            pthread_spin_destroy(&this.pia_lock);
            this.term->close();
            tev_free_ctx(this.tev);
            return -1;
        }
    }
    
    if (pthread_create(&this.cpu_thread, NULL, cpu_thread, NULL) != 0)
    {
        pthread_spin_destroy(&this.pia_lock);
        this.term->close();
        tev_free_ctx(this.tev);
        return -1;
    }

    tev_main_loop(this.tev);

    pthread_join(this.cpu_thread, NULL);
    pthread_spin_destroy(&this.pia_lock);
    /** term should have been closed */
    tev_free_ctx(this.tev);

    return 0;
}

static void* cpu_thread(void* ctx)
{
    this.pia.iface.reset(&this.pia);
    this.cpu.iface.reset(&this.cpu);

    while(true)
    {
        this.cpu.iface.step(&this.cpu);
    }

    return NULL;
}



static void pia_lock(void* ctx)
{
    pthread_spin_lock(&this.pia_lock);
}

static void pia_unlock(void* ctx)
{
    pthread_spin_unlock(&this.pia_lock);
}

static void write_to_term(uint8_t data, void* ctx)
{
    this.term->write(data);
}

static void on_term_input(char c, void* ctx)
{
    this.pia.iface.input(&this.pia, E6821_PORT_A, c);
    this.pia.iface.set_irq(&this.pia, E6821_PORT_A, E6821_IRQ_LINE_1);
}

static uint8_t on_cpu_load(uint16_t addr, void* ctx)
{
    if ((addr & 0xF000) == 0xD000)
    {
        return this.pia.iface.read(&this.pia, addr & 0b11);
    }
    else
    {
        return this.ram[addr];
    }
}

static void on_cpu_store(uint16_t addr, uint8_t data, void* ctx)
{
    if ((addr & 0xF000) == 0xD000)
    {
        this.pia.iface.write(&this.pia, addr & 0x3, data);
    }
    else
    {
        /** @todo protect rom */
        this.ram[addr] = data;
    }
}

static int load_rom(const rom_block_t* rom, uint8_t* dst, size_t size)
{
    if (rom->length + rom->offset > size)
    {
        return -1;
    }
    memcpy(dst + rom->offset, rom->data, rom->length);
    return 0;
}

