/***************************************************************************
 * *    Inf2C-CS Coursework 2: Cache Simulation
 * *
 * *    Boris Grot, Priyank Faldu
 * *
 * *    Deadline: Wed 23 Nov (Week 10) 16:00
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
/* Do not add any more header files */

typedef enum {dm, fa} cache_map_t;
typedef enum {fifo, lru, none} cache_replacement_t;
typedef enum {instruction, data} access_t;

typedef struct {
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct {
    uint32_t instruction_accesses;
    uint32_t instruction_hits;
    uint32_t data_accesses;
    uint32_t data_hits;
} result_t;

/* Cache Parameters */
uint32_t cache_size = 2048;
uint32_t block_size = 64;
cache_map_t cache_mapping = dm;
cache_replacement_t cache_replacement = none;


/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access)
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1002];
    char* token = NULL;
    char* string = buf;
    mem_access_t access;
    
    if (fgets(buf, 1000, ptr_file)!=NULL) {
        
        /* Get the access type */
        token = strsep(&string, " \n");
        if (strcmp(token,"I") == 0) {
            access.accesstype = instruction;
        } else if (strcmp(token,"D") == 0) {
            access.accesstype = data;
        } else {
            printf("Unkown access type\n");
            exit(-1);
        }
        
        /* Get the address */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);
        
        return access;
    }
    
    /* If there are no more entries in the file return an address 0 */
    access.address = 0;
    return access;
}

void print_statistics(uint32_t num_blocks, uint32_t bits_offset, uint32_t bits_index, uint32_t bits_tag, result_t r) {
    /* Do Not Modify This Function */
    printf("Num_Blocks:%u\n", num_blocks);
    printf("Bits_BlockOffset:%u\n", bits_offset);
    printf("Bits_Index:%u\n", bits_index);
    printf("Bits_Tag:%u\n", bits_tag);
    if ( (r.instruction_accesses == 0) || (r.data_accesses == 0) ) {
        /*
         * Just a protection over divide by zero.
         * Ideally, it should never reach here.
         */
        return;
    }
    printf("Total_Accesses:%u\n", r.instruction_accesses + r.data_accesses);
    printf("Total_Hits:%u\n", r.instruction_hits + r.data_hits);
    printf("Total_HitRate:%2.2f%%\n", (r.instruction_hits + r.data_hits) / ((float)(r.instruction_accesses + r.data_accesses)) * 100.0);
    printf("Instruction_Accesses:%u\n", r.instruction_accesses);
    printf("Instruction_Hits:%u\n", r.instruction_hits);
    printf("Instruction_HitRate:%2.2f%%\n", r.instruction_hits / ((float)(r.instruction_accesses)) * 100.0);
    printf("Data_Accesses:%u\n", r.data_accesses);
    printf("Data_Hits:%u\n", r.data_hits);
    printf("Data_HitRate:%2.2f%%\n", r.data_hits / ((float)(r.data_accesses)) * 100.0);
}

/*
 * Global variables
 * These variables must be populated before call to print_statistics(...) is made.
 */
uint32_t num_bits_for_block_offset = 0;
uint32_t num_bits_for_index = 0;
uint32_t num_bits_for_tag = 0;
uint32_t num_blocks = 0;
result_t result;

void rotate (int arr[], int start, int end)                 //This function is used to rotate our Fully associated LRU cache
{                                                           //It helps us order the array in least recently used to most recently used
    int i;
    for(i=start; i<end-1; i++)
    {
        int temp = arr[i];
        arr[i] = arr[i+1];
        arr[i+1] = temp;
    }
}

int main(int argc, char** argv)
{
    
    /*
     * Read command-line parameters and initialize:
     * cache_size, block_size, cache_mapping and cache_replacement variables
     */
    
    if ( argc != 4 ) { /* argc should be 4 for correct execution */
        printf("Usage: ./cache_sim [cache size: 64-8192] [cache block size: 32/64/128] [cache mapping: DM/FIFO/LRU]\n");
        exit(-1);
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */
        
        /* Set block and cache size in bytes */
        cache_size = atoi(argv[1]);
        block_size = atoi(argv[2]);
        assert(cache_size >= 256 && cache_size <= 8192);
        /* cache_size must be power of 2 */
        assert(!(cache_size & (cache_size-1)));
        assert(block_size >= 16 && block_size <= 256);
        /* block_size must be power of 2 */
        assert(!(block_size & (block_size-1)));
        assert(block_size <= cache_size);
        
        /* Set Cache Mapping */
        if (strcmp(argv[3], "DM") == 0) {
            cache_mapping = dm;
            cache_replacement = none;
        } else if (strcmp(argv[3], "FIFO") == 0) {
            cache_mapping = fa;
            cache_replacement = fifo;
        } else if (strcmp(argv[3], "LRU") == 0 ) {
            cache_mapping = fa;
            cache_replacement = lru;
        } else {
            printf("Unknown cache mapping: %s\n", argv[3]);
            exit(-1);
        }
    }
    
    
    num_blocks = cache_size/block_size;
    num_bits_for_block_offset = (uint32_t)(log(block_size)/log(2));
    
    if (cache_mapping == dm)
        num_bits_for_index = (uint32_t)(log(num_blocks)/log(2));
    else
        num_bits_for_index = 0;
    if (cache_mapping == dm)
        num_bits_for_tag = 32 - num_bits_for_block_offset - num_bits_for_index;
    else
        num_bits_for_tag = 32 - num_bits_for_block_offset;
        
    int hit_array[num_blocks];                   //For the cache values
    int pos = 0;                                 //For storing the current position in the fifo and LRU blocks
    int i;
    for(i=0; i<num_blocks; i++)
    {
        hit_array[i] = 0;
    }
    
    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file =fopen("mem_trace.txt","r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(-1);
    }
    
    /* Reset the result structure */
    memset(&result, 0, sizeof(result));
    
    /* Do not delete any of the lines below.
     * Use the following snippet and add your code to finish the task */
    
    /* Loop until whole trace file has been read */
    mem_access_t access;
    while(1) {
        access = read_transaction(ptr_file);
        //If no transactions left, break out of loop
        if (access.address == 0)
            break;
        if (cache_mapping == dm)
        {
            if (access.accesstype == instruction)
                result.instruction_accesses++;
            if (access.accesstype == data)
                result.data_accesses++;
            
            uint32_t tag = access.address >> (num_bits_for_index + num_bits_for_block_offset);
            uint32_t index = (access.address >> num_bits_for_block_offset) % (tag << num_bits_for_index);
            
            //int decimal_index = 0, base = 1, rem;            //Loop to convert the binary index into decimal
            //while (index > 0)
            //{
                //rem = index % 10;
                //decimal_index = decimal_index + rem * base;
                //index = index / 10 ;
                //base = base * 2;
            //}
            
            if (hit_array[index] == tag)
            {
                if (access.accesstype == instruction)
                result.instruction_hits++;
                if (access.accesstype == data)
                result.data_hits++;
            }
            else
                hit_array[index] = tag;
        }
        
        if (cache_mapping == fa && cache_replacement == fifo)
        {
            if (access.accesstype == instruction)
                result.instruction_accesses++;
            if (access.accesstype == data)
                result.data_accesses++;
            
            pos = pos % num_blocks;
            uint32_t tag = access.address >> num_bits_for_block_offset;
            
            int x = 0;
            int i;
            for(i=0; i<num_blocks; i++)
            {
                if (x==0)
                {
                    if(hit_array[i] == tag)
                    {
                        x++;
                        if (access.accesstype == instruction)
                            result.instruction_hits++;
                        if (access.accesstype == data)
                            result.data_hits++;
                    }
                }
            }
                if (x==0)
                {
                    hit_array[pos] = tag;
                    pos++;
                }
            }
            
        
        
        if (cache_mapping == fa  && cache_replacement == lru)
        {
            if (access.accesstype == instruction)
                result.instruction_accesses++;
            if (access.accesstype == data)
                result.data_accesses++;
            
            //pos = pos % num_blocks;                     //Make sure it doesn't exceed block value accidentally
            uint32_t tag = access.address >> num_bits_for_block_offset;
            
            int x = 0;
            int i;
            for(i=0; i<num_blocks; i++)
            {
                if (x==0)
                {
                    if(hit_array[i] == tag)
                    {
                        x++;
                        rotate(hit_array, i, pos);
                        int j;
                        //for(j=0; j<pos; j++)
                        //{
                            //printf("%d\n", hit_array[j]);
                        //}
                        if (access.accesstype == instruction)
                            result.instruction_hits++;
                        if (access.accesstype == data)
                            result.data_hits++;
                    }
                }
            }
            if (x==0 && pos == num_blocks)         //Cache full!
            {
                hit_array[0] = tag;                //Least Recently Used always stored at first position
                rotate(hit_array, 0, pos);         //shifting recently accessed variable to last position
                int j;
                //for(j=0; j<pos; j++)
                //{
                    //printf("%d\n", hit_array[j]);
                //}
                
            }
            if (x==0 && pos<num_blocks)           //If the value in the memory is empty
            {
                hit_array[pos] = tag;
                pos++;
                int j;
            }
        }

    }
    
    /* Do not modify code below */
    /* Make sure that all the parameters are appropriately populated */
    print_statistics(num_blocks, num_bits_for_block_offset, num_bits_for_index, num_bits_for_tag, result);
    
    /* Close the trace file */
    fclose(ptr_file);
    
    return 0;
}