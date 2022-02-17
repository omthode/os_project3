#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "gompeimalloc.h"

void *_arena_start;
int statusno;

int init(size_t size) {
    if (size < 0 || size > MAX_ARENA_SIZE) {
        statusno = ERR_BAD_ARGUMENTS;
        return statusno;
    }
    int page_size = getpagesize();
    printf("Initializing arena\n...requesting size %lu bytes\n...pagesize is %d bytes\n...adjusting size with page boundaries\n", size, page_size);

    size = (((size - 1) / page_size) * page_size) + page_size;
    printf("...adjusted page size is %lu\n...mapping arena with mmap()\n", size);

    int fd=open("/dev/zero",O_RDWR);
    _arena_start = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);


    printf("...arena starts at %p\n...arena ends at %p\n", _arena_start, _arena_start+size);

    printf("...initializing header for initial free chunk\n");

    node_t *list_head;
    list_head = (node_t *) _arena_start;
    list_head->is_free = 1;
    list_head->fwd = NULL;
    list_head->bwd = NULL;
    list_head->size = size - sizeof(node_t);

    printf("...header size is %lu bytes\n", sizeof(node_t));
    
    return size;
}
int destroy() {
    if(_arena_start == (void *) NULL) {
        statusno = ERR_UNINITIALIZED;
        return statusno;
    }
    printf("Destroying Arena:\n...unmapping arena with munmap()\n");
    int size = ((node_t *) _arena_start)->size;
    int status = munmap(_arena_start, size);

    _arena_start = (void *) NULL;

    return statusno;
}

void* walloc(size_t size) {
    if(_arena_start == (void *) NULL) {
        statusno = ERR_UNINITIALIZED;
        return NULL;
    }

    int found = 0;

    printf("Allocating memory:\n...looking for memory of >= %lu bytes\n", size);
    node_t *curr_node = (node_t *) _arena_start;
    while (curr_node != NULL) {
        if (curr_node->is_free) {
            if (curr_node->size >= size) {
                found = 1;
                break;
            }
        }
        curr_node = curr_node->fwd;
    }
    if (found == 0) {
        statusno = ERR_OUT_OF_MEMORY;
        return NULL;
    }
    printf("...found free chunk of %lu bytes with header at %p\n", curr_node->size, curr_node);

    printf("...free chunk->fwd currently points to %p\n", curr_node->fwd);
    printf("...free chunk->bwd currently points to %p\n...checking if splitting is required\n", curr_node->bwd);
    if (curr_node->size > (size+sizeof(node_t))) {
        // split chunk
        printf("...splitting free chunk\n");
        // create new node
        node_t* split_node = (node_t*) (((void*) curr_node) + sizeof(node_t) + size);

        // define new node
        split_node->fwd = curr_node->fwd;
        split_node->bwd = curr_node;
        split_node->is_free = 1;
        split_node->size = curr_node->size - (size + sizeof(node_t));

        // handle node chaining
        if(curr_node->fwd != NULL) curr_node->fwd->bwd = split_node;
        curr_node->fwd = split_node;
        curr_node->size = size;
        
    }
    else {
        printf("...splitting not required\n");
    }
    printf("...updating chunk header at %p\n", curr_node);
    curr_node->is_free = 0;
    printf("...being careful with my pointer arithmetic and void pointer casting\n");
    node_t *alloc = curr_node + 1;
    printf("...allocation starts at %p\n", alloc);
    return (void *) alloc;
}

void wfree(void *ptr) {
    printf("Freeing allocated memory:\n...supplied pointer %p:\n...being careful with my pointer arithmetic and void pointer casting\n", ptr);
    node_t *header = (node_t *) (ptr - sizeof(node_t));
    printf("...accessing chunk header at %p\n...chunk of size %lu\n", header, header->size);
    header->is_free = 1;
    printf("...checking if coalescing is needed\n");
    // logic to check for coalescing
    int coalescing_case = 0;
    if (header->bwd != NULL) {
        if (header->bwd->is_free == 1) {
            coalescing_case = 2;
            header->bwd->fwd = header->fwd;
            if (header->fwd != NULL) {
                header->fwd->bwd = header->bwd;
            }
            header->bwd->size += header->size + sizeof(node_t);
            header = header->bwd;
        }
    }
    if (header->fwd != NULL) {
        if (header->fwd->is_free == 1) {
            coalescing_case = (coalescing_case + 3) % 4;
            header->size += header->fwd->size + sizeof(node_t);
            header->fwd = header->fwd->fwd;
            if (header->fwd != NULL) {
                header->fwd->bwd = header;
            }
        }
    }
    switch(coalescing_case) {
        case 1:
            printf("...col. case 1: previous, current, and next chunks all free.\n");
            break;
        case 2:
            printf("...col. case 2: previous and current chunks free.\n");
            break;
        case 3:
            printf("...col. case 3: current and next chunks free.\n");
            break;
        default:
            printf("...coalescing not needed.\n");
    }
    
}