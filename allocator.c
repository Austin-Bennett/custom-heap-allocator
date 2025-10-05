#include "allocator.h"

#include <stdio.h>

#include "utils.h"
#include "windows.h"




//the linked list structure keeping track of allocated memory
typedef struct mem_node {
    usize size;
    struct mem_node* next;
} mem_node;

//a page is 1 MB of memory
//its also a linked list structure pointing to the next page
typedef struct page {
    mem_node* allocations;
    struct page* next;
} page;

#define NODE_ALLOC(node) ((u8*)node + sizeof(mem_node))

page* pages = nullptr;

void add_page() {
    page* result = (page*)VirtualAlloc(
        nullptr,
        PAGE_SIZE,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!result) {
        elog("Failed to allocate page with error: %lu", GetLastError());
        abort();
    }


    result->next = nullptr;
    //the allocations start at just past the pages data
    result->allocations = (mem_node*)((u8*)result + sizeof(page));
    result->allocations->next = nullptr;
    result->allocations->size = 0;

    page** store = &pages;
    while (*store) {
        store = &((*store)->next);
    }
    *store = result;
}

void clear_pages() {
    //consume the pages
    while (pages) {
        page* cur = pages;
        pages = pages->next;
        if (!VirtualFree(cur, 0, MEM_RELEASE)) {
            elog("Failed to free memory %p with error %lu", cur, GetLastError());
            abort();
        }
    }
}

void* alloc(usize amt) {
    if (amt + sizeof(mem_node) > PAGE_SIZE) {
        //right now we dont support pages with more than 1 MB of data
        elog("Failed to allocate memory because requested size was greater than supported page size\n"
             "Note: requested %zu bytes which is %zu bytes more than page size %d", amt, amt-PAGE_SIZE, PAGE_SIZE);
    }
    if (!pages) {
        add_page();
    }

    page* cur_page = pages;
    while (cur_page) {
        //try to find somewhere to allocate this memory
        mem_node* cur_node = cur_page->allocations;
        mem_node* store = nullptr;
        while (cur_node->next) {
            //the amount of bytes between this nodes allocation and the next node
            usize diff = (u8*)cur_node->next - NODE_ALLOC(cur_node) - cur_node->size;

            //if the space between this nodes allocations and the next is great enough to store amt bytes
            //or this node is empty and theres enough space between it and the next node to store amt bytes
            //since we can use this nodes space instead
            if (diff >= amt + sizeof(mem_node) || !cur_node->size && diff >= amt) {
                if (!cur_node->size) {
                    store = cur_node;
                } else {
                    store = (mem_node*)((u8*)cur_node + sizeof(mem_node) + cur_node->size);
                }
                break;
            }
            cur_node = cur_node->next;
        }

        if (!store) {
            if (!cur_node->size) {
                store = cur_node;
            } else {
                store = (mem_node*)((u8*)cur_node + sizeof(mem_node) + cur_node->size);
            }
        }

        //first check that we actually can store the data without exceeding the page size
        if ((u8*)cur_page + PAGE_SIZE - (u8*)store < amt || (u8*)store > (u8*)cur_page + PAGE_SIZE) {
            if (!cur_page->next) add_page();
            cur_page = cur_page->next;
            continue;
        }
        //otherwise return this memory
        store->next = cur_node->next;
        store->size = amt;

        if (cur_node != store) cur_node->next = store;

        return (void*)NODE_ALLOC(store);
    }
    //if we somehow get here, something has gone horribly wrong and we should burn it all down
    elog("Failed to allocate memory.\nLast error: %lu", GetLastError());
    abort();
}


void dealloc(void* mem) {
    //look for this pointer in our pages
    page* cur_page = pages;
    page* prev_page = nullptr;
    while (cur_page) {
        mem_node* cur_node = cur_page->allocations;
        mem_node* prev = nullptr;

        while (cur_node) {
            if (NODE_ALLOC(cur_node) == mem) {
                //we found the node
                if (cur_node->size == 0) {
                    //must be a double free, time to burn
                    elog("Double free detected at pointer %p, aborting...", mem);
                    abort();
                }

                if (prev) {
                    //we can just remove this node from the list
                    prev->next = cur_node->next;
                } else {
                    //we just have to mark the front as empty
                    cur_node->size = 0;
                }

                if (cur_page->allocations->next == nullptr && cur_page->allocations->size == 0) {
                    //free this page
                    page* to_free = cur_page;
                    if (prev_page) prev_page->next = to_free->next;
                    else pages = to_free->next;
                    if (!VirtualFree(to_free, 0, MEM_RELEASE)) {
                        elog("Failed to free page at %p with error: %lu\n", to_free, GetLastError());
                        abort();
                    }
                }
                return;
            }
            prev = cur_node;
            cur_node = cur_node->next;
        }

        prev_page = cur_page;
        cur_page = cur_page->next;
    }

    //we couldnt find the memory
    elog("Failed to free pointer %p, possibly double-freed or not allocated by this allocator", mem);
    abort();
}