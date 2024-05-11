#include "branchsim.hpp"
#include <cmath>
#include<iostream>

Counter_t *yehPattPT;
uint64_t *hisTab;
uint64_t yehpattPCmask;
uint64_t yehpattHisMask;
uint64_t yehpattIndex;
uint64_t history;

void yeh_patt_init_predictor(branchsim_conf_t *sim_conf) {
    // create history table of 2^H entries (all entries initialized to 0)
    uint64_t hisTabSize = pow(2, sim_conf -> H);
    uint64_t ptSize = pow(2, sim_conf->P);
    
    hisTab = (uint64_t *)malloc(hisTabSize * sizeof(uint64_t));
    for (int i = 0; i < hisTabSize ; i++){
        hisTab[i] = 0;
    }

    // create pattern table of 2^P entries init to weakly not taken
    yehPattPT = (Counter_t *)malloc(ptSize * sizeof(Counter_t));
    for (int i = 0; i < ptSize ; i++){

        Counter_init(&yehPattPT[i], 2);
    }

    yehpattHisMask = ((1UL << sim_conf->P )) - 1;
    yehpattPCmask = ((1UL << (sim_conf ->H + 2)) - 1) & (~((1UL << 2) - 1));


#ifdef DEBUG
    printf("Yeh-Patt: Creating a history table of %" PRIu64 " entries of length %" PRIu64 "\n", hisTabSize, sim_conf->P); // TODO: FIX ME
    printf("Yeh-Patt: Creating a pattern table of %" PRIu64 " 2-bit saturating counters\n", ptSize); // TODO: FIX ME
#endif
}

bool yeh_patt_predict(branch_t *br) {
#ifdef DEBUG
    printf("\tYeh-Patt: Predicting... \n"); // PROVIDED
#endif

    // add your code here
    yehpattIndex = (br->ip & yehpattPCmask) >> 2;
    history = hisTab[yehpattIndex];




#ifdef DEBUG
    printf("\t\tHT index: 0x%" PRIx64 ", History: 0x%" PRIx64 ", PT index: 0x%" PRIx64 ", Prediction: %d\n", yehpattIndex, history, history, Counter_isTaken(&yehPattPT[history])); // TODO: FIX ME
#endif
    return Counter_isTaken(&yehPattPT[history]); // TODO: FIX ME
}

void yeh_patt_update_predictor(branch_t *br) {
#ifdef DEBUG
    printf("\tYeh-Patt: Updating based on actual behavior: %d\n", (int) br->is_taken); // PROVIDED
#endif

    // add your code here
    yehpattIndex = (br->ip & yehpattPCmask) >> 2;
    history = hisTab[yehpattIndex];


    // add your code here
    Counter_update(&yehPattPT[history], br->is_taken);
  
    hisTab[yehpattIndex] = (hisTab[yehpattIndex] << 1);

    if (br->is_taken){
        hisTab[yehpattIndex] += 1;
    }

    hisTab[yehpattIndex] = hisTab[yehpattIndex] & yehpattHisMask;

#ifdef DEBUG
    printf("\t\tHT index: 0x%" PRIx64 ", History: 0x%" PRIx64 ", PT index: 0x%" PRIx64 ", New Counter: 0x%" PRIx64 ", New History: 0x%" PRIx64 "\n", yehpattIndex, history, history, yehPattPT[hisTab[yehpattIndex]], hisTab[yehpattIndex]); // TODO: FIX ME
#endif
}

void yeh_patt_cleanup_predictor() {
    // add your code here
    yehPattPT = NULL;
    hisTab = NULL;
}

//Implementation plan:
// 1. define array of counter with size 2^P (init)
// 2. each counter is indexed by hash(pc xor GHR) (predict)
// 3. update counter wtih actual behaviour and update GHR as well

Counter_t *patternTable;
uint64_t GHR;
uint64_t GHRmask, PCmask; 
uint64_t index ;
void gshare_init_predictor(branchsim_conf_t *sim_conf) {
    // create pattern table of 2^P entries init to weakly not taken

    uint64_t ptSize = pow(2, sim_conf->P);
    patternTable = (Counter_t *)malloc(ptSize * sizeof(Counter_t));

    for (int i = 0; i < ptSize ; i++){

        Counter_init(&patternTable[i], 2);
    }
  
    // initialize GHR to 0
    GHRmask = (1UL << (sim_conf ->P)) - 1;
    PCmask = ((1UL << (sim_conf ->P + 2)) - 1) & (~((1UL << 2) - 1));
    GHR= 0;


#ifdef DEBUG
    printf("Gshare: Creating a pattern table of %" PRIu64 " 2-bit saturating counters\n", ptSize); // TODO: FIX ME
#endif
}

bool gshare_predict(branch_t *br) {
#ifdef DEBUG
    printf("\tGshare: Predicting... \n"); // PROVIDED
#endif

    // add your code here
    // find GHR [P-1:0] and PC[2+P-1:2]
    // hash to index the pattern table
    // predict based on the state of the counter

    index = (GHR & GHRmask) ^ ((br->ip & PCmask) >> 2);




#ifdef DEBUG
    printf("\t\tHistory: 0x%" PRIx64 ", Counter index: 0x%" PRIx64 ", Prediction: %d\n", (GHR & GHRmask), index, Counter_isTaken(&patternTable[index]));
#endif
    return Counter_isTaken(&patternTable[index]); // TODO: FIX ME
}

void gshare_update_predictor(branch_t *br) {
#ifdef DEBUG
    printf("\tGshare: Updating based on actual behavior: %d\n", (int) br->is_taken); // PROVIDED
#endif

    index = (GHR & GHRmask) ^ ((br->ip & PCmask) >> 2);
    // add your code here
    Counter_update(&patternTable[index], br->is_taken);
    //GHR update
    GHR = GHR << 1;
    if ( br->is_taken )
        GHR +=  1;


#ifdef DEBUG
    printf("\t\tHistory: 0x%" PRIx64 ", Counter index: 0x%" PRIx64 ", New Counter value: 0x%" PRIx64 ", New History: 0x%" PRIx64 "\n", ((GHR >> 1) & GHRmask), index, Counter_get(&patternTable[index]), (GHR & GHRmask)); // TODO: FIX ME
#endif
}

void gshare_cleanup_predictor() {
    // add your code here
    //patternTable = NULL;
    GHR = 0; 
    GHRmask = 0;
    PCmask = 0; 
    index = 0 ;
    free(patternTable);
}

/**
 *  Function to update the branchsim stats per prediction
 *
 *  @param prediction The prediction returned from the predictor's predict function
 *  @param br Pointer to the branch_t that is being predicted -- contains actual behavior
 *  @param sim_stats Pointer to the simulation statistics -- update in this function
*/
void branchsim_update_stats(bool prediction, branch_t *br, branchsim_stats_t *sim_stats) {


    sim_stats->num_branch_instructions +=1;
    if (prediction == br->is_taken){
        sim_stats->num_branches_correctly_predicted += 1;
    }
    else
        sim_stats->num_branches_mispredicted += 1;
}

/**
 *  Function to cleanup branchsim statistic computations such as prediction rate, etc.
 *
 *  @param sim_stats Pointer to the simulation statistics -- update in this function
*/
void branchsim_finish_stats(branchsim_stats_t *sim_stats) {

    sim_stats->fraction_branch_instructions =static_cast<double>(sim_stats->num_branch_instructions)/(sim_stats->total_instructions);

    sim_stats->prediction_accuracy = static_cast<double>(sim_stats->num_branches_correctly_predicted)/sim_stats->num_branch_instructions;
  
}


