/* Copyright (c) 2007 Mega Man */
#include <kernel.h>
#include <sifrpc.h>

#include "iopprintdata.h"
#include "graphic.h"

static void print_iop_data(iop_text_data_t *data, void *arg)
{
	/* Be sure that it is zero terminated string. */
	info_prints(data->text);
}

void addEEDebugHandler(void)
{
		/* Register eedebug handler. */
		SifAddCmdHandler(0x00000010, print_iop_data, NULL);
}
