#include "cachesim.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>
#include <iostream>
#include <vector>


//next:  debug the +1 prefetcher for MIP then also do it for LIP

/*
implementation steps:
-> L1 Cache: FA, SA, DM (done)
-> L2 Cache: FA, SA, DM (done)
-> Add Prefetcher (+1) (done)
-> LFU replacement (done - debug)
-> strided prefetch (done - debug)
-> LIP implement (done - debug)
*/


/**
 * Subroutine for initializing the cache simulator. You many add and initialize any global or heap
 * variables as needed.
 * TODO: You're responsible for completing this routine
 */




/*
*We need to allocate memory for MAR, Cache (Tag Register, Valid bit + Tag for each Blocks )
* Based on the (C,B,S) determine type of the cache: FA (S = C- B), DM (S = 0), SA (S = otherwise)
* Define the structure of cache:
    => Tag register (compare) number is: 2^S and each tag size is (MAR - (C - S)). Tag store can be an 1D array of length 2^S.
    => Number of block is N = 2^(C-B). Blocks can be thought as 2D array of shape (2^(N-S), 2^S) [Don't need to implement for this Project]
    => Tag store (one valid bit + tag for each block) has same shape of block. (2^(C-B-S), 2^S)
*/

uint64_t timeStampCounter = 0;
uint64_t tag_to_be_replaced;
uint64_t last_miss_addr =0;
bool first_miss_flag = false;
char rw ='A';
bool prefetch_flag = false; 
int dirty_flag = 0;
uint64_t cycle = 0;

typedef struct Cache {

    bool disabled, prefetcher_disabled, strided_prefetch_disabled;
    int numBlocks, tagSize, validBits, numSets, numWays,c,b,s;
    uint64_t indexMask, tagMask, waySelectMask, timeStampCounter;
    std::vector<std::vector<u_int64_t>> tagStore, cacheTimestamp, valid, dirty, MRU_bit; 
    replace_policy_t replace_policy;
    insert_policy_t prefetch_insert_policy;
    write_strat_t write_strat;

} cache_t;

cache_t l1, l2;


void sim_setup(sim_config_t *config) {
    
    //L1 - cache setup
    cache_config_t l1_conf = config->l1_config;

    l1.c = l1_conf.c;
    l1.b = l1_conf.b;
    l1.s = l1_conf.s;
    l1.replace_policy = l1_conf.replace_policy;
    l1.numBlocks = pow(2, (l1_conf.c - l1_conf.b));
    l1.tagSize = 64 - (l1_conf.c - l1_conf.s);
    l1.validBits = 1;


    l1.numSets = l1_conf.c-l1_conf.b-l1_conf.s;
    l1.numSets = pow(2, l1.numSets);
    l1.numWays = pow(2,l1_conf.s);

    l1.tagStore.resize(l1.numSets);
    for (auto& row:l1.tagStore) {
        row.resize(l1.numWays, 0); 
    }

    l1.cacheTimestamp = l1.tagStore;  //stores timestamp for all blocks
    l1.valid = l1.tagStore;  //stores valid bit for all blocks
    l1.dirty = l1.valid;
    l1.MRU_bit = l1.valid;



    //we need tag musk ( , c-s), index mask(for FA index musk length = 0, else c-s-1  to b),
    // way select mask (only for SA: S > 0 and S < c - b)
    l1.tagMask = ~ ((1 << (l1_conf.c - l1_conf.s ))-1);

    if (l1_conf.s == (l1_conf.c - l1_conf.b))
        l1.indexMask = 0;
    else
        l1.indexMask = ~((1 << (l1_conf.b  ))-1) &  ((1 << (l1_conf.c - l1_conf.s ))-1);


    if ((l1_conf.s > 0) && (l1_conf.s < (l1_conf.c - l1_conf.b)))
        l1.waySelectMask =  ((1 << (l1_conf.c  ))-1) & l1.tagMask;
    else 
        l1.waySelectMask = 0;


    //std::cout << l1.numSets << " " << l1.numWays << std::endl;
    //std::cout << "TagStoreShape " << l1.tagStore.size() << " " << l1.tagStore[0].size() << " " << l1.indexMask << " " << l1.waySelectMask << " "  << std::endl;
    //L2 cache - if not Disabled
    l2.disabled = config->l2_config.disabled;
    cache_config_t l2_conf = config->l2_config;
    if (config->l2_config.disabled){
        
        l2.c = 0;
        l2.b = 0;
        l2.s = 0;
    }
    else{

        l2.c = l2_conf.c;
        l2.b = l2_conf.b;
        l2.s = l2_conf.s;
    }

    l2.replace_policy = l2_conf.replace_policy;
    l2.numBlocks = pow(2, (l2.c - l2.b));
    l2.tagSize = 64 - (l2.c - l2.s);
    l2.validBits = 1;


    l2.numSets = l2.c-l2.b-l2.s;
    l2.numSets = pow(2, l2.numSets);
    l2.numWays = pow(2,l2.s);

    l2.tagStore.resize(l2.numSets);
    for (auto& row:l2.tagStore) {
        row.resize(l2.numWays, 0); 
    }

    l2.cacheTimestamp = l2.tagStore;  //stores timestamp for all blocks
    l2.valid = l2.tagStore;  //stores valid bit for all blocks


    l2.dirty = l2.valid;
    l2.MRU_bit = l2.valid;


    //we need tag musk ( , c-s), index mask(for FA index musk length = 0, else c-s-1  to b),
    // way select mask (only for SA: S > 0 and S < c - b)
    l2.tagMask = ~ ((1 << (l2.c - l2.s ))-1);

    if (l2.s == (l2.c - l2.b))
        l2.indexMask = 0;
    else
        l2.indexMask = ~((1 << (l2.b  ))-1) &  ((1 << (l2.c - l2.s ))-1);


    if ((l2.s > 0) && (l2.s < (l2.c - l2.b)))
        l2.waySelectMask =  ((1 << (l2.c  ))-1) & l2.tagMask;
    else 
        l2.waySelectMask = 0;


    l1.write_strat = WRITE_STRAT_WBWA;
    l2.write_strat = WRITE_STRAT_WTWNA;
    l2.prefetch_insert_policy = config->l2_config.prefetch_insert_policy;
    l2.prefetcher_disabled = config->l2_config.prefetcher_disabled;
    l2.strided_prefetch_disabled = config->l2_config.strided_prefetch_disabled;



    l1.timeStampCounter = pow(2, (l1.c-l1.b+1));
    l2.timeStampCounter = pow(2, (l2.c-l2.b+1));


}

/**
 * Subroutine that simulates the cache one trace event at a time.
 * TODO: You're responsible for completing this routine
 */


/*
*********************** Only L1: WBWA ***********
* For each access: MAR -> Tag + Way (SA ) + Index (SA/DM) + BlockOffset ( ....., [C-1, C-S]SA, [C-S-1, B]index, [B-1, 0]Block offset)

    -> Ignore the block offset (B number of LSB bits)
    -> extract index from MAR: No index for FA, for SA/DM, Load the corresponding tags inside the tag register/s
    -> extract way from MAR and select that block(s) tag using way and index values.
    -> if all valid bit is set and tag doesn't match with (any) tag registers then it is a miss (miss and replace according to replacement policy)
    -> Even a single valid bit is not set and none of the tag registers has a match then fill the empty block and set valid bit of that block 
    -> if valid bit is set and tag matches then it's a hit
    => update strats and tag timestamps and tag store value (if replaced)
*/


//search tag store for the tag and update block's timestamp if found
bool searchTagStore( cache_t* cache, uint64_t tag, uint64_t index){


    bool found = false;

    
    for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {
        
        if ((cache->tagStore[index][j] == tag) && (cache->valid[index][j]==1)) {

            if (!prefetch_flag){

                if (cache->replace_policy == REPLACE_POLICY_LRU){
                    cache->cacheTimestamp[index][j] = cache->timeStampCounter;
                    #ifdef DEBUG
                        if (cache->write_strat == WRITE_STRAT_WBWA){
                            printf("L1 hit\n");
                            printf("In L1, moving Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 " to MRU position", tag, index);
                            }

                        else {
                            if( (dirty_flag !=1))
                                printf("L2 read hit\n");
                            else if( (dirty_flag == 1) )
                                printf("L2 found block in cache on write\n"); 
                            printf("In L2, moving Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 " to MRU position", tag, index);

                        }
                       
                            
                    #endif
                    cache->timeStampCounter += 1;
                }
                else if (cache->replace_policy ==  REPLACE_POLICY_LFU){

                    cache->cacheTimestamp[index][j] += 1;
                    //reset all MRU bit
                    for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {
                        cache->MRU_bit[index][j] = 0;
                    }
                    cache->MRU_bit[index][j] = 1;
                    #ifdef DEBUG
                        if (cache->write_strat == WRITE_STRAT_WBWA){
                            printf("L1 hit\n");
                            printf("In L1, moving Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 " to MRU position", tag, index);
                            }

                        else {
                            if( (dirty_flag !=1))
                                printf("L2 read hit\n");
                            else if( (dirty_flag == 1) )
                                printf("L2 found block in cache on write\n"); 
                            printf("In L2, moving Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 " to MRU position", tag, index);

                        }
                       
                            
                    #endif



                    cache->timeStampCounter +=1;
                }
                //cache->cacheTimestamp[index][j] += 1;
                if ((cache->write_strat == WRITE_STRAT_WBWA) && (rw == 'W')) {

                    #ifdef DEBUG   
                        printf(" and setting dirty bit\n");
                
                    #endif
                    cache->dirty[index][j] = 1;

                }
                else{
                    #ifdef DEBUG   
                        printf("\n");
                
                    #endif

                }
            
             }
            found = true;

            return found;
        }
    }

    return found;


}


int replaceBlockLIP(cache_t* cache, uint64_t tag, uint64_t index){

    if (cache->replace_policy == REPLACE_POLICY_LRU){

        int lru_j =-1;
        uint64_t lru_timestamp = 90000000;

        //find the lru position for replacement 
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {

            if ((lru_timestamp > cache->cacheTimestamp[index][j]) && (cache->valid[index][j]== 1) ){

                lru_timestamp = cache->cacheTimestamp[index][j];
                lru_j = j;
            }
        }

        //if there is no valid block then place it in the first block with current timestamp
        if (lru_j == -1){

            cache->cacheTimestamp[index][0] = cache->timeStampCounter;
            cache->timeStampCounter += 1;
            cache->tagStore[index][0] = tag;
            cache->valid[index][0] = 1;

            return 0;

        }



        // if there is a valid block with LRU then first try to install the prefetched block in any invalid block and
        // if there is no invalid block to place then place at LRU position
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {

            if ((cache->valid[index][j] == 0)){

                cache->cacheTimestamp[index][j] = lru_timestamp -1;
                cache->timeStampCounter += 1;
                cache->tagStore[index][j] = tag;
                cache->valid[index][j] = 1;

                return 0;

            }
        }

        //if there is no invalid block to place then place at LRU position
        cache->cacheTimestamp[index][lru_j] = cache->cacheTimestamp[index][lru_j] -1;
        cache->tagStore[index][lru_j] = tag;
        cache->valid[index][lru_j] = 1;
        cache->timeStampCounter += 1;

        return 0;
    }
    else{


        int lfu_j = -1;
        uint64_t lfu_timestamp = 9000000;

        //find the lfu position for replacement 
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {

            if ((lfu_timestamp > cache->cacheTimestamp[index][j]) && (cache->valid[index][j] == 1) && (cache->MRU_bit[index][j]==0)){

                lfu_timestamp = cache->cacheTimestamp[index][j];
                lfu_j = j;
            }

            else if ((lfu_timestamp == cache->cacheTimestamp[index][j]) && (cache->MRU_bit[index][j] == 0)){

                if (cache->tagStore[index][j] < cache->tagStore[index][lfu_j]){

                    lfu_j = j;
                    lfu_timestamp = cache->cacheTimestamp[index][j];
                }

            }   


        }

        //if there is no valid block then place it in the first block with current timestamp
        if (lfu_j == -1){

            cache->cacheTimestamp[index][0] = 0;
            cache->MRU_bit[index][0] = 0;
            cache->tagStore[index][0] = tag;
            cache->valid[index][0] = 1;
            cache->timeStampCounter += 1;

            return 0;

        }
        




        // if there is a valid block with lfu then first try to install the prefetched block in any invalid block and
        // if there is no invalid block to place then place at lfu position
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {

            if ((cache->valid[index][j] == 0)){

                cache->cacheTimestamp[index][j] = 0; //since prefetched block
                cache->tagStore[index][j] = tag;
                cache->MRU_bit[index][lfu_j] = 0;
                cache->valid[index][j] = 1;
                cache->timeStampCounter += 1;

                return 0;

            }
        }

        cache->cacheTimestamp[index][lfu_j] = 0; //since prefetched block
        cache->MRU_bit[index][lfu_j] = 0;
        cache->tagStore[index][lfu_j] = tag;
        cache->valid[index][lfu_j] = 1;
        cache->timeStampCounter += 1;


    return 0;


        
    }

}


int replaceBlock(cache_t* cache, uint64_t tag, uint64_t index){

    if (cache->replace_policy == REPLACE_POLICY_LRU){

        int lru_j  ;
        uint64_t lru_timestamp = 1000000;
            
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {
            
            if ((cache->valid[index][j]==0) ) {

                #ifdef DEBUG
                if (cache->write_strat == WRITE_STRAT_WBWA){
                    printf("Evict from %s: ", "L1");
                    printf("block with valid=0 and index=0x%" PRIx64 "\n", index);
                }
                else if (!prefetch_flag){
                    printf("Evict from %s: ", "L2");
                    printf("block with valid=0 and index=0x%" PRIx64 "\n", index);
                    }
                #endif
                cache->cacheTimestamp[index][j]= cache->timeStampCounter;
                cache->timeStampCounter +=1;

                cache->tagStore[index][j] = tag;
                cache->valid[index][j]= 1;
                
                
                if ((cache->write_strat == WRITE_STRAT_WBWA) && (rw == 'W')) {
                    //std::cout << "hi 2 " <<std::endl;
                 
                    cache->dirty[index][j] = 1;
                 

                }

                return 0;
            }

            if ((lru_timestamp > cache->cacheTimestamp[index][j]) && (cache->valid[index][j])){
                lru_timestamp = cache->cacheTimestamp[index][j];
                lru_j = j;
            }

        }

        dirty_flag = 0;
      
            
        if ((cache->dirty[index][lru_j] == 1) && (cache->write_strat == WRITE_STRAT_WBWA)){   //a dirty block is being replaced so need to write back.
            
            
            tag_to_be_replaced = cache->tagStore[index][lru_j];
            cache->dirty[index][lru_j] = 0;
            dirty_flag = 1;

            
        }

        #ifdef DEBUG

        if (cache->write_strat == WRITE_STRAT_WBWA)
        {
            printf("Evict from %s: ", "L1");
            printf("block with valid=1, dirty=%d, tag 0x%" PRIx64 " and index=0x%" PRIx64 "\n", dirty_flag, cache->tagStore[index][lru_j], index);
        }
        else if (!prefetch_flag){
            printf("Evict from %s: ", "L2");
            printf("block with valid=1, tag 0x%" PRIx64 " and index=0x%" PRIx64 "\n", cache->tagStore[index][lru_j], index);
        }
        #endif
       

        cache->cacheTimestamp[index][lru_j]= cache->timeStampCounter;
        cache->timeStampCounter +=1;

        cache->tagStore[index][lru_j] = tag;
        cache->valid[index][lru_j]= 1;

        if ((cache->write_strat == WRITE_STRAT_WBWA) && (rw == 'W')) {


            cache->dirty[index][lru_j] = 1;

        }
        return dirty_flag;
    }

    
    else if (cache->replace_policy == REPLACE_POLICY_LFU)   //now using cacheTimestamp to store frequecy 
    {

        int lfu_j ;
        uint64_t lfu_timestamp = 1000000;
            
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {
            
            if ((cache->valid[index][j]==0) ) {


                #ifdef DEBUG
                if (cache->write_strat == WRITE_STRAT_WBWA){
                    printf("Evict from %s: ", "L1");
                    printf("block with valid=0 and index=0x%" PRIx64 "\n", index);
                }
                else if (!prefetch_flag){
                    printf("Evict from %s: ", "L2");
                    printf("block with valid=0 and index=0x%" PRIx64 "\n", index);
                    }
                #endif

                if (prefetch_flag)
                    cache->cacheTimestamp[index][j] =  0;
                else    
                    cache->cacheTimestamp[index][j] = 1;
    
        
                  
                for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {
                    cache->MRU_bit[index][j] = 0;
                }
                cache->MRU_bit[index][j] = 1;
                

                    

                cache->tagStore[index][j] = tag;
                cache->valid[index][j]= 1;
                
                if ((cache->write_strat == WRITE_STRAT_WBWA) && (rw == 'W')) {
                    //std::cout << "hi 2 " <<std::endl;
                    cache->dirty[index][j] = 1;
                 

                }
                cache->timeStampCounter += 1;
                return 0;
            }

            if ((lfu_timestamp > cache->cacheTimestamp[index][j]) && (cache->MRU_bit[index][j] == 0)){
                lfu_timestamp = cache->cacheTimestamp[index][j];
                lfu_j = j;
            }
            else if ((lfu_timestamp == cache->cacheTimestamp[index][j]) && (cache->MRU_bit[index][j] == 0)){

                if (cache->tagStore[index][j] < cache->tagStore[index][lfu_j]){

                    lfu_j = j;
                    lfu_timestamp = cache->cacheTimestamp[index][j];
                }

            }   
        
        }

        dirty_flag = 0;
      
            
        if ((cache->dirty[index][lfu_j] == 1) && (cache->write_strat == WRITE_STRAT_WBWA)){   //a dirty block is being replaced so need to write back.
            
            
            tag_to_be_replaced = cache->tagStore[index][lfu_j];
            cache->dirty[index][lfu_j] = 0;
            dirty_flag = 1;
            
        }


        #ifdef DEBUG

        if (cache->write_strat == WRITE_STRAT_WBWA)
        {
            printf("Evict from %s: ", "L1");
            printf("block with valid=1, dirty=%d, tag 0x%" PRIx64 " and index=0x%" PRIx64 "\n", dirty_flag, cache->tagStore[index][lfu_j], index);
        }
        else if (!prefetch_flag){
            printf("Evict from %s: ", "L2");
            printf("block with valid=1, tag 0x%" PRIx64 " and index=0x%" PRIx64 "\n", cache->tagStore[index][lfu_j], index);
        }
        #endif
       

        cache->tagStore[index][lfu_j] = tag;
        cache->valid[index][lfu_j]= 1;
        cache->timeStampCounter += 1;

        
        if (!(prefetch_flag )) 
            cache->cacheTimestamp[index][lfu_j]= 1;
        else
            cache->cacheTimestamp[index][lfu_j]= 0;

        //reset all MRU bit
        for (size_t j = 0; j < cache->tagStore[index].size(); ++j) {
            cache->MRU_bit[index][j] = 0;
        }
        // set new MRU bit
        cache->MRU_bit[index][lfu_j] = 1;

        
            

        if ((cache->write_strat == WRITE_STRAT_WBWA) && (rw == 'W')) {

            cache->dirty[index][lfu_j] = 1;

        }
        return dirty_flag;



    }
    
    
}



void sim_access(char rw_m, uint64_t addr, sim_stats_t* stats) {


    #ifdef DEBUG
    printf("Time: %" PRIu64 ". Address: 0x%" PRIx64 ". Read/Write: %c\n", cycle, addr, rw_m);
    #endif
    rw = rw_m;
    //Count Read and write access
    if (rw == 'R')
        stats->reads +=1;
    else
        stats->writes +=1;
    
    stats->accesses_l1 += 1;

    //L1 cache 
    uint64_t tag = (addr & l1.tagMask) >> (l1.c-l1.s);
    uint64_t index = (addr & l1.indexMask) >> l1.b;
    uint64_t way = (addr & l1.waySelectMask) >> (l1.c-l1.s);

    #ifdef DEBUG
    printf("L1 decomposed address 0x%" PRIx64 " -> Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 "\n", addr, tag, index);
    #endif

    //L2 cache
    uint64_t tag_l2 = (addr & l2.tagMask) >> (l2.c-l2.s);
    uint64_t index_l2 = (addr & l2.indexMask) >> l2.b;
    uint64_t way_l2 = (addr & l2.waySelectMask) >> (l2.c-l2.s);

    //std::cout << way << " " << index << std::endl;
    // FA cache: Search the tag if missed, then check if there is any valid bit == 0, place the block there. otherwise, repplace
    
    if (searchTagStore(&l1, tag, index)){
        stats->hits_l1 += 1;

    }
    else {

        stats->misses_l1 +=1;
        #ifdef DEBUG
        
        printf("L1 miss\n");
        printf("L2 decomposed address 0x%" PRIx64 " -> Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 "\n", addr, tag_l2, index_l2);
        #endif

        //since miss look up for the L2 cache 
        
        
        stats->accesses_l2 += 1;

        stats->reads_l2+= 1;

        if (searchTagStore(&l2, tag_l2, index_l2)){ 
            
            stats->read_hits_l2 += 1;
            

            }
            
        else {

            stats->read_misses_l2 +=1;
            #ifdef DEBUG
            if (l2.disabled)
                printf("L2 is disabled, treating this as an L2 read miss\n");
            else
                printf("L2 read miss\n");
            #endif
            replaceBlock(&l2, tag_l2 , index_l2);
            
                //add prefetchers code 
            if (!l2.prefetcher_disabled){
                prefetch_flag = true;
                if (l2.strided_prefetch_disabled){
                    //+1 prefetch
                    
                
                    uint64_t next_block_address = ((addr >> l2.b) + 1) << l2.b;
                    uint64_t prefetch_tag = (next_block_address & l2.tagMask) >> (l2.c - l2.s);
                    uint64_t prefetch_index =  (next_block_address & l2.indexMask) >> l2.b;



                    if (!searchTagStore(&l2, prefetch_tag, prefetch_index)){

                        if (l2.prefetch_insert_policy == INSERT_POLICY_MIP)    
                            replaceBlock(&l2, prefetch_tag , prefetch_index);  //MIP
                        else if (l2.prefetch_insert_policy == INSERT_POLICY_LIP){
                            //std::cout << "ok 3" << std::endl;
                            replaceBlockLIP(&l2 , prefetch_tag, prefetch_index);
                            }
                        stats->prefetches_l2 +=1;
                        #ifdef DEBUG
                        printf("Prefetch block with address 0x%" PRIx64 " from memrory to L2\n", next_block_address );
                        #endif
                            
   
                    }
                    

                }
                else{  //strided prefetches

                    //first miss already recorded now stride can be applied


                    uint64_t next_block_address;
                    addr = addr >> l2.b;
                
                    if (last_miss_addr > addr){  //negative stride

                        next_block_address = addr - (last_miss_addr - addr) ;
                    }
                    else{     //positve stride
                        next_block_address = addr + (addr - last_miss_addr) ;
                    }
                    
                    uint64_t prefetch_tag = ((next_block_address << l2.b) & l2.tagMask) >> (l2.c-l2.s);
                    uint64_t prefetch_index =  ((next_block_address << l2.b) & l2.indexMask) >> l2.b;

                    
                
                    if (!searchTagStore(&l2, prefetch_tag, prefetch_index)){

                        if (l2.prefetch_insert_policy == INSERT_POLICY_MIP)    
                            replaceBlock(&l2, prefetch_tag , prefetch_index);  //MIP
                        else if (l2.prefetch_insert_policy == INSERT_POLICY_LIP){
                            replaceBlockLIP(&l2 , prefetch_tag, prefetch_index);
                            

                        }
            
                        stats->prefetches_l2 +=1;
                        #ifdef DEBUG
                        printf("Prefetch block with address 0x%" PRIx64 " from memrory to L2\n", (next_block_address << l2.b) );
                        #endif
                            
                    

                    }

                    last_miss_addr = addr;
            

                    
                }
                prefetch_flag= false;
            }
            
            
        }


        int l2_write_flag = replaceBlock(&l1, tag , index); //handle write back of L1 inside this function along with replace



        if ((l2_write_flag == 1) ){  //if block is replaced write back the block if exist in the L2

        //std::cout << "hi" << std::endl;
        //we need the tag of the replaced block
            uint64_t restored_trace = (tag_to_be_replaced << (l1.c-l1.s)) + (index << l1.b);
            uint64_t restored_tag_l2 = (restored_trace &  l2.tagMask) >> (l2.c-l2.s);
            uint64_t restored_index_l2 = (restored_trace & l2.indexMask) >> l2.b;  
            #ifdef DEBUG

                if (dirty_flag){
                    printf("Writing back dirty block with address 0x%" PRIx64 " to L2\n", restored_trace) ;
                    printf("L2 decomposed address 0x%" PRIx64 " -> Tag: 0x%" PRIx64 " and Index: 0x%" PRIx64 "\n", restored_trace,restored_tag_l2, restored_index_l2);
                    
                }
                    
            #endif 
            //std::cout << "hi olk" << std::endl;
            //std::cout << tag_to_be_replaced << " " << restored_index_l2 << " " << restored_trace  << " " << index << std::endl;        
            stats->writes_l2 += 1;
            if (l2.disabled){
                
                #ifdef DEBUG
                printf("L2 is disabled, writing through to memory\n");
                #endif

            }

            else if (searchTagStore(&l2, restored_tag_l2, restored_index_l2)){
                
                    //stats->writes_l2 += 1;
            }
            else{
                #ifdef DEBUG
                printf("L2 did not find block in cache on write, writing through to memory anyway\n");
                #endif
            }

            dirty_flag = 0;
            

        }
        
        
    }
    



    timeStampCounter += 1;
    cycle += 1;

    #ifdef DEBUG
        printf("\n");
    #endif
}



/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * TODO: You're responsible for completing this routine
 */
void sim_finish(sim_stats_t *stats) {

    stats->hit_ratio_l1 =(double) (stats->hits_l1)/stats->accesses_l1;
    stats->miss_ratio_l1 = (double)(stats->misses_l1)/stats->accesses_l1;
    stats->read_hit_ratio_l2 =(double) (stats->read_hits_l2)/stats->reads_l2;
    stats->read_miss_ratio_l2 = (double)(stats->read_misses_l2)/stats->reads_l2;
 
    double l1_hit_time = L1_HIT_K0+L1_HIT_K1*(l1.c-l1.b-l1.s)+L1_HIT_K2*(std::max(3,l1.s)-3);
    //double l2_hit_time = L2_HIT_K0+L1_HIT_K1*(l1.c-l1.b-l1.s)+L1_HIT_K2*(std::max(3,l1.s)-3);


    
    double l1_MP =  DRAM_ACCESS_TIME;

    if (!l2.disabled){
        double l2_hit_time = L2_HIT_K3+L2_HIT_K4*(l2.c-l2.b-l2.s)+L2_HIT_K5*(std::max(3,l2.s)-3);
        //double l2_hit_time = L2_HIT_K0+l2_HIT_K1*(l2.c-l2.b-l2.s)+l2_HIT_K2*(std::max(3,l2.s)-3);
        double l2_MP = DRAM_ACCESS_TIME;
        stats->avg_access_time_l2 = l2_hit_time  + stats->read_miss_ratio_l2 *  DRAM_ACCESS_TIME;
        l1_MP = l2_hit_time  + stats->read_miss_ratio_l2 *  DRAM_ACCESS_TIME;
    }
    else{

        double l2_hit_time = 0;
        //double l2_hit_time = L2_HIT_K0+l2_HIT_K1*(l2.c-l2.b-l2.s)+l2_HIT_K2*(std::max(3,l2.s)-3);
        double l2_MP = DRAM_ACCESS_TIME;
        stats->avg_access_time_l2 = l2_hit_time  + stats->read_miss_ratio_l2 *  DRAM_ACCESS_TIME;
        l1_MP = l2_hit_time  + stats->read_miss_ratio_l2 *  DRAM_ACCESS_TIME;
    }
    stats->avg_access_time_l1 = l1_hit_time + stats->miss_ratio_l1*l1_MP;


}