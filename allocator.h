#ifndef CONSOLETEXTEDITOR_ALLOCATOR_H
#define CONSOLETEXTEDITOR_ALLOCATOR_H

#include "globals.h"
#define PAGE_SIZE (1024 * 1024)

void* alloc(usize amt);
void dealloc(void* ptr);


//WARNING this will make all memory previously allocated INVALID
//ONLY USE IF YOU KNOW WHAT YOU'RE DOING
void clear_pages();

#endif