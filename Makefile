CFLAGS?=-O3
override CFLAGS+=-MMD -MP
override CFLAGS+=-Itev -I6502_emulator -I6821_emulator

APP=apple1
SRC=apple1.c terminal.c 6502_emulator/e6502.c 6821_emulator/e6821.c
STATIC_LIB=tev/libtev.a

.PHONY: all
all: $(APP)

$(APP):	$(SRC:.c=.o) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(SRC:.c=.d)

tev/libtev.a:
	$(MAKE) -C tev

.PHONY: clean
clean:
	$(MAKE) -C tev clean
	rm -f $(APP) *.o *.d 6502_emulator/*.o 6502_emulator/*.d 6821_emulator/*.o 6821_emulator/*.d
