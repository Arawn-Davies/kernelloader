/* Copyright (c) 2007 - 2009 Mega Man */
#include "gsKit.h"
#include "dmaKit.h"
#include "malloc.h"
#include "stdio.h"
#include "kernel.h"
#include "sio.h"

#include "config.h"
#include "menu.h"
#include "rom.h"
#include "graphic.h"
#include "loader.h"
#include "configuration.h"
#include "kprint.h"
#include <screenshot.h>


/** Maximum buffer size for error_printf(). */
#define MAX_BUFFER 128

/** Maximum buffer size of info message buffer. */
#define MAX_INFO_BUFFER 4096

/** Maximum number of error messages. */
#define MAX_MESSAGES 10

/** Maximum texture size in Bytes. */
#define MAX_TEX_SIZE 0x30000

/** Size of small textures (will be uploaded). */
#define SMALL_TEXT_SIZE 0x4000

/** GS_MODE_VGA_1280_75 is not used, use it as special value for autodetect. */
#define GS_AUTO_DETECT GS_MODE_VGA_1280_75

/** True, if graphic is initialized. */
static bool graphicInitialized = false;

static Menu *menu = NULL;

static Menu *mainMenu = NULL;

static bool enableDisc = 0;

/** gsGlobal is required for all painting functiions of gsKit. */
static GSGLOBAL *gsGlobal = NULL;

/** Colours used for painting. */
static u64 White, Black, Blue, Red;

/** Text colour. */
static u64 TexCol;

/** Red text colour. */
static u64 TexRed;

/** Black text colour. */
static u64 TexBlack;

/** Font used for printing text. */
static GSFONTM *gsFont;

/** File name that is printed on screen. */
static char loadName[26];

static const char *statusMessage = NULL;

/** Percentage for loading file shown as progress bar. */
static int loadPercentage = 0;

/** Scale factor for font. */
static float scale = 1.0f;

/** Ring buffer with error messages. */
static const char *errorMessage[MAX_MESSAGES] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/** Read pointer into ring buffer of error messages. */
static int readMsgPos = 0;

/** Write pointer into ring buffer of error messages. */
static int writeMsgPos = 0;

static GSTEXTURE *texFolder = NULL;

static GSTEXTURE *texUp = NULL;

static GSTEXTURE *texBack = NULL;

static GSTEXTURE *texSelected = NULL;

static GSTEXTURE *texUnselected = NULL;

static GSTEXTURE *texPenguin = NULL;

static GSTEXTURE *texDisc = NULL;

static GSTEXTURE *texCloud = NULL;

static int reservedEndOfDisplayY = 42;

static bool usePad = false;

static int emulatedKey = ' ';

int scrollPos = 0;

int inputScrollPos = 0;

u32 globalVram;

u32 modeList[] = {
	GS_AUTO_DETECT,
	GS_MODE_VGA_640_60,
	GS_MODE_VGA_640_72,
	GS_MODE_VGA_640_75,
	GS_MODE_VGA_640_85,
	GS_MODE_DTV_480P,
	GS_MODE_NTSC,
	GS_MODE_PAL,
};
int currentMode = 0;
int lastMode = 0;

int frequenzy[] = {
	0,
	60,
	72,
	75,
	85,
	60,
	60,
	50,
};

const char *modeDescription[] = {
	"Auto",
	"640x480 60Hz",
	"640x480 72Hz",
	"640x480 75Hz",
	"640x480 85Hz",
	"480P",
	"NTSC",
	"PAL",
};

extern "C" {
	int xoffset = 0;
	int yoffset = 0;
}

void check_screen_offsets(void)
{
	kprintf("Set screen mode to %ux%u\n", gsGlobal->Width, gsGlobal->Height);
	if (gsGlobal->Width > 640) {
		xoffset = (gsGlobal->Width - 640) / 2;
	} else {
		xoffset = 0;
	}
}

static void gsKit_texture_upload_inline(GSGLOBAL *gsGlobal, GSTEXTURE *Texture)
{
	static u32 *lastMem;
	static u32 lastVram;

	/* Check if already uploaded the last time. */
	if ((lastMem != Texture->Mem) || (lastVram != Texture->Vram)) {
		/* Texture was not uploaded, need to upload it. */
		gsKit_setup_tbw(Texture);
	
		if (Texture->PSM == GS_PSM_T8)
		{
			gsKit_texture_send_inline(gsGlobal, Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
			gsKit_texture_send_inline(gsGlobal, Texture->Clut, 16, 16, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);
	
		}
		else if (Texture->PSM == GS_PSM_T4)
		{
			gsKit_texture_send_inline(gsGlobal, Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_TEXTURE);
			gsKit_texture_send_inline(gsGlobal, Texture->Clut, 8,  2, Texture->VramClut, Texture->ClutPSM, 1, GS_CLUT_PALLETE);
		}
		else
		{
			gsKit_texture_send_inline(gsGlobal, Texture->Mem, Texture->Width, Texture->Height, Texture->Vram, Texture->PSM, Texture->TBW, GS_CLUT_NONE);
		}
	}
}

void paintTexture(GSTEXTURE *tex, int x, int y, int z)
{
	if (tex != NULL) {
		if (tex->Vram != GSKIT_ALLOC_ERROR) {
			u32 size;

			size = gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM);
			if (size <= SMALL_TEXT_SIZE) {
				/* No uploading required for small textures. */
				gsKit_prim_sprite_texture(gsGlobal, tex,
					x, y, 0, 0, x + tex->Width, y + tex->Height,
					tex->Width, tex->Height, z,
					GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
			} else {
				GSTEXTURE uploadTex;
				u32 slice;
				u32 offset;
				u32 vramSize;

				/* Need to upload texture, because it is too big for cache. */
				slice = tex->Height;
				vramSize = gsKit_texture_size(tex->Width, slice, tex->PSM);
				while (vramSize > MAX_TEX_SIZE) {
					slice = (slice + 1) >> 1;
					do {
						size = gsKit_texture_size_ee(tex->Width, slice, tex->PSM);
						vramSize = gsKit_texture_size(tex->Width, slice, tex->PSM);
						if (slice == 0) {
							kprintf("minimum vramSize 0x%08x\n", vramSize);
						}
						if (size & 15) {
							/* Get next 16 byte aligned size. */
							/* DMA will only support a start address which is 16 Byte aligned. */
							/* The start address of the next block is calculated by adding slice. */
							slice++;
						}
					} while (size & 15); /* Wait until size is 16 Byte aligned. */
				}
	
				uploadTex = *tex;
				uploadTex.Height = slice;
	
				/* Upload image as slices. */	
				for (offset = 0; offset < tex->Height; offset += slice) {
					u32 remaining;
	
					remaining = tex->Height - offset;
					if (remaining < slice) {
						uploadTex.Height = remaining;
					}
					gsKit_texture_upload_inline(gsGlobal, &uploadTex);
					gsKit_prim_sprite_texture(gsGlobal, &uploadTex,
						x, y + offset, 0, 0, x + uploadTex.Width, y + offset + uploadTex.Height,
						uploadTex.Width, uploadTex.Height, z,
						GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00));
					uploadTex.Mem = (u32 *) (((u32) uploadTex.Mem) + size);
				}
			}
		}
	}
}

static char infoBuffer[MAX_INFO_BUFFER];
static int infoBufferPos = 0;

static char *inputBuffer = NULL;
static int writeable = 0;
static int cursor_counter = 0;
static int cursorpos = 0;

int printTextBlock(int x, int y, int z, int maxCharsPerLine, int maxY, const char *msg, int scrollPos, int cursorpos, int cursor)
{
	char lineBuffer[maxCharsPerLine + 1]; /* + 1 for cursor */
	int i;
	int pos;
	int lastSpace;
	int lastSpacePos;
	int lineNo;
	int insertCursorPos;

	pos = 0;
	lineNo = 0;
	do {
		i = 0;
		lastSpace = -1;
		lastSpacePos = 0;
		insertCursorPos = -1;
		if (pos == cursorpos) {
			if (pos == 0) {
				insertCursorPos = i;
			}
		}
		while (i < maxCharsPerLine) {
			lineBuffer[i] = msg[pos];
			if (msg[pos] == 0) {
				lastSpace = i;
				lastSpacePos = pos;
				break;
			} else if (msg[pos] == '\r') {
				lineBuffer[i] = 0;
				lastSpace = i;
				lastSpacePos = pos + 1;
			} else if (msg[pos] == '\n') {
				lineBuffer[i] = 0;
				lastSpace = i;
				lastSpacePos = pos + 1;
				pos++;
				break;
			}
			if (i >= (maxCharsPerLine - 1)) {
				if (msg[pos] == ' ') {
					/* Last character is a space, show it at the beginning of the next line. */
					lastSpace = i;
					lastSpacePos = pos;
				}
				break;
			}
			if (msg[pos] == ' ') {
				/* Current character is a space. */
				lastSpace = i;
				lastSpacePos = pos + 1;
				i++;
			} else if (msg[pos] == '\r') {
				/* ignore */
			} else {
				i++;
			}
			pos++;
			if (pos == cursorpos) {
				insertCursorPos = i;
			}
		}
		if (lastSpace >= 0) {
			pos = lastSpacePos;
		} else {
			/* No whitespace in current line, cut off at last character in line. */
			lastSpace = i;
		}
		lineBuffer[lastSpace] = 0;
		if ((insertCursorPos >= 0) && (lastSpace >= insertCursorPos)) {
			char *a;
			char *c;

			a = &lineBuffer[insertCursorPos + 1], 
			c = &lineBuffer[lastSpace];
			for (c = &lineBuffer[lastSpace]; c >= &lineBuffer[insertCursorPos]; c--) {
				c[1] = c[0];
			}
			if (cursor) {
				lineBuffer[insertCursorPos] = '_';
			} else {
				lineBuffer[insertCursorPos] = emulatedKey;
			}
		}

		if (lineNo >= scrollPos) {
#if 0
			kprintf("Test pos %d i %d lastSpacePos %d %s\n", pos, i, lastSpacePos, lineBuffer);
#else
			gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + x, yoffset + y, z, scale, TexCol,
				lineBuffer);
#endif
			y += 30;
			if (y > (maxY - 30)) {
				break;
			}
		}
		lineNo++;
	} while(msg[pos] != 0);
	if (lineNo < scrollPos) {
		return lineNo;
	} else {
		return scrollPos;
	}
}

void graphic_common(void)
{
	gsKit_clear(gsGlobal, Blue);

	/* Paint background. */
	paintTexture(texCloud, xoffset + 0, yoffset + 0, 0);

	paintTexture(texPenguin, xoffset + 5, yoffset + 10, 1);

	gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 110, yoffset + 50, 3, scale, TexCol,
		"Kernelloader " LOADER_VERSION
#ifdef RESET_IOP
		"R"
#endif
#ifdef SCREENSHOT
		"S"
#endif
#ifdef NEW_ROM_MODULES
		"M"
#endif
#ifdef OLD_ROM_MODULES
		"O"
#endif
#ifdef SHARED_MEM_DEBUG
		"S"
#endif
	);
	gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 490, gsGlobal->Height - reservedEndOfDisplayY - 15, 3, 0.5, TexBlack,
		modeDescription[currentMode]);
	gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 490, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.5, TexBlack,
		"by Mega Man");
	gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 490, gsGlobal->Height - reservedEndOfDisplayY + 15, 3, 0.5, TexBlack,
		"UNSTABLE"
#ifdef RTE
		" RTE"
#endif
	);
}

/** Paint screen when Auto Boot is in process. */
void graphic_auto_boot_paint(int time)
{
	static char msg[80];

	if (!graphicInitialized) {
		return;
	}
	graphic_common();

	snprintf(msg, sizeof(msg), "Auto Boot in %d seconds.", time);
	gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
		msg);

	gsKit_queue_exec(gsGlobal);
	gsKit_finish(); /* Ensure that DMA has been finished before switching screen buffer. */
	gsKit_sync_flip(gsGlobal);
}

/** Paint current state on screen. */
void graphic_paint(void)
{
	const char *msg;

	if (!graphicInitialized) {
		return;
	}
	graphic_common();

	if (enableDisc) {
		paintTexture(texDisc, xoffset + 100, yoffset + 300, 40);
	}

	if (statusMessage != NULL) {
		gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, yoffset + 90, 3, scale, TexCol,
			statusMessage);
	} else if (loadName[0] != 0) {
		gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, yoffset + 90, 3, scale, TexCol,
			loadName);
		gsKit_prim_sprite(gsGlobal, xoffset + 50, yoffset + 120, xoffset + 50 + 520, yoffset + 140, 2, White);
		if (loadPercentage > 0) {
			gsKit_prim_sprite(gsGlobal, xoffset + 50, yoffset + 120,
				xoffset + 50 + (520 * loadPercentage) / 100, yoffset + 140, 2, Red);
		}
	}
	msg = getErrorMessage();
	if (msg != NULL) {
		gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, yoffset + 170, 3, scale, TexRed,
			"Error Message:");
		printTextBlock(50, 230, 3, 26, gsGlobal->Height - reservedEndOfDisplayY, msg, 0, -1, 0);
	} else {
		if (!isInfoBufferEmpty()) {
			scrollPos = printTextBlock(xoffset + 50, yoffset + 170, 3, 26, gsGlobal->Height - reservedEndOfDisplayY, infoBuffer, scrollPos, -1, 0);
		} else {
			if (inputBuffer != NULL) {
				inputScrollPos = printTextBlock(xoffset + 50, yoffset + 170, 3, 26, gsGlobal->Height - reservedEndOfDisplayY, inputBuffer, inputScrollPos, writeable ? cursorpos : -1, writeable && (cursor_counter < (getModeFrequenzy()/2)));
			} else if (menu != NULL) {
				menu->paint();
			}
		}
	}
	if (enableDisc) {
		gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
			"Loading, please wait...");
	} else {
		if (msg != NULL) {
			if (usePad) {
				gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
					"Press CROSS to continue.");
			}
		} else {
			if (!isInfoBufferEmpty()) {
				if (usePad) {
					gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
						"Press CROSS to continue.");
					gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY + 18, 3, 0.8, TexBlack,
						"Use UP and DOWN to scroll.");
				}
			} else {
				if (inputBuffer != NULL) {
					if (writeable) {
						gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
							"Please use USB keyboard.");
					}
					gsKit_fontm_print_scaled(gsGlobal, gsFont, 50, xoffset + gsGlobal->Height - reservedEndOfDisplayY + 18, 3, 0.8, TexBlack,
						"Press CROSS to quit.");
				} else if (menu != NULL) {
					if (usePad) {
						gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
							"Press CROSS to select menu.");
						gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY + 18, 3, 0.8, TexBlack,
							"Use UP and DOWN to scroll.");
					}
				}
			}
		}
		if (!usePad) {
			gsKit_fontm_print_scaled(gsGlobal, gsFont, xoffset + 50, gsGlobal->Height - reservedEndOfDisplayY, 3, 0.8, TexBlack,
				"Please wait...");
		}
	}
	gsKit_queue_exec(gsGlobal);
	gsKit_finish(); /* Ensure that DMA has been finished before switching screen buffer. */
	gsKit_sync_flip(gsGlobal);

	cursor_counter++;
	if (cursor_counter >= getModeFrequenzy()) {
		cursor_counter = 0;
	}
}

extern "C" {

	/**
	 * Set load percentage of file.
	 * @param percentage Percentage to set (0 - 100).
	 * @param name File name printed on screen.
	 */
	void graphic_setPercentage(int percentage, const char *name) {
		if (percentage > 100) {
			percentage = 100;
		}
		loadPercentage = percentage;

		if (name != NULL) {
			unsigned int len;

			len = strlen(name);
			if (len < sizeof(loadName)) {
				strcpy(loadName, name);
			} else {
				int r;
				int n;
				const char ellipse[] = "...";

				/* Name too long, show only start and end of string. */
				r = sizeof(loadName) - 1;
				n = (r - (sizeof(ellipse) - 1)) / 2;
				memcpy(loadName, name, n);
				r -= n;
				strcpy(&loadName[n], ellipse);
				r -= sizeof(ellipse) - 1;
				memcpy(&loadName[n + sizeof(ellipse) - 1], &name[len - r], r);
				loadName[sizeof(loadName) - 1] = 0;
			}
		} else {
			loadName[0] = 0;
		}
		graphic_paint();
	}

	/**
	 * Set status message.
	 * @param text Text displayed on screen.
	 */
	void graphic_setStatusMessage(const char *text) {
		if (text != NULL) {
			sio_printf("Status: %s\n", text);
		}
		statusMessage = text;
		graphic_paint();
	}
}

GSTEXTURE *getTexture(const char *filename)
{
	GSTEXTURE *tex = NULL;
	const rom_entry_t *romfile;
	romfile = rom_getFile(filename);
	if (romfile != NULL) {
		tex = (GSTEXTURE *) malloc(sizeof(GSTEXTURE));
		if (tex != NULL) {
			u32 size;

			tex->Width = romfile->width;
			tex->Height = romfile->height;
			if (romfile->depth == 4) {
				tex->PSM = GS_PSM_CT32;
			} else {
				tex->PSM = GS_PSM_CT24;
			}
			tex->Mem = (u32 *) romfile->start;
			tex->Filter = GS_FILTER_LINEAR;

			size = gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM);
			if (size <= SMALL_TEXT_SIZE) {
				tex->Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(tex->Width, tex->Height, tex->PSM), GSKIT_ALLOC_USERBUFFER);
				if (tex->Vram != GSKIT_ALLOC_ERROR) {
					gsKit_texture_upload(gsGlobal, tex);
				} else {
					kprintf("Out of VRAM \"%s\".\n", filename);
					free(tex);
					error_printf("Out of VRAM while loading texture (%s).", filename);
					return NULL;
				}
			} else {
				tex->Vram = globalVram;
			}
		} else {
			error_printf("Out of memory while loading texture (%s).", filename);
		}
	} else {
		error_printf("Failed to open texture \"%s\".", filename);
	}
	return tex;
}

void reallocTexture(GSTEXTURE *tex)
{
	u32 size;

	size = gsKit_texture_size_ee(tex->Width, tex->Height, tex->PSM);
	if (size <= SMALL_TEXT_SIZE) {
		tex->Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(tex->Width, tex->Height, tex->PSM), GSKIT_ALLOC_USERBUFFER);
		if (tex->Vram != GSKIT_ALLOC_ERROR) {
			gsKit_texture_upload(gsGlobal, tex);
		} else {
			kprintf("Out of VRAM.\n");
			error_printf("Out of VRAM while realloc texture.");
		}
	}
}

bool isNTSCMode(void)
{
	return (gsGlobal->Mode != GS_MODE_PAL);
}

int getCurrentMode(void)
{
#if 0
	return gsGlobal->Mode;
#else
	return modeList[currentMode];
#endif
}

/**
 * Initialize graphic screen.
 */
Menu *graphic_main(void)
{
	int i;
	int numberOfMenuItems;

	addConfigVideoItem("videomode", &currentMode);

	gsGlobal = gsKit_init_global();

	if (isNTSCMode()) {
		frequenzy[0] = 60;
	} else {
		frequenzy[0] = 50;
	}

	kprintf("Switching to %s\n", modeDescription[currentMode]);

	lastMode = currentMode;

	if (currentMode == 0) { 
		gsGlobal->Mode = gsKit_detect_signal();
	} else {
		gsGlobal->Mode = modeList[currentMode];
	}

	if (isNTSCMode()) {
		numberOfMenuItems = 7;
	} else {
		numberOfMenuItems = 8;
	}

	dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
		D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

	// Initialize the DMAC
	dmaKit_chan_init(DMA_CHANNEL_GIF);
	dmaKit_chan_init(DMA_CHANNEL_FROMSPR);
	dmaKit_chan_init(DMA_CHANNEL_TOSPR);

	Black = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00);
	White = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x00, 0x00);
	Blue = GS_SETREG_RGBAQ(0x10, 0x10, 0xF0, 0x00, 0x00);
	Red = GS_SETREG_RGBAQ(0xF0, 0x10, 0x10, 0x00, 0x00);

	TexCol = GS_SETREG_RGBAQ(0xFF, 0xFF, 0xFF, 0x80, 0x00);
	TexRed = GS_SETREG_RGBAQ(0xF0, 0x10, 0x10, 0x80, 0x00);
	TexBlack = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);

	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	gsGlobal->ZBuffering = GS_SETTING_OFF;

	gsKit_init_screen(gsGlobal);

	check_screen_offsets();

	gsFont = gsKit_init_fontm();
	if (gsKit_fontm_upload(gsGlobal, gsFont) != 0) {
		kprintf("Can't find any font to use\n");
		SleepThread();
	}

	globalVram = gsKit_vram_alloc(gsGlobal, MAX_TEX_SIZE, GSKIT_ALLOC_USERBUFFER);
	if (globalVram == GSKIT_ALLOC_ERROR) {
		kprintf("Failed to allocate texture buffer.\n");
		error_printf("Failed to allocate texture buffer.\n");
	}

	gsFont->Spacing = 0.8f;
	texFolder = getTexture("folder.rgb");
	texUp = getTexture("up.rgb");
	texBack = getTexture("back.rgb");
	texSelected = getTexture("selected.rgb");
	texUnselected = getTexture("unselected.rgb");
	texPenguin = getTexture("penguin.rgb");
	texDisc = getTexture("disc.rgb");
	texCloud = getTexture("cloud.rgb");

	mainMenu = menu = new Menu(gsGlobal, gsFont, numberOfMenuItems);
	menu->setPosition(50, 120);

	gsKit_mode_switch(gsGlobal, GS_ONESHOT);

	/* Activate graphic routines. */
	graphicInitialized = true;

	for (i = 0; i < 2; i++) {
		graphic_paint();
	}
	return menu;
}

void incrementMode(void)
{
	currentMode++;
	if (currentMode >= (int) (sizeof(modeList)/sizeof(modeList[0]))) {
		currentMode = 0;
	}
}

void decrementMode(void)
{
	currentMode--;
	if (currentMode < 0) {
		currentMode = (sizeof(modeList)/sizeof(modeList[0])) - 1;
	}
}

void setMode(int mode)
{
	if (mode < (int) (sizeof(modeList)/sizeof(modeList[0]))) {
		currentMode = mode;
	}
}

int getModeFrequenzy(void)
{
	return frequenzy[currentMode];
}

int setCurrentMenu(void *arg)
{
	Menu *newMenu = (Menu *) arg;

	menu = newMenu;

	return 0;
}

Menu *getCurrentMenu(void)
{
	return menu;
}

GSTEXTURE *getTexFolder(void)
{
	return texFolder;
}

GSTEXTURE *getTexUp(void)
{
	return texUp;
}

GSTEXTURE *getTexBack(void)
{
	return texBack;
}

GSTEXTURE *getTexSelected(void)
{
	return texSelected;
}

GSTEXTURE *getTexUnselected(void)
{
	return texUnselected;
}

extern "C" {
	void setErrorMessage(const char *text) {
		if (errorMessage[writeMsgPos] == NULL) {
			errorMessage[writeMsgPos] = text;
			writeMsgPos = (writeMsgPos + 1) % MAX_MESSAGES;
		} else {
			kprintf("Error message queue is full at error:\n");
			kprintf("%s\n", text);
		}
	}

	void goToNextErrorMessage(void)
	{
		if (errorMessage[readMsgPos] != NULL) {
			errorMessage[readMsgPos] = NULL;
			readMsgPos = (readMsgPos + 1) % MAX_MESSAGES;
		}
	}

	const char *getErrorMessage(void) {
		return errorMessage[readMsgPos];
	}

	int error_printf(const char *format, ...)
	{
		static char buffer[MAX_MESSAGES][MAX_BUFFER];
		int ret;
		va_list varg;
		va_start(varg, format);

		if (errorMessage[writeMsgPos] == NULL) {
			ret = vsnprintf(buffer[writeMsgPos], MAX_BUFFER, format, varg);

			sio_putsn(buffer[writeMsgPos]);

			setErrorMessage(buffer[writeMsgPos]);

			if (graphicInitialized) {
				if (readMsgPos == writeMsgPos) {
					/* Show it before doing anything else. */
					graphic_paint();
				}
			}
		} else {
			kprintf("error_printf loosing message: %s\n", format);
			ret = -1;
		}

		va_end(varg);
		return ret;
	}

	void info_prints(const char *text)
	{
		int len = strlen(text) + 1;
		int remaining;

		if (len > MAX_INFO_BUFFER) {
			kprintf("info_prints(): text too long.\n");
			return;
		}

		remaining = MAX_INFO_BUFFER - infoBufferPos;
		if (len > remaining) {
			int required;
			int i;

			/* required space in buffer. */
			required = len - remaining;

			/* Find next new line. */
			for (i = required; i < MAX_INFO_BUFFER; i++) {
				if (infoBuffer[i] == '\n') {
					i++;
					break;
				}
			}

			if (i >= MAX_INFO_BUFFER) {
				/* Delete complete buffer, buffer doesn't have any carriage returns. */
				infoBufferPos = 0;
			} else {
				/* Scroll buffer and delete old stuff. */
				for (i = 0; i < (infoBufferPos - required); i++) {
					infoBuffer[i] = infoBuffer[required + i];
				}
				infoBufferPos = infoBufferPos - required;
			}
			infoBufferPos -= required;
		}
		strcpy(&infoBuffer[infoBufferPos], text);
		infoBufferPos += len - 1;
	}

	int info_printf(const char *format, ...)
	{
		int ret;
		static char buffer[MAX_BUFFER];
		va_list varg;
		va_start(varg, format);

		ret = vsnprintf(buffer, MAX_BUFFER, format, varg);
		info_prints(buffer);

		va_end(varg);
		return ret;
	}

	void setEnableDisc(int v)
	{
		enableDisc = v;

		/* Show it before doing anything else. */
		graphic_paint();
	}

	void scrollUpFast(void)
	{
		int i;

		for (i = 0; i < 8; i++) {
			scrollUp();
		}
	}

	void scrollUp(void)
	{
		if (inputBuffer != NULL) {
			inputScrollPos--;
			if (inputScrollPos < 0) {
				inputScrollPos = 0;
			}
		} else {
			scrollPos--;
			if (scrollPos < 0) {
				scrollPos = 0;
			}
		}
	}

	void scrollDownFast(void)
	{
		int i;

		for (i = 0; i < 8; i++) {
			scrollDown();
		}
	}

	void scrollDown(void)
	{
		if (inputBuffer != NULL) {
			inputScrollPos++;
		} else {
			scrollPos++;
		}
	}

	int getScrollPos(void)
	{
		return scrollPos;
	}

	int isInfoBufferEmpty(void)
	{
		return !(loaderConfig.enableEEDebug && (infoBufferPos > 0));
	}

	void clearInfoBuffer(void)
	{
		infoBuffer[0] = 0;
		scrollPos = 0;
		infoBufferPos = 0;

		if (graphicInitialized) {
			/* Show it before doing anything else. */
			graphic_paint();
		}
	}
	void enablePad(int val)
	{
		usePad = val;
	}

	void setInputBuffer(char *buffer, int w)
	{
		scrollPos = 0;
		inputScrollPos = 0;
		inputBuffer = buffer;
		writeable = w;
		if (writeable) {
			cursorpos = strlen(inputBuffer);
		} else {
			cursorpos = 0;
		}
	}

	char *getInputBuffer(void)
	{
		return inputBuffer;
	}

	int isWriteable(void)
	{
		return writeable;
	}

	void graphic_screenshot(void)
	{
		static int screenshotCounter = 0;
		char text[256];

#ifdef RESET_IOP
		snprintf(text, 256, "mass0:kloader%d.tga", screenshotCounter);
#else
		snprintf(text, 256, "host:kloader%d.tga", screenshotCounter);
#endif
		ps2_screenshot_file(text, gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1],
			gsGlobal->Width, gsGlobal->Height / 2, gsGlobal->PSM);
		screenshotCounter++;

		/* Fix deadlock in gsKit. */
		gsGlobal->FirstFrame = GS_SETTING_ON;
	}

	void moveScreen(int dx, int dy)
	{
		gsGlobal->StartX += dx;
		gsGlobal->StartX &= 0xFFF;
		gsGlobal->StartY += dy;
		gsGlobal->StartY &= 0xFFF;
	
		GS_SET_DISPLAY1(gsGlobal->StartX,		// X position in the display area (in VCK unit
				gsGlobal->StartY,		// Y position in the display area (in Raster u
				gsGlobal->MagH,			// Horizontal Magnification
				gsGlobal->MagV,			// Vertical Magnification
				gsGlobal->DW - 1,	// Display area width
				gsGlobal->DH - 1);		// Display area height
	
		GS_SET_DISPLAY2(gsGlobal->StartX,		// X position in the display area (in VCK units)
				gsGlobal->StartY,		// Y position in the display area (in Raster units)
				gsGlobal->MagH,			// Horizontal Magnification
				gsGlobal->MagV,			// Vertical Magnification
				gsGlobal->DW - 1,	// Display area width
				gsGlobal->DH - 1);		// Display area height
	}

	void changeMode(void)
	{
		int i;
		int numberOfMenuItems;

		/* Range check by decrement and increment. */
		decrementMode();
		incrementMode();

		kprintf("Switching to %s\n", modeDescription[currentMode]);
		if (lastMode == currentMode) {
			/* Nothing to do. */
			return;
		}

		lastMode = currentMode;

		gsKit_deinit_global(gsGlobal);

		/* XXX: gsKit has no function to free allocated memory for fonts. */	
		if (gsFont->Header.offset_table != NULL) {
			free(gsFont->Header.offset_table);
			gsFont->Header.offset_table = NULL;
		}
		if (gsFont->TexBase != NULL) {
			free(gsFont->TexBase);
			gsFont->TexBase = NULL;
		}
		if (gsFont->Texture->Clut != NULL) {
			free(gsFont->Texture->Clut);
			gsFont->Texture->Clut = NULL;
		}

		gsGlobal = gsKit_init_global();

	
		if (currentMode == 0) { 
			gsGlobal->Mode = gsKit_detect_signal();
		} else {
			gsGlobal->Mode = modeList[currentMode];
		}
	
		if (isNTSCMode()) {
			numberOfMenuItems = 7;
		} else {
			numberOfMenuItems = 8;
		}
	
		dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
			D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
	
		// Initialize the DMAC
		dmaKit_chan_init(DMA_CHANNEL_GIF);
		dmaKit_chan_init(DMA_CHANNEL_FROMSPR);
		dmaKit_chan_init(DMA_CHANNEL_TOSPR);
	
		gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
		gsGlobal->ZBuffering = GS_SETTING_OFF;
	
		gsKit_init_screen(gsGlobal);

		check_screen_offsets();

		if (gsGlobal->Width > 640) {
			xoffset = (gsGlobal->Width - 640) / 2;
		} else {
			xoffset = 0;
		}
	
		if (gsKit_fontm_upload(gsGlobal, gsFont) != 0) {
			kprintf("Can't find any font to use\n");
			SleepThread();
		}
	
		globalVram = gsKit_vram_alloc(gsGlobal, MAX_TEX_SIZE, GSKIT_ALLOC_USERBUFFER);
		if (globalVram == GSKIT_ALLOC_ERROR) {
			kprintf("Failed to allocate texture buffer.\n");
			error_printf("Failed to allocate texture buffer.\n");
		}
	
		gsFont->Spacing = 0.8f;
		reallocTexture(texFolder);
		reallocTexture(texUp);
		reallocTexture(texBack);
		reallocTexture(texSelected);
		reallocTexture(texUnselected);
		reallocTexture(texPenguin);
		reallocTexture(texDisc);
		reallocTexture(texCloud);

		if (mainMenu != NULL) {	
			mainMenu->reset(gsGlobal, gsFont, numberOfMenuItems);
			mainMenu->setPosition(50, 120);
		}
	
		gsKit_mode_switch(gsGlobal, GS_ONESHOT);
	
		/* Activate graphic routines. */
		graphicInitialized = true;
	
		for (i = 0; i < 2; i++) {
			graphic_paint();
		}
	}


}

int getCursorPos(void)
{
	return cursorpos;
}

void incCursorPos(void)
{
	if (cursorpos < (int) strlen(inputBuffer)) {
		cursorpos++;
	}
}

void decCursorPos(void)
{
	if (cursorpos > 0) {
		cursorpos--;
	}
}

void homeCursorPos(void)
{
	cursorpos = 0;
}

void endCursorPos(void)
{
	cursorpos = strlen(inputBuffer);
}

void setEmulatedKey(int key)
{
	emulatedKey = key;
}
