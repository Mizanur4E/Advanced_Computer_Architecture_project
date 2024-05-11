#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <list>
#include <iostream>
#include <map>

#include "procsim.hpp"

#define NUM_REGS 32
#define NUM_MATS 64

//
// TODO: Define any useful data structures and functions here
//

//FU types
typedef struct{

    uint64_t tag;
    bool ready;

} ALU_t;

typedef struct{

    uint64_t tag;
    inst_t inst;
    int stalledCycles, maxStall;
    bool ready;
    bool stallStarted;

} LSU_t;


typedef struct 
{
    uint64_t tag1 , tag2, tag3;
    bool ready1, ready2, ready3;
    bool ready;
} MUL_t;



//for registers and Memory registers (MAT)
typedef struct{

    uint64_t tag;
    bool ready;

} register_tt;

typedef struct{

    inst_t inst;
    bool nop;
    bool ready;

} dispQ_t;

typedef struct {

    inst_t inst;
    bool src1ready, src2ready, msready;
    uint64_t destTag = 0;
    uint64_t src1tag = 0;
    uint64_t src2tag = 0;
    uint64_t ms_tag= 0;
    uint64_t md_tag = 0;
    opcode_t FU;
    bool ready, inFU, ms_ready;

} schedulingQueque_t;

typedef struct {

    uint64_t tag, regsN;
    bool ready;

} CDB_t;

typedef struct {

    inst_t inst;
    uint64_t tag = 0;
    bool completed;
    bool ready;
} rob_t;

int robSize;
const procsim_conf_t *procsim_conf;
std::vector<dispQ_t>* dispatchQ;
std::vector<schedulingQueque_t>* schedlingQ;
std::vector<CDB_t>* resultBuses;
std::vector<register_tt>* Aregs;
std::vector<register_tt>* MAT;
uint64_t globalTagCounter;
std::vector<register_tt>* futureRegsFile;
std::vector<rob_t>* ROB;
std::vector<ALU_t>* alu;
std::vector<MUL_t>* mul;
std::vector<LSU_t>* lsu;




// The helper functions in this#ifdef are optional and included here for your
// convenience so you can spend more time writing your simulator logic and less
// time trying to match debug trace formatting! (If you choose to use them)

int usageMeterRob(){

    int usage =0;
    for (int i= 0; i < ROB->size(); i++){
        if (!(*ROB)[i].ready){
            usage += 1;
        }
    }
    return usage;

}

int usageMeterSchdQ(){

    int usage =0;
    for (int i= 0; i < schedlingQ->size(); i++){
        if (!(*schedlingQ)[i].ready){
            usage += 1;
        }
    }
    return usage;

}


int usageMeterDispQ(){

    int usage =0;
    for (int i= 0; i < dispatchQ->size(); i++){
        if (!(*dispatchQ)[i].ready){
            usage += 1;
        }
    }
    return usage;

}



#ifdef DEBUG
static void print_operand(int8_t rx) {
    if (rx < 0) {
        printf("(none)"); //  PROVIDED
    } else {
        printf("R%" PRId8, rx); //  PROVIDED
    }
}

// Useful in the fetch and dispatch stages
static void print_instruction(const inst_t *inst) {
    if (!inst ) {
        return;
    }
    static const char *opcode_names[] = {NULL, NULL, "ADD", "MUL", "LOAD", "STORE", "BRANCH"};

    printf("opcode=%s, dest=", opcode_names[inst->opcode]); // PROVIDED
    print_operand(inst->dest); // PROVIDED
    printf(", src1="); // PROVIDED
    print_operand(inst->src1); // PROVIDED
    printf(", src2="); // PROVIDED
    print_operand(inst->src2); // PROVIDED
    printf(", dyncount=%lu", inst->dyn_instruction_count); //  PROVIDED
}


// This will print the state of the ROB where instructions are identified by their dyn_instruction_count
static void print_rob(void) {
    size_t printed_idx = 0;

            

    printf("\tAllocated Entries in ROB: %lu\n", usageMeterRob()); // TODO: Fix Me
    for ( int i = 0; i < ROB->size(); i++) { // TODO: Fix Me
    
        if (!(*ROB)[i].ready){

            if (printed_idx == 0) {
                printf("    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", (*ROB)[i].inst.dyn_instruction_count, (*ROB)[i].completed, (*ROB)[i].inst.mispredict); // TODO: Fix Me
            } else if (!(printed_idx & 0x3)) {
                printf("\n    { dyncount=%05" PRIu64 ", completed: %d, mispredict: %d }", (*ROB)[i].inst.dyn_instruction_count, (*ROB)[i].completed, (*ROB)[i].inst.mispredict); // TODO: Fix Me
            } else {
                printf(", { dyncount=%05" PRIu64 " completed: %d, mispredict: %d }", (*ROB)[i].inst.dyn_instruction_count, (*ROB)[i].completed, (*ROB)[i].inst.mispredict); // TODO: Fix Me
            }
            printed_idx++;

        }
    }
    if (!printed_idx) {
        printf("    (ROB empty)"); //  PROVIDED
    }
    printf("\n"); //  PROVIDED
}

static void print_messy_rf(void) {
    for (size_t i = 0; i < NUM_REGS; i++) {
        if (!(i & 0x3)) {
            printf("\t");
        } else {
            printf(", ");
        }
            printf("R%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", i, (*futureRegsFile)[i].tag, (*futureRegsFile)[i].ready); // TODO: Fix Me
        if ((!((i+1) & 0x3)) && ((i+1) != NUM_REGS)) {
            printf("\n"); // PROVIDED
        }
    }
    printf("\n"); // PROVIDED
}

static void print_mat(void) {
    for (size_t i = 0; i < NUM_MATS; i++) {
        if (!(i & 0x3)) {
            printf("\t");
        } else {
            printf(", ");
        }
            printf("M%" PRIu64 "={tag: %" PRIu64 ", ready: %d}", i, (*MAT)[i].tag, (*MAT)[i].ready); // TODO: Fix Me
        if ((!((i+1) & 0x3)) && ((i+1) != NUM_MATS)) {
            printf("\n");
        }
    }
    printf("\n"); // PROVIDED
}
#endif


// Optional helper function which retires instructions in ROB in program
// order. (In a real system, the destination register value from the ROB would be written to the
// architectural registers, but we have no register values in this
// simulation.) This function returns the number of instructions retired.
// Immediately after retiring a mispredicting branch, this function will set
// *retired_mispredict_out = true and will not retire any more instructions. 
// Note that in this case, the mispredict must be counted as one of the retired instructions.
std::map<int, std::string> opcode_dict = {
    {2, "ADD"},
    {3, "MUL"},
    {4, "LOAD"},
    {5, "STORE"},
    {6, "BRANCH"}
};
static uint64_t stage_state_update(procsim_stats_t *stats,
                                   bool *retired_mispredict_out) {
    // TODO: fill me in




    #ifdef DEBUG
        printf("Stage State Update: \n"); //  PROVIDED
        printf("Checking ROB: \n");
    #endif
    rob_t retiringROBentry, newEmptyEntry;
    newEmptyEntry.completed = false;
    newEmptyEntry.ready = true;
    int retiredInstCount = 0;
    while (true){

        if ((*ROB)[0].completed  && (retiredInstCount < procsim_conf->fetch_width)){

            retiringROBentry = (*ROB)[0];
            (*ROB).erase((*ROB).begin());
            (*ROB).push_back(newEmptyEntry);

            #ifdef DEBUG
                
                printf("\tRetiring: ");
                print_instruction(&retiringROBentry.inst);
                printf("\n");


            #endif

            if (retiringROBentry.inst.opcode == 6){

                procsim_driver_update_predictor(retiringROBentry.inst.pc, retiringROBentry.inst.br_taken, retiringROBentry.inst.dyn_instruction_count);
            
            }


            
            retiredInstCount += 1;



            if (retiringROBentry.inst.mispredict){

                *retired_mispredict_out = true;

                break;
            }


        }
        else {

            #ifdef DEBUG
                if (!(*ROB)[0].ready  && (retiredInstCount < procsim_conf->fetch_width)){

                    printf("\tROB entry still in flight: dyncount=%lu\n", (*ROB)[0].inst.dyn_instruction_count );
                }
                
            #endif
            break;
            
        }
    }
    #ifdef DEBUG
        printf("\t%" PRIu64 " instructions retired\n", retiredInstCount);
    #endif

    stats->instructions_retired +=  retiredInstCount;
    return retiredInstCount; // TODO: Fix Me
}

// Optional helper function which is responsible for moving instructions
// through pipelined Function Units and then when instructions complete (that
// is, when instructions are in the final pipeline stage of an FU and aren't
// stalled there), setting the ready bits in the register file. This function 
// should remove an instruction from the scheduling queue when it has completed.

void updateSchdlingQafterExec(std::vector<uint64_t>* tagLists){

    uint64_t tag =0;
    uint64_t md_tag =0;
    int matIndex;
    bool lsuFlag = false;
    if (tagLists != nullptr){

            
        for (int k = 0; k< tagLists->size(); k++){

            tag = (*tagLists)[k];

            // remove inst that has been completed
            schedulingQueque_t newRS;
            newRS.ready = true;
            newRS.inFU = false;
            for (int i= 0; i < schedlingQ->size(); i++){

                if (((*schedlingQ)[i].destTag == tag) && (*schedlingQ)[i].inFU ){
            
                    #ifdef DEBUG
                        printf("\tProcessing Result Bus for: ");
                        print_instruction(&(*schedlingQ)[i].inst);
                        printf("\n");
                    #endif
                    (*schedlingQ)[i].inFU = false;
                    if (((*schedlingQ)[i].inst.opcode == 4) || ((*schedlingQ)[i].inst.opcode == 5) ){
                        
                        lsuFlag = true;
                        md_tag =  (*schedlingQ)[i].md_tag ;
                        for (int p= 0; p < MAT->size(); p++){

                            if ((*MAT)[p].tag == (*schedlingQ)[i].md_tag ){

                                (*MAT)[p].ready = true;
                                
                                matIndex = p;

                                #ifdef DEBUG
                                        printf("Instr is load/store, marking ready=1 for M%" PRId8 " (tag = %" PRIu64 ") in MAT\n",matIndex, (*schedlingQ)[i].md_tag);
                                #endif

                            }
                        }
                    }
                    else{
                        lsuFlag = false; //monitors completed instructioin type for md_tag search in schdling q
                    }



                    // update messy register file

                    if ((*schedlingQ)[i].inst.dest>-1){
                        if ((*futureRegsFile)[(*schedlingQ)[i].inst.dest].tag == tag){

                            (*futureRegsFile)[(*schedlingQ)[i].inst.dest].ready = true;

                            #ifdef DEBUG
                            
                                printf("Instr has dest, marking ready=1 for R%" PRId8 " (tag = %" PRIu64 ") in messy RF\n", (*schedlingQ)[i].inst.dest, tag);
                            #endif

                        }

                    }

                

                    #ifdef DEBUG
                        printf("Removing instruction with tag %" PRIu64 " from the scheduling queue\n", tag);
                        
                    #endif

                    // now remove
                    (*schedlingQ).erase((*schedlingQ).begin() + i);
                    (*schedlingQ).push_back(newRS);


                }

            }

            //update src in queque
            for (int i= 0; i < schedlingQ->size(); i++){

                if ((*schedlingQ)[i].src1tag == tag){
                    (*schedlingQ)[i].src1ready = true;

                    #ifdef DEBUG
                        printf("Setting rs1_ready = 1 for rd_tag %" PRIu64 " from the scheduling queue\n", tag);
                    #endif

                }
      

                if ((*schedlingQ)[i].src2tag == tag){
                    (*schedlingQ)[i].src2ready = true;
                    #ifdef DEBUG
                        printf("Setting rs2_ready = 1 for rd_tag %" PRIu64 " from the scheduling queue\n", tag);
                    #endif

                }



                
                if (lsuFlag){
                    if ((*schedlingQ)[i].ms_tag == md_tag){

                        (*schedlingQ)[i].ms_ready = true;
                

                        #ifdef DEBUG
                            printf("Setting ms_ready = 1 for md_tag %" PRIu64 " from the scheduling queue\n", md_tag);
                        #endif
                    }

                }


            }

            for (int i= 0; i < ROB->size(); i++){
                
                if ((*ROB)[i].tag == tag){

                    // update ROB
                    (*ROB)[i].completed = true;
                    #ifdef DEBUG
                        printf("Setting instruction with rd_tag %" PRIu64 " as done in the ROB\n", tag);
                    #endif

                }
            }
        
        }

    }
}


static void stage_exec(procsim_stats_t *stats) {
    // TODO: fill me 


    std::vector<uint64_t>* postExecProcessQ = new std::vector<uint64_t>();


    int instCompletedCount = 0;
    uint64_t dynCount;
    #ifdef DEBUG
        printf("Stage Exec: \n"); //  PROVIDED
    #endif

        // Progress ALUs
    #ifdef DEBUG
        printf("\tProgressing ALU units\n");  // PROVIDED
    #endif

    //for alu
    for (int j= 0; j< alu->size(); j++){
        if (!(*alu)[j].ready){
            (*alu)[j].ready = true;


            #ifdef DEBUG
             
                for (int i= 0; i < schedlingQ->size(); i++){

                        if ((*schedlingQ)[i].destTag == (*alu)[j].tag){

                            dynCount = (*schedlingQ)[i].inst.dyn_instruction_count;
                            break;
                        }

                }

                printf("\tCompleting ALU: %ld, for dyncount=%" PRIu64 "\n", j, dynCount);
            #endif
            //update ROB with match tag completed
            // update all src regs with tag to ready in schdling queque
            //remove inst with same dest tag in the schdling q 
            // update messy register file
            instCompletedCount += 1;
            postExecProcessQ->push_back((*alu)[j].tag);
            //updateSchdlingQafterExec();


        }
    }



    //for MUL
    // Progress MULs
    #ifdef DEBUG
        printf("\tProgressing MUL units\n");  // PROVIDED
    #endif


    for (int j= 0; j< mul->size(); j++){

        if (!(*mul)[j].ready3){

            #ifdef DEBUG
             

                for (int i= 0; i < schedlingQ->size(); i++){

                        if ((*schedlingQ)[i].destTag == (*mul)[j].tag3){

                            dynCount = (*schedlingQ)[i].inst.dyn_instruction_count;
                            break;
                        }

                }

                printf("\tCompleting MUL: %ld, for dyncount=%" PRIu64 "\n", j, dynCount);
               
            #endif
            instCompletedCount += 1;
            //if stage 3 is busy, proceed one cycle and tag3 inst will be completed. update necessary
            
            postExecProcessQ->push_back((*mul)[j].tag3);
        }
                // if stage 2 is busy, proceed one cycle and tag2 will be moved to tag3
        if ((*mul)[j].ready2){
            (*mul)[j].ready3 = true;
        }
        else{
            (*mul)[j].tag3 =  (*mul)[j].tag2;
            (*mul)[j].ready3 = false;
        }
        if ((*mul)[j].ready1){
            (*mul)[j].ready2 = true;
        }
        else{
            (*mul)[j].tag2 =  (*mul)[j].tag1;
            (*mul)[j].ready2 = false;
            (*mul)[j].ready1 = true;
        }
    }    


    //for LSU

    // Progress LSU loads / stores
    #ifdef DEBUG
        printf("\tProgressing LSU units\n");  // PROVIDED
    #endif

    for (int j= 0; j< lsu->size(); j++){

        if (!(*lsu)[j].ready){


            if ((*lsu)[j].inst.opcode == OPCODE_STORE){
                (*lsu)[j].ready = true;

                instCompletedCount += 1;
                #ifdef DEBUG
                    printf("\tCompleting LSU: %ld, for dyncount=%" PRIu64 "\n", j, (*lsu)[j].inst.dyn_instruction_count);
                #endif
                postExecProcessQ->push_back((*lsu)[j].tag);
            }
            else if ((*lsu)[j].inst.opcode = OPCODE_LOAD){

                if ((*lsu)[j].stallStarted){

                    (*lsu)[j].stalledCycles += 1;
                    if ((*lsu)[j].stalledCycles == (*lsu)[j].maxStall){


                        instCompletedCount += 1;
                        #ifdef DEBUG
                            printf("\tCompleting LSU: %ld, for dyncount=%" PRIu64 "\n", j, (*lsu)[j].inst.dyn_instruction_count);
               
                        #endif
                        postExecProcessQ->push_back((*lsu)[j].tag);
                        (*lsu)[j].ready = true;
                        (*lsu)[j].stallStarted = false;
                        (*lsu)[j].maxStall = 0;
                        (*lsu)[j].stalledCycles = 0;
                    }
                    else{

                        #ifdef DEBUG

                        printf("\tLSU: %ld, needs to wait %" PRIu64 " cycles to complete\n", j, ((*lsu)[j].maxStall- (*lsu)[j].stalledCycles));

                        #endif
                    }
                    
                }
                else{

                    (*lsu)[j].stallStarted = true;
                    if ( (*lsu)[j].inst.dcache_hit == CACHE_LATENCY_L1_HIT){
                        (*lsu)[j].maxStall = 2;
                    }
                    else if ( (*lsu)[j].inst.dcache_hit == CACHE_LATENCY_L2_HIT){

                        (*lsu)[j].maxStall = 10;
                    }
                    else if ( (*lsu)[j].inst.dcache_hit == CACHE_LATENCY_L2_MISS){
                        (*lsu)[j].maxStall = 100;
                    }

                    (*lsu)[j].stalledCycles += 1;
                    #ifdef DEBUG

                    printf("\tLSU: %ld, needs to wait %" PRIu64 " cycles to complete\n", j, ((*lsu)[j].maxStall- (*lsu)[j].stalledCycles));

                    #endif


                                   
                }
 

            }
        }

    }



// 	Processing Result Busses
// 	Processing Result Bus for: opcode=ADD, dest=R14, src1=R10, src2=R14, dyncount=30
// Removing instruction with tag 155 from the scheduling queue
// Setting rs1_ready = 1 for rd_tag 155 from the scheduling queue
// Setting instruction with rd_tag 155 as done in the ROB




    // Apply Result Busses
#ifdef DEBUG
    printf("\tProcessing Result Busses\n"); // PROVIDED

#endif
if (postExecProcessQ != nullptr){

    updateSchdlingQafterExec(postExecProcessQ);
}
else{
#ifdef DEBUG
    printf("\tNullPtr");
#endif

}


#ifdef DEBUG
    printf("\t%" PRIu64 " instructions completed\n", instCompletedCount);
#endif

delete postExecProcessQ;

}

// Optional helper function which is responsible for looking through the
// scheduling queue and firing instructions that have their source pregs
// marked as ready. Note that when multiple instructions are ready to fire
// in a given cycle, they must be fired in program order. 
// Also, load and store instructions must be fired according to the 
// memory disambiguation algorithm described in the assignment PDF. Finally,
// instructions stay in their reservation station in the scheduling queue until
// they complete (at which point stage_exec() above should free their RS).
static void stage_schedule(procsim_stats_t *stats) {

    //search through the scheduling quuqe to see both src register ready and fire if FU is avaiable

    #ifdef DEBUG
        printf("Stage Schedule: \n"); //  PROVIDED
    #endif
    int instFiredCount = 0;

    for (int i=0; i < schedlingQ->size(); i++){

        if (!(*schedlingQ)[i].ready && !(*schedlingQ)[i].inFU && (*schedlingQ)[i].src1ready && (*schedlingQ)[i].src2ready && (*schedlingQ)[i].ms_ready){
            
            #ifdef DEBUG
                printf("\tAttempting to fire instruction: ");
                print_instruction(&(*schedlingQ)[i].inst);
                printf(" to \n");
            #endif
            if ((*schedlingQ)[i].FU == OPCODE_MUL){

                for (int j = 0; j< mul->size(); j++){
                    if ((*mul)[j].ready){

                        #ifdef DEBUG
                            printf("\t\tFired to MUL #%" PRIu64 "\n", j);
                        #endif
                        instFiredCount += 1;
                        (*mul)[j].ready = false;
                        (*mul)[j].ready1 = false;
                        (*mul)[j].tag1 = (*schedlingQ)[i].destTag;
                        (*schedlingQ)[i].inFU = true;
                        break;
                    }
                    if (j == (mul->size()-1)){

                        #ifdef DEBUG
                            printf("\t\tCannot fire instruction\n");
                        #endif
                    }
                
                }

            }
            else if (((*schedlingQ)[i].FU == OPCODE_ADD) || ((*schedlingQ)[i].FU == OPCODE_BRANCH)){

                for (int j= 0; j< alu->size(); j++){
                    if ((*alu)[j].ready){


                        #ifdef DEBUG
                            printf("\t\tFired to ALU #%" PRIu64 "\n", j);
                        #endif
                        (*schedlingQ)[i].inFU = true;
                        instFiredCount += 1;
                        (*alu)[j].ready = false;
                        (*alu)[j].tag = (*schedlingQ)[i].destTag;
                        break;
                    }

                    if (j == (alu->size()-1)){

                        #ifdef DEBUG
                            printf("\t\tCannot fire instruction\n");
                        #endif
                    }
                }

            }
            else if (((*schedlingQ)[i].FU == OPCODE_LOAD) || ((*schedlingQ)[i].FU == OPCODE_STORE)){
                // check for ms_ready as well
               

                for (int j= 0; j< lsu->size(); j++){
                    if ((*lsu)[j].ready){

                        #ifdef DEBUG
                            printf("\t\tFired to LSU #%" PRIu64 "\n", j);
                        #endif
                        (*schedlingQ)[i].inFU = true;
                        instFiredCount += 1;
                        (*lsu)[j].ready = false;
                        (*lsu)[j].tag = (*schedlingQ)[i].destTag;
                        (*lsu)[j].inst = (*schedlingQ)[i].inst;
                        break;
                    }

                if (j == (lsu->size()-1)){

                    #ifdef DEBUG
                        printf("\t\tCannot fire instruction\n");
                    #endif
                }
                }




            }

        }

    

    }




    for (int j = 0; j< mul->size(); j++){
        (*mul)[j].ready = true;
    }

    #ifdef DEBUG
        if (instFiredCount == 0){
            printf("\tCould not find scheduling queue entry to fire this cycle\n");
        }
        printf("\t%" PRIu64 " instructions fired\n",  instFiredCount);

    #endif
    if (instFiredCount == 0){
        
        stats->no_fire_cycles += 1;
    }
    

}

// Optional helper function which looks through the dispatch queue, decodes
// instructions, and inserts them into the scheduling queue. Dispatch should
// not add an instruction to the scheduling queue unless there is space for it
// in the scheduling queue and the ROB; 
// effectively, dispatch allocates reservation stations and ROB space for 
// each instruction dispatched and stalls if there any are unavailable. 
// Note the scheduling queue has a configurable size and the ROB has F * 32 entries.
// The PDF has details.
// dispatch width is fetch width

bool isDispQempty(std::vector<dispQ_t>* dispatchQ){
    
    if (dispatchQ != nullptr){

        for (int j= 0; j < robSize; j++){

            if ((*dispatchQ)[j].ready == false){
                return false;
            }

        }

    }

    return true;
}



bool isSchdQueqefull(std::vector<schedulingQueque_t>* schedlingQ){

    if (schedlingQ != nullptr){
        for (int j= 0; j < schedlingQ->size(); j++){

            if ((*schedlingQ)[j].ready == true){
                return false;
            }

        }
    }
return true;

}

bool isROBfull(std::vector<rob_t>* ROB_1){

    if (ROB_1 != nullptr){

        for (int j= 0; j < robSize; j++){

            if ((*ROB_1)[j].ready == true){
                return false;
            }

        }
    }


    return true;

}

static void stage_dispatch(procsim_stats_t *stats) {

#ifdef DEBUG
    printf("Stage Dispatch: \n"); //  PROVIDED
#endif
    // TODO: fill me in
// - dispatch instruction from dispatch queqe head (program order) if

//     . dispatch queqe is non empty
//     . Scheduling queqe and ROB is not full
//     . # of non-NOP dispatched < fetch width;
//     . # of NOP dispatched < fetch width;

int countNOPdisp = 0;
int countNotNOPdisp = 0;

dispQ_t dispQentry; //to be dispatched
dispQ_t newDispEntry; //new disp entry to add in the dispQ
newDispEntry.ready = true;
newDispEntry.nop = false;

while (true){






    // if (isROBfull(ROB)){
    //         stats->rob_no_dispatch_cycles += 1;
    //         #ifdef DEBUG
    //                 printf("\tDispatch stalled due to insufficient ROB space\n");
    //         #endif
    //         break;
    // }

    if (isROBfull(ROB)){


    }

        

    if (!isDispQempty(dispatchQ) && !(*dispatchQ)[0].ready  && !isSchdQueqefull(schedlingQ) && (countNOPdisp < procsim_conf->fetch_width) && (countNotNOPdisp < procsim_conf->fetch_width)){

        if (!isROBfull(ROB)){
            //dispatch
            dispQentry = (*dispatchQ)[0];

            #ifdef DEBUG

                if (!(*dispatchQ)[0].ready && !dispQentry.nop){
                    printf("\tAttempting Dispatch for: ");
                    print_instruction(&(*dispatchQ)[0].inst);
                    printf("\n");

                }


            #endif 

            //remove from dispQ
            (*dispatchQ).erase((*dispatchQ).begin());  
            (*dispatchQ).push_back(newDispEntry);
            if (dispQentry.nop){
                countNOPdisp += 1;
            }
            else{

                countNotNOPdisp += 1;
                
                //add to schdlingQ
                for (int j= 0; j< schedlingQ->size(); j++){

                    if ((*schedlingQ)[j].ready == true){



                        //ready slot to dispatch

                        //ms and md
                

                        //check if the opcode is of load or store
                        //hash load_store_addr[6:11] and use it in following
                        if ((dispQentry.inst.opcode == 4) || (dispQentry.inst.opcode == 5)){



                            // assign the previous tag and ready bit to the reservation station. if it is ready then tag can be ignored
                            (*schedlingQ)[j].ms_tag = (*MAT)[((dispQentry.inst.load_store_addr >> 6) & 0x3F)].tag;
                            (*schedlingQ)[j].ms_ready = (*MAT)[((dispQentry.inst.load_store_addr >> 6) & 0x3F)].ready;
                            (*schedlingQ)[j].md_tag = globalTagCounter;

                            // assign the memory register now busy. and assign a tag to it. clear this busy bit after completion in exection stage
                            (*MAT)[((dispQentry.inst.load_store_addr >> 6) & 0x3F)].tag = (*schedlingQ)[j].md_tag ;
                            (*MAT)[((dispQentry.inst.load_store_addr >> 6) & 0x3F)].ready = false;

                            #ifdef DEBUG
                                printf("\t\tInstr has load_store_addr=0x%" PRIx64 ", addr_class=0x%" PRIx64 ", setting ms_tag=%"  PRIu64 ", ms_ready=%"  PRIu64 ", md_tag=%" PRIu64 "\n",dispQentry.inst.load_store_addr, ((dispQentry.inst.load_store_addr >> 6) & 0x3F), (*schedlingQ)[j].ms_tag, (*schedlingQ)[j].ms_ready, (*schedlingQ)[j].md_tag);
                                printf("\t\tIncrease tag counter by 1\n");
                                printf("\t\tInstr is load/store, marking ready=0 and assigning tag=%" PRIu64 " for M%" PRId8 " in MAT\n", globalTagCounter, ((dispQentry.inst.load_store_addr >> 6) & 0x3F));
                            #endif
                            
                            globalTagCounter += 1;
                            
                        }
                        else{

                            (*schedlingQ)[j].ms_tag = 0;
                            (*schedlingQ)[j].ms_ready = true;
                            (*schedlingQ)[j].md_tag = globalTagCounter;

                            #ifdef DEBUG
                        
                                printf("\t\tInstr is not load/store, setting ms_tag=0, ms_ready=1, md_tag=%" PRIu64 "\n", globalTagCounter);
                                printf("\t\tIncrease tag counter by 1\n");
                            #endif
                            globalTagCounter += 1;
                        }


                        //assign FU type
                        (*schedlingQ)[j].FU = dispQentry.inst.opcode;

                        //assign inst
                        (*schedlingQ)[j].inst = dispQentry.inst;


                        // src1 assign
                        if ((*schedlingQ)[j].inst.src1 == -1){

                            (*schedlingQ)[j].src1ready = true;

                            #ifdef DEBUG
                                printf("\t\tInstr does not have src1,  setting src1_tag=0, src1_ready=1 \n");
                            #endif
                        }
                        else{

                            #ifdef DEBUG
                                printf("\t\tInstr has src1, ");
                            #endif

                            if ((*futureRegsFile)[(*schedlingQ)[j].inst.src1].ready){

                            #ifdef DEBUG
                                printf("setting src1_tag=%" PRIu64 ", src1_ready=1\n", (*futureRegsFile)[(*schedlingQ)[j].inst.src1].tag );
                            
                            #endif


                                (*schedlingQ)[j].src1ready = true;
                            }
                            else{
                                (*schedlingQ)[j].src1tag = (*futureRegsFile)[(*schedlingQ)[j].inst.src1].tag;
                                (*schedlingQ)[j].src1ready = false;

                                #ifdef DEBUG
                                    printf("setting src1_tag=%" PRIu64 ", src1_ready=0\n",  (*schedlingQ)[j].src1tag );
                    
                                #endif
                            }
                        }

                        //src2 assign


                        if ((*schedlingQ)[j].inst.src2 == -1){
                            #ifdef DEBUG

                                printf("\t\tInstr does not have src2, setting src2_tag=0, src2_ready=1\n");
                            #endif
                            (*schedlingQ)[j].src2ready = true;
                        }
                        else{

                            #ifdef DEBUG
                                printf("\t\tInstr has src2, ");
                            #endif

                            if ((*futureRegsFile)[(*schedlingQ)[j].inst.src2].ready){
                                
                                (*schedlingQ)[j].src2ready = true;

                                #ifdef DEBUG
                                    printf("setting src2_tag=%" PRIu64 ", src2_ready=1\n", (*futureRegsFile)[(*schedlingQ)[j].inst.src2].tag );
                                
                                #endif


                            }
                            else{
                                (*schedlingQ)[j].src2tag = (*futureRegsFile)[(*schedlingQ)[j].inst.src2].tag;
                                (*schedlingQ)[j].src2ready = false;
                            #ifdef DEBUG
                                printf("setting src2_tag=%" PRIu64 ", src2_ready=0\n",  (*schedlingQ)[j].src2tag );
                
                            #endif


                            }

                        }

                        (*schedlingQ)[j].destTag = globalTagCounter;

                        //allocate room in the ROB
                        for (int j= 0; j< ROB->size(); j++){
                            if ((*ROB)[j].ready == true){
                                (*ROB)[j].inst = dispQentry.inst;
                                (*ROB)[j].ready = false;
                                (*ROB)[j].tag = globalTagCounter;
                                break;
                            }
                        }

                        #ifdef DEBUG
                            printf("\t\tsetting rd_tag=%" PRIu64 "\n", globalTagCounter);
                            printf("\t\tIncrease tag counter by 1\n");
                        #endif

                        globalTagCounter += 1;


                        
                        if ((*schedlingQ)[j].inst.dest > -1){

                            //dest tag assign and update future regs with the new tag. make the regs not ready.
                            
                            (*futureRegsFile)[(*schedlingQ)[j].inst.dest].tag = (*schedlingQ)[j].destTag;
                            (*futureRegsFile)[(*schedlingQ)[j].inst.dest].ready = false;

                            #ifdef DEBUG
                                printf("\tInstr has dest, marking ready=0 and assigning tag=%" PRIu64 " for R%" PRId8 " in messy RF\n", (*schedlingQ)[j].destTag, (*schedlingQ)[j].inst.dest);
                            #endif

                        }


                        //make the entry busy
                        (*schedlingQ)[j].ready =false;


                        #ifdef DEBUG
                            printf("\tDispatching and adding the instruction to the scheduling queue\n");
            
                        #endif



                        break;
                    }


                }

                // if (isROBfull(ROB) && (countNotNOPdisp < procsim_conf->fetch_width)){
                //         stats->rob_no_dispatch_cycles += 1;
                //         #ifdef DEBUG
                //                 printf("\tDispatch stalled due to insufficient ROB space\n");
                //         #endif
                //         break;
                // }
                // else if (isROBfull(ROB) ){
                //     break;
                // }



            }

        }
        else{

            stats->rob_no_dispatch_cycles += 1;
            #ifdef DEBUG
                    printf("\tDispatch stalled due to insufficient ROB space\n");
            #endif
            break;

        }
    

    }
    else{

        break;
    }

    
    

}

#ifdef DEBUG
    printf("\t%" PRIu64 " instructions dispatched and added to the scheduling queue\n", (countNotNOPdisp));
#endif


}

// Optional helper function which fetches instructions from the instruction
// cache using the provided procsim_driver_read_inst() function implemented
// in the driver and appends them to the dispatch queue. 
// The size of the dispatch queue is same as number of rob entries.
// Fetch should not add an instruction to the dispatch queue unless there
// is space for it in the dispatch queue 
// If a NULL pointer is fetched from the procsim_driver_read_inst,
// insert a NOP to the scheduling queue
// that NOP should be dropped at the dispatch stage
bool isDispFull(){

    for (int i=0; i< robSize; i++){
        if ((*dispatchQ)[i].ready){
            return false;
        }

    }
    return true;

}


static void stage_fetch(procsim_stats_t *stats) {


    // TODO: fill me in
    #ifdef DEBUG
        printf("Stage Fetch: \n"); //  PROVIDED
    #endif

    driver_read_status_t read_status;
    inst_t inst;
    bool dispQfull = false;

   

    for (int i=0; i< procsim_conf->fetch_width; i++){

        dispQfull = isDispFull();
        if (!dispQfull){


            inst_t *ptr = (inst_t*)procsim_driver_read_inst(&read_status);
            if (ptr == NULL) {

            } 
            else {
                inst = *ptr;
            }



            //if mispredict or end of trace just ignore and avoid adding to dispQ
            if (read_status == driver_read_status_t::DRIVER_READ_MISPRED) {
                #ifdef DEBUG
            
                            
                            
                    printf("\tFetched NOP because of branch misprediction\n");
                #endif

                continue;
            }
            else if (read_status == driver_read_status_t::DRIVER_READ_END_OF_TRACE){
                
                #ifdef DEBUG
                    printf("\tFetched NOP because of end of trace is reached\n");
                #endif

                continue;;
            }
            
            //else add to dispQ if room is available
            else if (dispQfull == false) {

                for (int j= 0; j < dispatchQ->size(); j++){

                    
                    if ((*dispatchQ)[j].ready == true){

                        if (read_status == driver_read_status_t::DRIVER_READ_ICACHE_MISS){

                        

                            #ifdef DEBUG
                                printf("\tFetched NOP because of icache miss\n");
                                printf("\tPushed an NOP into the dispatch queue on icache miss\n");
                            #endif

                            //add NOP in the dispQ 
                            (*dispatchQ)[j].ready = false;
                            (*dispatchQ)[j].nop = true; 
                            break;
                

                        }
                        else if (read_status == driver_read_status_t::DRIVER_READ_OK){


                            //add inst to the dispQ
                            (*dispatchQ)[j].ready = false;
                            (*dispatchQ)[j].inst = inst; 

                            #ifdef DEBUG
                                printf("\tFetched Instruction: ");
                                print_instruction(&inst);
                                printf("\n");
                            #endif

                            if (inst.opcode == 6){
                                stats->num_branch_instructions += 1;


                            #ifdef DEBUG
                                if (inst.mispredict){

                                    printf("\t\tBranch Misprediction will be handled by driver\n");
                                }
                            #endif


                            }


                                
                            stats->instructions_fetched += 1;

                            break;
                        }

                    }
                    else if ((j == (dispatchQ->size()-1)) && ((*dispatchQ)[j].ready == false) ){

                        dispQfull = true;

                        #ifdef DEBUG
                        printf("\tNot fetching because the dispatch queue is full\n");
                        #endif
                        break;

                    }

                }

            }
            else {
                break;
            }
        }

    }


    

}

// Use this function to initialize all your data structures, simulator
// state, and statistics.

void procsim_init(const procsim_conf_t *sim_conf, procsim_stats_t *stats) {
    // TODO: fill me in
    robSize =  sim_conf->num_rob_entries;
    int schdQsize = sim_conf->num_schedq_entries_per_fu* (sim_conf->num_alu_fus + sim_conf->num_lsu_fus + sim_conf->num_mul_fus);
    int resultBusSize = sim_conf->num_alu_fus + sim_conf->num_lsu_fus + sim_conf->num_mul_fus;
    procsim_conf = sim_conf;
    dispatchQ = new std::vector<dispQ_t>(robSize);
    schedlingQ = new std::vector<schedulingQueque_t>(schdQsize);
    Aregs = new std::vector<register_tt>(32);
    MAT = new std::vector<register_tt>(64);
    resultBuses = new std::vector<CDB_t>(resultBusSize);
    globalTagCounter = 96;
    futureRegsFile = new std::vector<register_tt>(32);
    ROB = new std::vector<rob_t>(robSize);
    alu = new std::vector<ALU_t>(sim_conf->num_alu_fus);
    mul = new std::vector<MUL_t>(sim_conf->num_mul_fus);
    lsu = new std::vector<LSU_t>(sim_conf->num_lsu_fus);



    // initialize future register file
    for (int i=0; i< futureRegsFile->size(); i++){

        (*futureRegsFile)[i].ready = true;
        (*futureRegsFile)[i].tag = i;
    }


    // initialize memory alias table 
    for (int i=0; i< MAT->size(); i++){

        (*MAT)[i].ready = true;
        (*MAT)[i].tag = i+32;

    }

    // initialize ROB and dispQ as empty
    for (int i=0; i< dispatchQ->size(); i++){

        (*dispatchQ)[i].ready = true;
        (*dispatchQ)[i].nop = false;
        (*ROB)[i].ready = true;
        (*ROB)[i].completed = false;
        (*ROB)[i].tag = 0;

    }
    for (int i=0; i< schdQsize; i++){

        (*schedlingQ)[i].ready = true;
        (*schedlingQ)[i].inFU = false;
        (*schedlingQ)[i].src2tag = 0;
        (*schedlingQ)[i].ms_tag = 0;
        (*schedlingQ)[i].src1tag = 0;
        (*schedlingQ)[i].destTag = 0;

    }
    for (int i=0; i< resultBusSize; i++){

        (*resultBuses)[i].ready = true;

    }

    for (int j = 0; j< mul->size(); j++){
        (*mul)[j].ready1 = true;
        (*mul)[j].ready2 = true;
        (*mul)[j].ready3 = true;
        (*mul)[j].ready = true;

    }
    for (int j = 0; j< alu->size(); j++){
        (*alu)[j].ready = true;
    }
    for (int j = 0; j< lsu->size(); j++){
        (*lsu)[j].ready = true;
        (*lsu)[j].stalledCycles = 0;
        (*lsu)[j].stallStarted = false;
    }


#ifdef DEBUG
    printf("\nScheduling queue capacity: %lu instructions\n", schdQsize); // TODO: Fix Me
    printf("\n"); //  PROVIDED
#endif
}

// To avoid confusion, we have provided this function for you. Notice that this
// calls the stage functions above in reverse order! This is intentional and
// allows you to avoid having to manage pipeline registers between stages by
// hand. This function returns the number of instructions retired, and also
// returns if a mispredict was retired by assigning true or false to
// *retired_mispredict_out, an output parameter.
uint64_t procsim_do_cycle(procsim_stats_t *stats,
                          bool *retired_mispredict_out) {
#ifdef DEBUG
    printf("================================ Begin cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
#endif

    // stage_state_update() should set *retired_mispredict_out for us
    uint64_t retired_this_cycle = stage_state_update(stats, retired_mispredict_out);

    if (*retired_mispredict_out) {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Retired mispredict, so notifying driver to fetch correctly!\n", retired_this_cycle); //  PROVIDED
#endif

        // After we retire a misprediction, the other stages don't need to run
        stats->branch_mispredictions++;
    } else {
#ifdef DEBUG
        printf("%" PRIu64 " instructions retired. Did not retire mispredict, so proceeding with other pipeline stages.\n", retired_this_cycle); //  PROVIDED
#endif

        // If we didn't retire an interupt, then continue simulating the other
        // pipeline stages
        stage_exec(stats);
        stage_schedule(stats);
        stage_dispatch(stats);
        stage_fetch(stats);
    }

    int dispQusage, schdQusage, robUsage;
    dispQusage = usageMeterDispQ();
    schdQusage = usageMeterSchdQ();
    robUsage = usageMeterRob();

    if (dispQusage > stats->dispq_max_usage){
        stats->dispq_max_usage = dispQusage;
    }
    if (schdQusage > stats->schedq_max_usage){
        stats->schedq_max_usage = schdQusage;
    }
    if (robUsage > stats->rob_max_usage){
        stats->rob_max_usage = robUsage;
    }

    stats->rob_avg_usage += robUsage;
    stats->schedq_avg_usage += schdQusage;
    stats->dispq_avg_usage += dispQusage;

#ifdef DEBUG
    printf("End-of-cycle dispatch queue usage: %lu\n", dispQusage); // TODO: Fix Me
    printf("End-of-cycle sched queue usage: %lu\n", schdQusage); // TODO: Fix Me
    printf("End-of-cycle ROB usage: %lu\n", robUsage); // TODO: Fix Me
    printf("End-of-cycle Messy RF state:\n"); // PROVIDED
    print_messy_rf(); // PROVIDED
    printf("End-of-cycle MAT state:\n"); // PROVIDED
    print_mat(); // PROVIDED
    printf("End-of-cycle ROB state:\n"); // PROVIDED
    print_rob(); // PROVIDED
    printf("================================ End cycle %" PRIu64 " ================================\n", stats->cycles); //  PROVIDED
    print_instruction(NULL); // this makes the compiler happy, ignore it
#endif

    // TODO: Increment max_usages and avg_usages in stats here!
    stats->cycles++;

    // Return the number of instructions we retired this cycle (including the
    // interrupt we retired, if there was one!)
    return retired_this_cycle;
}

// Use this function to free any memory allocated for your simulator and to
// calculate some final statistics.
void procsim_finish(procsim_stats_t *stats) {


    // TODO: fill me in

    stats->rob_avg_usage /= stats->cycles;
    stats->schedq_avg_usage /= stats->cycles;
    stats->dispq_avg_usage /= stats->cycles;
    stats->ipc = static_cast<double>(stats->instructions_retired) / stats->cycles;

    delete dispatchQ;
    delete schedlingQ;
    delete Aregs;
    delete MAT;
    delete resultBuses;
    delete futureRegsFile;
    delete ROB;
    delete mul;
    delete lsu;
    delete alu;
}
