/*
 * Part of Astonia Client (c) Daniel Brockhaus. Please read license.txt.
 *
 * Memory allocation abstraction layer
 *
 * Provides macros for selecting between mimalloc and standard libc allocators,
 * and declares the xmalloc/xfree family of functions for tracked memory allocation.
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

// Memory allocator selection: Use mimalloc by default, but can be disabled via USE_MIMALLOC=0 at build time
// Address sanitizer builds should set USE_MIMALLOC=0
#ifndef USE_MIMALLOC
#define USE_MIMALLOC 1
#endif

#if USE_MIMALLOC
#include <mimalloc.h>
#define MALLOC  mi_malloc
#define CALLOC  mi_calloc
#define REALLOC mi_realloc
#define FREE    mi_free
#define STRDUP  mi_strdup
#else
#define MALLOC  malloc
#define CALLOC  calloc
#define REALLOC realloc
#define FREE    free
#define STRDUP  strdup
#endif

// Memory tracking structure
struct memhead {
	size_t size;
	uint8_t ID;
};

// Memory allocation functions with tracking
void *xmalloc(size_t size, uint8_t ID);
void *xrealloc(void *ptr, size_t size, uint8_t ID);
void *xrecalloc(void *ptr, size_t size, uint8_t ID);
void xfree(void *ptr);
char *xstrdup(const char *src, uint8_t ID);
int xmemcheck(void *ptr);
void xinfo(void *ptr);
void list_mem(void);

// Memory statistics (exported for debugging/monitoring)
extern size_t memused;
extern int memptrused;
extern size_t maxmemsize;
extern int maxmemptrs;
extern int memptrs[];
extern size_t memsize[];

#endif // MEMORY_H
