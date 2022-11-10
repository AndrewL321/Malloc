#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "Meta.h"

// Array pointers to heads of free lists
Node* heads[LISTS_TOTAL] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

void * initSbrk = NULL;              //  holds the initial heap address
void * curSbrk = NULL;              // latest heap address


// Param is size of memory wanted
// returns pointer to start of free memory of at least the requested size
void* new_malloc(size_t size){


    if(size > 8176){                // mmap for big memory sizes
        int * map = mmap(NULL,size + 8, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANON|MAP_PRIVATE, 0, 0);
        *map = size + 8;               // Set size inside this memory
        map = map +1;
        *map = MMAP_BLOCK;                  // Set tag identifying mmap block
        return map +1;
    }
    if(size == 0){         // Don't allocate for 0  
        return NULL;
    }
    size = ((size+9) >> 1) << 1;        // Add 8 for boundary ints, round up to whole number
    if(size< 28){                       // 28 = size of metadata
        size = 28;
    }

    return findBlock(size) + 4;         // offset of 4, start of free memory
}


// Param address to be freed
// checks addres is valid, should have a boundary int which is odd
// call coalesce check, if can't coalesce add new block to free
void new_free(void* ptr){

    int* check = (int*) ptr;                // Used to check value in current address, for checking if free block

    int* sizeTemp = (int*) ptr  - 1;       // variable pointing to location of start of blocka

    if(*sizeTemp == MMAP_BLOCK){            // if block identified as mmap block
        sizeTemp = sizeTemp - 1;
        munmap(sizeTemp, *sizeTemp);
        return;
    }

    if( ptr > curSbrk){                     // Address to high
        printf("----- Address out of bounds ----- \n");
        return;
    }
    if(*check != 0 && (*sizeTemp & 1) == 1){        // Address is a free block
        printf("-- This is a free block -- Nothing Freed -----\n");
        return;
    }
    
    if((*sizeTemp & 1) == 0){                       // Address is not taken
        printf("--Not a valid address to free--  Nothing Freed ----- \n\n");
        return;
    }

    if(coCheck(ptr -4, *sizeTemp  - 1) == 1){          // Check if block can coalesce
        return;
    }

    addBlock(ptr - 4 ,*sizeTemp  - 1);                  // Pass all, add new block


}


//Param = size of block required
// returns pointer to block that can the size
void* findBlock(size_t size){
    int i = getI(size);                 // Index based off size
    Node* fitBlock = NULL;                 // pointer to block that is big enough
    Node* temp = heads[i];                  // Pointer to head of list of correct size

    while(i < LISTS_TOTAL){                 // Find a block that can fit

        if(temp != NULL && temp->size >= size){
            fitBlock = temp;
            break;
        }
        if(temp != NULL){
            temp = temp->next;
        }
        
        while(temp == NULL && i < LISTS_TOTAL){
            i++;
            temp = heads[i];
        }
        
    }

    Node* best = NULL;              // pointer to the lowest sized block that fits space

    while(fitBlock != NULL){     //  Check current block found is best fit
        if(best == NULL){
            best = fitBlock;
        }
        if(best != NULL && best->size > fitBlock->size && fitBlock->size >= size){
            best = fitBlock; 
        }
        fitBlock = fitBlock->next;
    }


    if(best != NULL){           // If block is found
        
        removeBlock(best);
        int remain = best->size - size;         // space left at end of block
        void* temp = (void*) best;

        if(remain >= 28){                       // Don't create block if too small for metadata
            addBlock(temp + size,best->size - size );  
        }
        else{ 
            size = size +remain;
        }

        addFullTag(best,size);
        return best;

    }

    if(best == NULL){                       // No block found
        addMem();
        return findBlock(size);
    }

}


// Claims 8192 bytes of memory from heap
void addMem(){

    void* temp = sbrk(8192);                // Claim new memory
    addFullTag(temp,8192);                  // add boundary tags

    if(temp == curSbrk){                    // if nothing called sbrk since last can overwrite boundary tag
        addBlock(temp - 4 ,8192);
        coCheck(temp-4, 8192);
    }
    else{
        addBlock(temp + 4, 8184);
    }
    curSbrk = sbrk(0);                        // Current top heap address
    
}

// param ptr, address where free block created - size, size of block
// creates free block and adds to correct free list
void addBlock(Node* ptr, size_t size){
    int i = (getI(size));

    addFreeTag(ptr,size);
    Node* new = ptr;
    new->size = size;
    
    new->prev = NULL;
    if(heads[i] == NULL){       // if list empty
        new->next = NULL;
        heads[i] = new; 
    }
    else if (heads[i] !=NULL){          // list not empty
        new->next = heads[i];
        heads[i]->prev = new;
        heads[i] = new;
    }

}

//checks if new free memory can coalesce with next door memory
//param = ptr, address to check - size, size of block that is getting checked
//return = 1 for coalesced , 0 for not
int coCheck(void* ptr, size_t size){

    int tempSize = size;                    // Variable holding size
    int * temp = ptr + size ;               // pointer to end of block
    int success = 0;                        // success variable, set to 1 if co takes place
    int newSize;                            // newSize = current size + size of block coalescing

    if((*temp & 1) == 0){               // if boundary tag indicates memory to right is free
        newSize = tempSize + *temp;
        removeBlock((Node*)temp);
        addBlock(ptr , newSize);
        success = 1;
        tempSize = newSize;    
    }

    temp = ptr  -4;
    if((*temp&1) == 0 && temp){            // left is free
        newSize = tempSize + *temp;
        int tempJump = *temp;
        temp = ptr - tempJump;
        removeBlock(ptr);
        removeBlock((Node*)temp);     
        addBlock((Node*)temp, newSize );
        success = 1;
    
    }
    return success;
}

// removes block from its free lists
// ptr = block to be removed
void removeBlock(Node* ptr){

    int i = getI(ptr->size);            // index based of size of block

    // only node on list
    if(ptr->next == NULL && ptr->prev == NULL){

        heads[i] = NULL;
        return;
    }
    // node is head
    else if(ptr->next != NULL && ptr->prev == NULL)
    {
        heads[i] = ptr->next;
        heads[i]-> prev = NULL;
        return;
    }
    // node in middle
    else if(ptr->next != NULL && ptr->prev != NULL){

        ptr->next->prev = ptr->prev;
        ptr->prev->next = ptr->next;
        return;
    }
    // node is end
    else if (ptr->next == NULL && ptr->prev != NULL)
    {

        ptr->prev->next = NULL;
        return;
    }
    printf("Error in remove, should not reach this message"); 
}

// add boundary tags of size + 1 for full
// param addr - address to add tags to, size - size of block
void addFullTag(void* addr, size_t size){
    int * temp = addr;          // int pointer to start
    *temp = size+1;             // set value to odd to indicate full
    addr = addr + size -4;
    temp = addr;
    *temp = size+1;

}
// add free tag to end of block
// param = addr, address of block - size, size of block
void addFreeTag(void* addr, size_t size){
    int * temp = addr + size - 4;       // point to end of block - 1 int size
    *temp = size;
}

// get lists index based off size
// param size of block
int getI(size_t size){

    if(size <= 32){
        return 0;
    }
    if(size <= 64){
        return 1;
    }
    if(size <= 128){
        return 2;
    }
    if(size <= 256){
        return 3;
    }
    if(size <= 512){
        return 4;
    }
    if(size <= 1024){
        return 5;
    }
    if(size <= 2048){
        return 6;
    }
    if(size <= 4096){
        return 7;
    }
    if(size <= 8192){
        return 8;
    }
    if(size > 8192){
        return 9;
    }
}

// displays free memory
void displayFree(){
    int countTotal = 0;             // Total count of memory
    for(int i = 0; i< LISTS_TOTAL ; i++){
        Node* temp = heads[i];          // Point to head of free list
        int thisList = 0;               // Total mem on current list
        if(temp!= NULL){
            printf("--- List %i contains --- \n" , i + 1);
        }
        while(temp != NULL){
            
            printf("||  Memory size : %i  Address %p ||", temp->size - 8, temp);
            countTotal += temp->size -8;
            thisList += temp->size -8; 
            temp = temp->next;
             
        } 
        if(thisList > 0){
            printf("\nTotal memory on list %i = %i\n\n", i + 1, thisList);    
        }
    }
 
    printf("Total free on all lists = %i\n\n", countTotal);
}

// main program to test
int main(){

    char input[20];              // Takes letter input
    unsigned int size;          // holds size input
    void * ptr;                     // holds address input
    char* finalInput;               // Holds input minus first char

    printf("\nA<Bytes>, to get memory of size Bytes\n");
    printf("F<addr>, to free memory at addr\n\n");
    
    initSbrk = sbrk(0);         
    curSbrk = initSbrk;


 
    while(1){

        printf("Enter A<Bytes> or F<Addr> ....\n");
        scanf("%s", input);

        finalInput = strtok(input, "AF");
        
        if(input[0] == 'A' && finalInput != NULL){          // Add
            int sizeInput = atoi(finalInput);
            printf("The address returned = %p\n",new_malloc(sizeInput)); 
            displayFree();  
        }
        else if(input[0] == 'F' && finalInput != NULL){         // Free
            long addInput = strtol(finalInput,NULL, 16);
            ptr = NULL;
            ptr += addInput;

            if(ptr <= initSbrk){
                printf("Invalid address\n");
            }
            else{
                new_free(ptr);
                displayFree();
            }
        }  
        else{                                                   // Invalid input
            printf("Invalid, try again, enter A<Bytes> or F<Addr>\n");
        }
        
    }
    
}