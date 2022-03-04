#ifndef SIM_CACHE_H
#define SIM_CACHE_H

typedef struct cache_params{
    unsigned long int block_size;
    unsigned long int l1_size;
    unsigned long int l1_assoc;
    unsigned long int vc_num_blocks;
    unsigned long int l2_size;
    unsigned long int l2_assoc;
}cache_params;

typedef struct cache_gen_params{
    unsigned long int block_size;
    unsigned long int l_cache_levels;
    unsigned long int vc_num_blocks;
    //unsigned long int vc_caches;
}cache_gen_params;

typedef struct level_params{

    
    unsigned long int level_size;
    unsigned long int level_assoc;
   

}level_params;
// Put additional data structures here as per your requirement

#endif
