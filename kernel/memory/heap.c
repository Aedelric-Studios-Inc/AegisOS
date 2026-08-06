/* SPDX-License-Identifier: Proprietary */
/* AegisOS — kernel/memory/heap.c
 * Simple kernel heap allocator (free-list).
 */

#include "memory.h"

typedef struct block_header {
    u64                   size;
    bool                  free;
    struct block_header  *next;
} block_header_t;

static block_header_t *heap_head = NULL;

void heap_init(virt_addr_t base, u64 size) {
    heap_head        = (block_header_t *)base;
    heap_head->size  = size - sizeof(block_header_t);
    heap_head->free  = true;
    heap_head->next  = NULL;
}

void *kmalloc(u64 size) {
    size = (size + 7) & ~(u64)7;  /* 8-byte align */
    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            /* Split block if large enough */
            if (cur->size >= size + sizeof(block_header_t) + 8) {
                block_header_t *new_block = (block_header_t *)((u8 *)cur + sizeof(block_header_t) + size);
                new_block->size = cur->size - size - sizeof(block_header_t);
                new_block->free = true;
                new_block->next = cur->next;
                cur->size = size;
                cur->next = new_block;
            }
            cur->free = false;
            return (void *)((u8 *)cur + sizeof(block_header_t));
        }
        cur = cur->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *hdr = (block_header_t *)((u8 *)ptr - sizeof(block_header_t));
    hdr->free = true;
    /* Coalesce adjacent free blocks */
    block_header_t *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(block_header_t) + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *krealloc(void *ptr, u64 new_size) {
    if (!ptr) return kmalloc(new_size);
    block_header_t *hdr = (block_header_t *)((u8 *)ptr - sizeof(block_header_t));
    if (hdr->size >= new_size) return ptr;
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;
    for (u64 i = 0; i < hdr->size; i++)
        ((u8 *)new_ptr)[i] = ((u8 *)ptr)[i];
    kfree(ptr);
    return new_ptr;
}
