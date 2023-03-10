PS2LINUXDVD = /media/cdrom
TARGET_IP = 192.168.0.23
EXAMPLE_ELF = ../hello/hello.elf

# Set debug output type:
# 1. fileio - SIF RPC stdout
# 2. callback - Call function registered by Linux kernel (printk, dmesg).
# sharedmem.irx via ps2link will be used if loaded
DEBUG_OUTPUT_TYPE = sio

# Reset IOP at start (only working when enabled)
RESET_IOP = yes

# Activate ps2link debug modules in kernelloader (has only effect when IOP
# reset is done):
LOAD_PS2LINK = no

# Use new ROM modules in loader.
NEW_ROM_MODULES = no

# Press button "R1" to get a screenshot on "host:" or "mass0:".
SCREENSHOT = yes

# Activate debug for SBIOS (has only effect with shared memory debug or callback debug or sio).
SBIOS_DEBUG = no

# Activate to be able to move the screen with the analog stick.
PAD_MOVE_SCREEN = yes

# Choose toolchain for simple kernel example:
NEW_KERNEL_TOOLCHAIN = no

SHARED_MEM_DEBUG = yes

### Don't change the following part, change DEBUG_OUTPUT_TYPE instead.

ifeq ($(DEBUG_OUTPUT_TYPE),fileio)
# SIF RPC stdout
FILEIO_DEBUG = yes
else
FILEIO_DEBUG = no
endif

ifeq ($(DEBUG_OUTPUT_TYPE),callback)
# Activate printf callback in SBIOS
CALLBACK_DEBUG = yes
else
CALLBACK_DEBUG = no
endif

ifeq ($(DEBUG_OUTPUT_TYPE),sio)
# Activate SIO
SIO_DEBUG = yes
else
SIO_DEBUG = no
endif
