# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004.
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_OBJS_DIR = obj/
IOP_BIN_DIR = bin/
IOP_SRC_DIR = src/
IOP_INC_DIR = include/

IOP_BIN  = bin/dmarelay.irx
IOP_OBJS = obj/main.o obj/ata.o obj/smap.o obj/imports.o

IOP_CFLAGS  += -Wall -fno-builtin-printf -fno-builtin-memcpy -Wimplicit-int -Werror-implicit-function-declaration -Werror
IOP_LDFLAGS += -s

install: all
	mkdir -p ../../../loader/TGE
	install -m 644 -p $(IOP_BIN) ../../../loader/TGE

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release
