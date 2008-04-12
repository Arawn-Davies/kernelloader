/* Copyright (c) 2007 Mega Man */
#include "mmu.h"
#include "memory.h"
#include "stdio.h"
#include "stdint.h"
#include "panic.h"
#include "cp0register.h"

#define SYNCP() \
	__asm__ __volatile__("sync.p"::);

#define PAGE_SIZE 0x1000
#define PAGE_MASK (~(PAGE_SIZE - 1))

#define PAGE_CACHEABLE (3 << 3)
#define PAGE_UNCACHEABLE (2 << 3)
#define PAGE_UNCACHEABLE_ACCELERATED (7 << 3)
/** Page is writeable. */
#define PAGE_DIRTY (1 << 2)
/** Page is valid. */
#define PAGE_VALID (1 << 1)
/** Page is global. */
#define PAGE_GLOBAL (1 << 0)
/** Page size is 4 Kbyte. */
#define PAGE_4K (0 << 13)
/** Page size is 16 Kbyte. */
#define PAGE_16K (0x03 << 13)
/** Page size is 64 Kbyte. */
#define PAGE_64K (0x0f << 13)
/** Page size is 256 Kbyte. */
#define PAGE_256K (0x3f << 13)
/** Page size is 1 Mbyte. */
#define PAGE_1M (0xff << 13)
/** Page size is 4 Mbyte. */
#define PAGE_4M (0x3ff << 13)
/** Page size is 16 Mbyte. */
#define PAGE_16M (0xfff << 13)

uint32_t pagesize;

void mapMemory(uint32_t index, uint32_t vaddr,
	uint32_t paddr1, uint32_t flags1,
	uint32_t paddr2, uint32_t flags2,
	uint32_t pagesize)
{
	uint32_t asid;
	uint32_t pagemask;

	pagemask = pagesize | 0x1FFF;
	pagemask >>= 1;
	pagemask = ~pagemask;

	asid = 0;
	CP0_SET_PAGEMASK(pagesize);
	CP0_SET_INDEX(index);
	CP0_SET_ENTRYLO0((paddr1 & pagemask) >> (12 - 6) | flags1);
	CP0_SET_ENTRYLO1((paddr2 & pagemask) >> (12 - 6) | flags2);
	CP0_SET_ENTRYHI((vaddr & (pagemask << 1)) | asid);

	/* Set TLB: */
	__asm__ __volatile__("sync.p\n"
		"tlbwi\n"
		"sync.p\n");
}

/** Flush all TLBs. User space cannot be accessed after calling this function. */
void flush_tlbs(void)
{
	int entry;
	uint32_t pagesize;

	pagesize = PAGE_16M;
	/* Set wired to 0. */
	CP0_SET_WIRED(0);
	/* Set page mask to 4k. */
	CP0_SET_PAGEMASK(pagesize);
	/* Set entrylo0 to zero. */
	CP0_SET_ENTRYLO0(0);
	/* Set entrylo1 to zero. */
	CP0_SET_ENTRYLO1(0);
	/* Barrier. */
	SYNCP();

	pagesize |= 0x1FFF;
	pagesize++;
	//printf("Pagesize 0x%x.\n", pagesize);

	for (entry = 0; entry < 48; entry++) {
		/* Set entryhi. */
		CP0_SET_ENTRYHI(KSEG0 + entry * pagesize);
		/* Set index. */
		CP0_SET_INDEX(entry);
		/* Barrier. */
		SYNCP();
		/* Write tlb entry. */
		__asm__ __volatile__("tlbwi"::);
		/* Barrier. */
		SYNCP();
	}
}
void mmu_init_module(void)
{
	flush_tlbs();

#if 0
	/* XXX; No TLB handler setting WIRED makes no sense. */
	CP0_SET_WIRED(4);

	/* Just map complete 32 MByte memory to 0. */
	mapMemory(0, 0x00000000,
		0x00000000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x01000000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_16M);
	mapMemory(1, 0x10000000,
		0x10000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x11000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_16M);
	mapMemory(2, 0x12000000,
		0x12000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x00000000, 0,
		PAGE_16M);
	mapMemory(3, 0x1E000000,
		0x1E000000, PAGE_UNCACHEABLE | PAGE_VALID | PAGE_GLOBAL,
		0x1F000000, PAGE_UNCACHEABLE | PAGE_VALID | PAGE_GLOBAL,
		PAGE_16M);
#else
	/* XXX; No TLB handler setting WIRED makes no sense. */
	CP0_SET_WIRED(31);

	mapMemory(0, 0x70000000,
		0x00000000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x00000000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL, /* Strange mapping from PS2 kernel .*/
		PAGE_4K);

	if ((KSEG0 + ZERO_REG_ADDRESS) < (uint32_t) &_end) {
		panic("Kernel image is larger than ZERO_REG_ADDRESS.\n");
	}

	/* Required by exception handler. */
	mapMemory(0, 0xffff8000,
		ZERO_REG_ADDRESS, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		ZERO_REG_ADDRESS + 0x4000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_16K);

	/* EE hardware register mapping. */
	mapMemory(2, 0x10000000,
		0x10000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x10001000, PAGE_UNCACHEABLE | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(3, 0x10002000,
		0x10002000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x10003000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(4, 0x10004000,
		0x10004000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x10005000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(5, 0x10006000,
		0x10006000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x10007000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(6, 0x10008000,
		0x10008000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x10009000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(7, 0x1000a000,
		0x1000a000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1000b000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(8, 0x1000c000,
		0x1000c000, PAGE_UNCACHEABLE | PAGE_VALID | PAGE_GLOBAL,
		0x1000d000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);
	mapMemory(9, 0x1000e000,
		0x1000e000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1000f000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4K);

	mapMemory(10, 0x11000000,
		0x11000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x11010000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_GLOBAL, /* invalid */
		PAGE_64K);

	mapMemory(11, 0x12000000,
		0x12000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x12010000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_GLOBAL, /* invalid */
		PAGE_64K);

	mapMemory(12, 0x1e000000,
		0x1e000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1f000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_16M);

	/* User level memory 32 MB - 0.5 MB cached with offset 0x0. */
	mapMemory(13, 0x80000,
		0x80000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0xc0000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(14, 0x100000,
		0x100000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x140000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(15, 0x180000,
		0x180000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x180000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(16, 0x200000,
		0x200000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x300000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(17, 0x400000,
		0x400000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x500000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(18, 0x600000,
		0x600000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x700000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(19, 0x800000,
		0x800000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0xc00000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	mapMemory(20, 0x1000000,
		0x1000000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1400000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	mapMemory(21, 0x1800000,
		0x1800000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1c00000, PAGE_CACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	/* User level memory 32 MB - 0.5 MB uncached with offset 0x20000000. */
	mapMemory(22, 0x20080000,
		0x80000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0xc0000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(23, 0x20100000,
		0x100000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x140000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(24, 0x20180000,
		0x180000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x180000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(25, 0x20200000,
		0x200000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x300000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(26, 0x20400000,
		0x400000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x500000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(27, 0x20600000,
		0x600000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x700000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(28, 0x20800000,
		0x800000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0xc00000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	mapMemory(29, 0x21000000,
		0x1000000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1400000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	mapMemory(30, 0x21800000,
		0x1800000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1c00000, PAGE_UNCACHEABLE | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	/* User level memory 32 MB - 1 MB uncached accelerated with offset 0x30000000. */
	mapMemory(31, 0x30100000,
		0x100000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x140000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(32, 0x30180000,
		0x180000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1c0000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_256K);

	mapMemory(33, 0x30200000,
		0x200000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x300000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(34, 0x30400000,
		0x400000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x500000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(35, 0x30600000,
		0x600000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x700000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_1M);

	mapMemory(36, 0x30800000,
		0x800000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0xc00000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	mapMemory(37, 0x31000000,
		0x1000000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1400000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);

	mapMemory(38, 0x31800000,
		0x1800000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		0x1c00000, PAGE_UNCACHEABLE_ACCELERATED | PAGE_DIRTY | PAGE_VALID | PAGE_GLOBAL,
		PAGE_4M);
#endif
}
