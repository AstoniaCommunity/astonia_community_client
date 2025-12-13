/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Memory allocation implementation
 *
 * Provides tracked memory allocation (xmalloc/xfree) and allocator selection
 * macros (MALLOC/FREE) for choosing between mimalloc and standard libc.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "astonia.h"
#include "game/memory.h"
#include "sdl/sdl.h"

// Memory statistics
size_t memused = 0;
int memptrused = 0;

size_t maxmemsize = 0;
int maxmemptrs = 0;
int memptrs[MAX_MEM];
size_t memsize[MAX_MEM];

// Memory check tracking
static size_t memcheckset = 0;
static char memcheck[256];

// TODO: removed unused memory areas
static char *memname[MAX_MEM] = {"MEM_TOTA", // 0
    "MEM_GLOB", "MEM_TEMP", "MEM_ELSE", "MEM_DL",
    "MEM_IC", // 5
    "MEM_SC", "MEM_VC", "MEM_PC", "MEM_GUI",
    "MEM_GAME", // 10
    "MEM_TEMP11", "MEM_VPC", "MEM_VSC", "MEM_VLC", "MEM_SDL_BASE", "MEM_SDL_PIXEL", "MEM_SDL_PNG", "MEM_SDL_PIXEL2",
    "MEM_TEMP5", "MEM_TEMP6", "MEM_TEMP7", "MEM_TEMP8", "MEM_TEMP9", "MEM_TEMP10"};

// External reference to xmemcheck_failed (defined in main.c)
extern int xmemcheck_failed;

static void update_mem_stats_add(uint8_t ID, size_t size)
{
	memsize[ID] += size;
	memptrs[ID] += 1;
	memsize[0] += size;
	memptrs[0] += 1;

	if (memsize[0] > maxmemsize) {
		maxmemsize = memsize[0];
	}
	if (memptrs[0] > maxmemptrs) {
		maxmemptrs = memptrs[0];
	}

	memused += sizeof(struct memhead) + sizeof(memcheck) + size + sizeof(memcheck);
	memptrused++;
}

static void update_mem_stats_remove(uint8_t ID, size_t size)
{
	memsize[ID] -= size;
	memptrs[ID] -= 1;
	memsize[0] -= size;
	memptrs[0] -= 1;

	memused -= sizeof(struct memhead) + sizeof(memcheck) + size + sizeof(memcheck);
	memptrused--;
}

int xmemcheck(void *ptr)
{
	struct memhead *mem;
	unsigned char *head, *tail, *rptr;

	if (!ptr) {
		return 0;
	}

	{
		uintptr_t ptr_val = (uintptr_t)ptr;
		ptr_val -= sizeof(struct memhead) + sizeof(memcheck);
		mem = (struct memhead *)ptr_val;
	}

	// ID check
	if (mem->ID >= MAX_MEM) {
		note("xmemcheck: ill mem id (%d)", mem->ID);
		xmemcheck_failed = 1;
		return -1;
	}

	// border check
	head = ((unsigned char *)(mem)) + sizeof(struct memhead);
	rptr = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck);
	tail = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck) + mem->size;

	if (memcmp(head, memcheck, sizeof(memcheck))) {
		fail("xmemcheck: ill head in %s (ptr=%p)", memname[mem->ID], (void *)rptr);
		xmemcheck_failed = 1;
		return -1;
	}
	if (memcmp(tail, memcheck, sizeof(memcheck))) {
		fail("xmemcheck: ill tail in %s (ptr=%p)", memname[mem->ID], (void *)rptr);
		xmemcheck_failed = 1;
		return -1;
	}

	return 0;
}

void *xmalloc(size_t size, uint8_t ID)
{
	struct memhead *mem;
	unsigned char *head, *tail, *rptr;

	if (!memcheckset) {
		for (memcheckset = 0; memcheckset < sizeof(memcheck); memcheckset++) {
			memcheck[memcheckset] = (char)rrand(256);
		}
		sprintf(memcheck, "!MEMCKECK MIGHT FAIL!");
	}

	if (!size) {
		return NULL;
	}

	memptrused++;

	mem = CALLOC(1, sizeof(struct memhead) + sizeof(memcheck) + size + sizeof(memcheck));
	if (!mem) {
		fail("OUT OF MEMORY !!!");
		return NULL;
	}

	memused += sizeof(struct memhead) + sizeof(memcheck) + size + sizeof(memcheck);

	if (ID >= MAX_MEM) {
		fail("xmalloc: ill mem id");
		return NULL;
	}

	mem->ID = ID;
	mem->size = size;
	memsize[mem->ID] += mem->size;
	memptrs[mem->ID] += 1;
	memsize[0] += mem->size;
	memptrs[0] += 1;

	if (memsize[0] > maxmemsize) {
		maxmemsize = memsize[0];
	}
	if (memptrs[0] > maxmemptrs) {
		maxmemptrs = memptrs[0];
	}

	head = ((unsigned char *)(mem)) + sizeof(struct memhead);
	rptr = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck);
	tail = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck) + mem->size;

	// set memcheck
	memcpy(head, memcheck, sizeof(memcheck));
	memcpy(tail, memcheck, sizeof(memcheck));

	xmemcheck(rptr);

	return rptr;
}

char *xstrdup(const char *src, uint8_t ID)
{
	size_t src_len;
	int size;
	char *dst;

	src_len = strlen(src);
	if (src_len > INT_MAX - 1) {
		return NULL;
	}
	size = (int)(src_len + 1);

	dst = xmalloc((size_t)size, ID);
	if (!dst) {
		return NULL;
	}

	memcpy(dst, src, (size_t)size);

	return dst;
}

void xfree(void *ptr)
{
	struct memhead *mem;

	if (!ptr) {
		return;
	}
	if (xmemcheck(ptr)) {
		return;
	}

	// get mem
	{
		uintptr_t ptr_val = (uintptr_t)ptr;
		ptr_val -= sizeof(struct memhead) + sizeof(memcheck);
		mem = (struct memhead *)ptr_val;
	}

	update_mem_stats_remove(mem->ID, (size_t)mem->size);

	// free
	FREE(mem);
}

void xinfo(void *ptr)
{
	struct memhead *mem;

	if (!ptr) {
		printf("NULL");
		return;
	}
	if (xmemcheck(ptr)) {
		printf("ILL");
		return;
	}

	// get mem
	{
		uintptr_t ptr_val = (uintptr_t)ptr;
		ptr_val -= sizeof(struct memhead) + sizeof(memcheck);
		mem = (struct memhead *)ptr_val;
	}

	printf("%zu bytes", mem->size);
}

// Verify struct size
_Static_assert(sizeof(struct memhead) == 16, "memhead must be 16 bytes");

void *xrealloc(void *ptr, size_t size, uint8_t ID)
{
	struct memhead *mem;
	unsigned char *head, *tail, *rptr;

	if (!ptr) {
		if (size > INT_MAX) {
			return NULL;
		}
		return xmalloc(size, ID);
	}
	if (!size) {
		xfree(ptr);
		return NULL;
	}
	if (xmemcheck(ptr)) {
		return NULL;
	}

	{
		uintptr_t ptr_val = (uintptr_t)ptr;
		ptr_val -= sizeof(struct memhead) + sizeof(memcheck);
		mem = (struct memhead *)ptr_val;
	}

	uint8_t old_ID = mem->ID;
	size_t old_size = mem->size;
	update_mem_stats_remove(old_ID, old_size);

	// realloc
	struct memhead *new_mem = REALLOC(mem, sizeof(struct memhead) + sizeof(memcheck) + size + sizeof(memcheck));
	if (!new_mem) {
		// Restore counters since realloc failed
		update_mem_stats_add(old_ID, old_size);
		fail("xrealloc: OUT OF MEMORY !!!");
		return NULL;
	}
	mem = new_mem;

	if (size > INT_MAX) {
		fail("xrealloc: size too large");
		return NULL;
	}
	update_mem_stats_add(ID, size);
	mem->ID = ID; // Update ID in case it changed
	mem->size = size;

	head = ((unsigned char *)(mem)) + sizeof(struct memhead);
	rptr = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck);
	tail = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck) + mem->size;

	// set memcheck
	memcpy(head, memcheck, sizeof(memcheck));
	memcpy(tail, memcheck, sizeof(memcheck));

	return rptr;
}

void *xrecalloc(void *ptr, size_t size, uint8_t ID)
{
	struct memhead *mem;
	unsigned char *head, *tail, *rptr;

	if (!ptr) {
		return xmalloc(size, ID);
	}
	if (!size) {
		xfree(ptr);
		return NULL;
	}
	if (xmemcheck(ptr)) {
		return NULL;
	}

	{
		uintptr_t ptr_val = (uintptr_t)ptr;
		ptr_val -= sizeof(struct memhead) + sizeof(memcheck);
		mem = (struct memhead *)ptr_val;
	}

	uint8_t old_ID = mem->ID;
	size_t old_size = mem->size;
	update_mem_stats_remove(old_ID, old_size);

	// realloc
	struct memhead *new_mem = REALLOC(mem, sizeof(struct memhead) + sizeof(memcheck) + size + sizeof(memcheck));
	if (!new_mem) {
		// Restore counters since realloc failed
		update_mem_stats_add(old_ID, old_size);
		fail("xrecalloc: OUT OF MEMORY !!!");
		return NULL;
	}
	mem = new_mem;

	if (size - old_size > 0) {
		bzero(
		    ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck) + old_size, (size_t)(size - old_size));
	}

	update_mem_stats_add(ID, size);
	mem->ID = ID; // Update ID in case it changed
	mem->size = size;

	head = ((unsigned char *)(mem)) + sizeof(struct memhead);
	rptr = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck);
	tail = ((unsigned char *)(mem)) + sizeof(struct memhead) + sizeof(memcheck) + mem->size;

	// set memcheck
	memcpy(head, memcheck, sizeof(memcheck));
	memcpy(tail, memcheck, sizeof(memcheck));

	return rptr;
}

void list_mem(void)
{
	int i, flag = 0;
	long long mem_tex = sdl_get_mem_tex();

	note("--mem----------------------");
	for (i = 1; i < MAX_MEM; i++) {
		if (memsize[i] || memptrs[i]) {
			flag = 1;
			note("%s %.2fMB in %d ptrs", memname[i], (double)memsize[i] / (1024.0 * 1024.0), memptrs[i]);
		}
	}
	if (flag) {
		note("%s %.2fMB in %d ptrs", memname[0], (double)memsize[0] / (1024.0 * 1024.0), memptrs[0]);
	}
	note("%s %.2fMB in %d ptrs", "MEM_MAX", (double)maxmemsize / (1024.0 * 1024.0), maxmemptrs);
	note("---------------------------");
	note("Texture Cache: %.2fMB", (double)mem_tex / (1024.0 * 1024.0));

	note("UsedMem=%.2fG of %.2fG", (double)((long long)memused + mem_tex) / 1024.0 / 1024.0 / 1024.0,
	    (double)get_total_system_memory() / 1024.0 / 1024.0 / 1024.0);
}
