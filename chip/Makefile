# gpSP makefile
# Gilead Kutnick - Exophase
# CHIP port - Fox Cunning

# Global definitions

CC        = $(CROSS_COMPILE)gcc

OBJS      = chip.o main.o menu.o cpu.o memory.o video.o input.o sound.o gui.o \
            cheats.o zip.o cpu_threaded.o arm_stub.o video_blend.o warm.o \
            upscale_aspect.o configfile_fk.o
BIN       = gpsp

# Platform specific definitions 

VPATH      += .. ../arm ../gp2x
CFLAGS     += -DARM_ARCH -DCHIP_BUILD -Wall
CFLAGS     += -ffast-math
CFLAGS     += -ggdb
ifndef DEBUG
CFLAGS     += -O3 -funsafe-math-optimizations -ftree-vectorize
endif

# expecting to have PATH set up to get correct sdl-config first
CFLAGS     += `sdl-config --cflags`
LIBS       += `sdl-config --libs`
LIBS       += -lSDL_ttf -lSDL_image
LIBS       += -ldl -lpthread -lz

# Compilation:

all:	$(BIN)

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

main.o:
cpu.o cpu_threaded.o video.o: CFLAGS += -Wno-unused-variable -Wno-unused-label -fprefetch-loop-arrays

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $(BIN)

clean:
	rm -f *.o $(BIN) picorestore

# Installation

install: gpsp ../game_config.txt
	cp gpsp /usr/games/
	cp ../game_config.txt /usr/games/

# ----------- release -----------

rel: gpsp ../game_config.txt ../COPYING.DOC
	rm -rf out
	mkdir out
	cp $^ out/
