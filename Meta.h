#include<stdio.h>

#define LISTS_TOTAL 10      // Amount of lists 
#define MMAP_BLOCK 111

typedef struct Node{        // Struct for metadata - everything but size can be overwritten when block in use
    int size;
    struct Node *next;
    struct Node *prev;
}Node;


void addBlock(Node* addr, size_t size);
void removeBlock(Node*addr);
void* findBlock(size_t size);
void addMem();
void* addBoundry(void* ptr,int size);
int getI(size_t size);
void addFullTag(void* ptr, size_t size);
void addFreeTag(void* ptr, size_t size);
int coCheck(void* addr, size_t size);
void displayFree();