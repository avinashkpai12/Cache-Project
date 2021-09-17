#include <stdio.h>
#include <stdlib.h>
#include "sim_cache.h"
#include <string>
#include<iostream>
#include<bits/stdc++.h>
#include<math.h>

using namespace std;

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim_cache 32 8192 4 7 262144 8 gcc_trace.txt
    argc = 8
    argv[0] = "sim_cache"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/


struct Level_config{
    unsigned long int numBlocks;
    unsigned long int numSets;
    unsigned long int numIndex;
    unsigned long int numOfTagBits;
    unsigned long int blockOffset;
    unsigned long int readHits=0,readMisses=0,writeHits=0,writeMisses=0;
    unsigned long int numReads=0,numWrites=0;
    unsigned long int swaps=0,swapRequests=0;
    unsigned long int L1_WBMem=0,memTraffic=0,L2_WBMem=0;
};

struct addr_attrib
{
    unsigned long int tag;
    unsigned long int index;
};

struct arr_cell_val
{
    int valid_bit;
    unsigned long int RU;
    unsigned long int tagValue;
    char dirtyBit;
};

class Level{

    public :
        Level_config l_config;
        cache_params cache_par;
        addr_attrib attrib_val; //Attrib value from the address after parsing from the gcc_trace
        arr_cell_val** arrTagVal; //Array for tag

        //to create cache
        Level createCache(Level &l,unsigned  int numberOfSets, unsigned  int associativity){

           unsigned long int i, j;

            l.arrTagVal = new arr_cell_val*[numberOfSets];
            for(i=0;i<numberOfSets;i++){ //traversing the sets

                l.arrTagVal[i] = new arr_cell_val[associativity]; //creating block for that set index

                for(j=0;j<associativity;j++){

                    l.arrTagVal[i][j].valid_bit = 0;
                    l.arrTagVal[i][j].RU = j;
                    l.arrTagVal[i][j].dirtyBit = 0;
                    l.arrTagVal[i][j].tagValue = '\0';

                }

            }

            return l;

        }

        //calculating tag and index
        addr_attrib calculateParams(Level &l,unsigned long int addr, unsigned long int levelSize,
                                     unsigned long int assoc, unsigned long int blockSize ){

            
            
            l.l_config.numSets = levelSize/(assoc * blockSize); //Calculting number of Sets
            l.l_config.numIndex = log2(l.l_config.numSets);
            l.l_config.blockOffset = log2(blockSize);
            l.l_config.numOfTagBits = 32 - l.l_config.numIndex - l.l_config.blockOffset;
            unsigned long int tempTagIndex = addr>>l.l_config.blockOffset;
            unsigned long int temp = tempTagIndex;
            l.attrib_val.tag = tempTagIndex>>l.l_config.numIndex;
            //attrib_val.index = temp<<l_config.numOfTagBits;
            l.attrib_val.index = (addr>>l.l_config.blockOffset) & ((1<<l.l_config.numIndex)-1); //calculating index
            l.attrib_val.tag = (addr>>(l.l_config.blockOffset + l.l_config.numIndex)) & ((1<<l.l_config.numOfTagBits)-1); //calculating tag bits
            
            //0001 0000 0000 0000 1010 1|001 010|
            // |     |     |     |     |    0001 0101  
            //0010 0100 0000 0000 0000 0000 0000
            //
            /*
            printf("number Index bits : %d \n", l_config.numIndex);
            printf("number Offset bits : %d \n", l_config.blockOffset);
            printf("number Tag bits : %d \n", l_config.numOfTagBits);
            printf("Temp : %lx \n", tempTagIndex);
            printf("Index : %lx \n", attrib_val.index);
            printf("Tag : %lx \n", attrib_val.tag);
            */
            //cout<<"Address size : "<<addr<<"\n";

            return attrib_val;
             
        }

        void addTagValToLatestEmptySet(Level &l1, unsigned long int l1_assoc,  addr_attrib l1_attrib){
            for(int j=0;j<l1_assoc;j++){
                if(!l1.arrTagVal[l1_attrib.index][j].valid_bit){
                    l1.arrTagVal[l1_attrib.index][j].valid_bit = 1;//change valid to 1
                    l1.arrTagVal[l1_attrib.index][j].tagValue = l1_attrib.tag; //add the tag to latest
                    break;
                }
                else
                    continue;

            }
        }

        //compute the RU value each time regardless of hit or miss
        void computeNewLRU(Level &l, unsigned long int RU, unsigned long int assoc, unsigned long int index){
            
            unsigned long int j;
            for(int j=0;j<assoc;j++){

                if(l.arrTagVal[index][j].RU == RU) //change the RU value for the mached assoc to 0
                    l.arrTagVal[index][j].RU = 0;
                else if(l.arrTagVal[index][j].RU < RU) //increment the RU's by 1
                    l.arrTagVal[index][j].RU+=1;
            }

        }

        bool searchLevelForTagMatch(Level &l, unsigned long int l_assoc, addr_attrib l_attrib, char rw){


            unsigned long int j, assocRU = 0;

            for(j=0;j<l_assoc;j++){
                
                if(l.arrTagVal[l_attrib.index][j].valid_bit && (l.arrTagVal[l_attrib.index][j].tagValue == l_attrib.tag))//check for valid=0 
                {
                    if(rw == 'r'){
                        l.l_config.readHits+=1;
                        l.l_config.numReads+=1;   
                    }
                    else
                    {
                        // cout<<"Writing bit";
                        l.l_config.writeHits+=1;
                        l.l_config.numWrites+=1;
                        l.arrTagVal[l_attrib.index][j].dirtyBit = 'D';
                    }

                    assocRU = l.arrTagVal[l_attrib.index][j].RU;
                    computeNewLRU(l, assocRU, l_assoc, l_attrib.index);

                    return 1;
                }
            }

            return 0;


        }


        bool checkForValidBitZero(Level &l, unsigned long int assoc, addr_attrib l_attrib, char rw ){
            
            unsigned long int j, assocRU= 0;
            for(j=0;j<assoc;j++){
                

               // cout<<"Valid bit  : "<<l.arrTagVal[l_attrib.index][j].valid_bit;
                //if valid bit 0 put the tag that came in 
                if(l.arrTagVal[l_attrib.index][j].valid_bit == 0){
                    
                    //cout<<"Valid bit count : "<<j;

                    if(rw == 'r'){
                        l.l_config.numReads+=1;
                        l.l_config.readMisses+=1;
                    }
                    else{
                        // cout<<"Writing bit";
                        l.l_config.numWrites+=1;
                        l.l_config.writeMisses+=1;
                        l.arrTagVal[l_attrib.index][j].dirtyBit = 'D'; 
                        

                    }

                    l.l_config.memTraffic+=1;
                    l.arrTagVal[l_attrib.index][j].tagValue = l_attrib.tag; //putting the tag in that cell
                    l.arrTagVal[l_attrib.index][j].valid_bit = 1; //change the valid bit since putting data
                    assocRU = l.arrTagVal[l_attrib.index][j].RU;
                    // cout<<"RU Val :"<<assocRU<<"\n";
                    computeNewLRU(l, assocRU, assoc, l_attrib.index);
                    // cout<<"RU Val after compute:"<<l.arrTagVal[l_attrib.index][j].RU<<"\n";
                    return 1;
                    //
                }

            }

            return 0;
        }

        void replaceTagVal(Level &l, unsigned long int assoc,addr_attrib l_attrib, char rw){



            unsigned long int evicted_block,evicted_block_index;
            unsigned long int j,assocRU;
            char evicted_block_dirty;
            for(j=0;j<assoc;j++)
            {
                // if(l.arrTagVal[l_attrib.index][j].tagValue!=l_attrib.tag 
                //   &&l.arrTagVal[l_attrib.index][j].valid_bit==1 
                //   && l.arrTagVal[l_attrib.index][j].RU==assoc-1)
                // cout<<"Associativity :"<<assoc<<"\n";
                if(l.arrTagVal[l_attrib.index][j].RU==(assoc-1)) //(assoc-1 == 1)
                {

                    // cout<<"replacing : "<<j<<"\n";
                    // cout<<"Assoc RU val : "<<l.arrTagVal[l_attrib.index][j].RU;
                    evicted_block=l.arrTagVal[l_attrib.index][j].tagValue;
                    evicted_block_index=l_attrib.index;
                    evicted_block_dirty=l.arrTagVal[l_attrib.index][j].dirtyBit;
                    
                    //chceking for a dirty block
                    if(evicted_block_dirty=='D') 
                    {
                        l.l_config.memTraffic+=1;
                        l.l_config.L2_WBMem+=1;
                    }
                    
                    //for read request
                    if(rw=='r')
                    {
                        l.l_config.readMisses+=1;
                        l.l_config.numReads+=1;
                        l.arrTagVal[l_attrib.index][j].dirtyBit='\0';
                    }
                    else // for write request
                    {   
                        l.l_config.writeMisses+=1;
                        l.l_config.numWrites+=1;
                        l.arrTagVal[l_attrib.index][j].dirtyBit='D';
                    }
                    
                    l.arrTagVal[l_attrib.index][j].tagValue= l_attrib.tag;
                    l.l_config.memTraffic+=1;
                    assocRU=l.arrTagVal[l_attrib.index][j].RU;
                    computeNewLRU(l, assocRU,assoc, l_attrib.index);
                    break;
                }
            }


        }

        
    //performing read operation on L1
        void updateLevel(Level &l1, Level &l2, unsigned long int l1_assoc, addr_attrib l1_attrib,
        unsigned l2_assoc, addr_attrib l2_attrib, char rw){
            
            bool matchL2=0;

            if(rw == 'r'){

                bool matchL1 = l1.searchLevelForTagMatch(l1, l1_assoc, l1_attrib, rw);

               

                if(!matchL1){
                    // cout<< "No Match Tag :"<<hex<<l1_attrib.tag<<"\n";

                     //reflect the L2 to L1
                    bool validBitStat1 = l1.checkForValidBitZero(l1, l1_assoc, l1_attrib, rw);

                    if(validBitStat1 == 0){ //we replace the existing tag value
                        // cout<< "replacing existing tag with : "<<hex<<l1_attrib.tag<<"\n";
                        // cout<<"L1 RU 0 :"<<l1.arrTagVal[l1_attrib.index][0].RU<<"\n";
                        // cout<<"L1 RU 1 :"<<l1.arrTagVal[l1_attrib.index][1].RU<<"\n";
                        replaceTagVal(l1, l1_assoc, l1_attrib, rw); //evicting when miss  based on LRU 

                    }
                
                    ///go to L2 if size of L2 not 0
                    if(l2.cache_par.l2_size !=0){
                        matchL2 = l2.searchLevelForTagMatch(l2, l2_assoc, l2_attrib, rw);

                        if(!matchL2){

                            //check for the valid bit
                            bool validBitStat2 = l2.checkForValidBitZero(l2, l2_assoc, l2_attrib, rw);

                            //valid bit zero and tag has been added to the level assoc
                            if(validBitStat2 == 0){
                                replaceTagVal(l2, l2_assoc, l2_attrib, rw); //evicting when miss based on LRU 

                                //check for dirty bit

                            
                            }
                        }



                    }
                    

                }
            }

            else if(rw == 'w'){

                // cout<<"Writing!!";
                bool matchL1 = l1.searchLevelForTagMatch(l1, l1_assoc, l1_attrib, rw);

                if(!matchL1){

                    bool validBitStat1 = l1.checkForValidBitZero(l1, l1_assoc, l1_attrib, rw);
                    if(validBitStat1 == 0){ //we replace the existing tag value
                        // cout<< "replacing existing tag with : "<<hex<<l1_attrib.tag<<"\n";
                        // cout<<"L1 RU 0 :"<<l1.arrTagVal[l1_attrib.index][0].RU<<"\n";
                        // cout<<"L1 RU 1 :"<<l1.arrTagVal[l1_attrib.index][1].RU<<"\n";
                        replaceTagVal(l1, l1_assoc, l1_attrib, rw); //evicting when miss  based on LRU 

                    }
                 ///go to L2 if size of L2 not 0
                    if(l2.cache_par.l2_size !=0){
                        rw = 'r'; //write miss to L1 will be read request L2
                        matchL2 = l2.searchLevelForTagMatch(l2, l2_assoc, l2_attrib, rw);


                        if(!matchL2){

                        //check for the valid bit
                        bool validBitStat2 = l2.checkForValidBitZero(l2, l2_assoc, l2_attrib, rw);

                        //valid bit zero and tag has been added to the level assoc
                        if(validBitStat2 == 0){
                                replaceTagVal(l2, l2_assoc, l2_attrib, rw); //evicting when miss based on LRU 


                            //check bor dirty bit

                        
                        }
                    }
                }


            }
        }
            
        //     for(int j=0;j<l1_assoc;j++){
            
                    
        //         if(!l1.arrTagVal[l1_attrib.index][j].valid_bit)//check for valid=0 
        //         {
        //             if(l1.arrTagVal[l1_attrib.index][j].RU == (l1_assoc-1)){//Check has reached the last block

        //                 //add the tag value latest empty L1 set    
        //                 addTagValToLatestEmptySet(l1, l1_assoc, l1_attrib);
        //                 cout<<"Finished L1 mooving on to L2";//go to L2
        //             }
        //             else             
        //                 continue; //continue checking for next associativity

        //         }
        //         else{

        //             if(l1.arrTagVal[l1_attrib.index][j].tagValue != l1_attrib.tag){
                        
        //                 if(l1.arrTagVal[l1_attrib.index][j].RU == (l1_assoc-1)){

        //                 }
        //                 else
        //                     continue; //check for tag in the next associativity
        //             }
        //             else{

        //             }
        //         }         
        //         if(!l1.arrTagVal[l1_attrib.index][j].valid_bit || 
        //             (l1.arrTagVal[l1_attrib.index][j].valid_bit && (l1.arrTagVal[l1_attrib.index][j].tagValue != l1_attrib.tag))) { //check for valid_bit=1 && !tag
        //             l_config.readMisses++; //incrementing read misses
                
                
        //             //readL2(l2, l2_assoc, l2_attrib);    // since miss go to L2 then place tag1 in L1
                


        //             //updating L1 as well
        //             l1.arrTagVal[l1_attrib.index][j].valid_bit = 1;
        //             l1.arrTagVal[l1_attrib.index][j].tagValue = l1_attrib.tag;

        //         }
        //     }        
            
        // }

        // void readL2(Level &l2, unsigned long int l2_assoc, addr_attrib l2_attrib){


        //     for(int j=0;j<l2_assoc;j++){
        //         //trying to read L2 for tag
        //         cout<<"in read2 for";



        //         if(!arrTagVal[l2_attrib.index][j].valid_bit || 
        //             (arrTagVal[l2_attrib.index][j].valid_bit && (arrTagVal[l2_attrib.index][j].tagValue != l2_attrib.tag))) 
        //             { //check for valid_bit=1 && !tag

        //                 cout<<"inside if read2";
        //                 l_config.readMisses++;
        //                 l_config.memTraffic++;
        //                 arrTagVal[l2_attrib.index][j].valid_bit = 1;
        //                 //storing tag in L2 
        //                 arrTagVal[l2_attrib.index][j].tagValue = l2_attrib.tag; 

        //             }
        //     }

     }

    
};



int main(int argc, char* argv[]){

    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    cache_params params;    // look at sim_cache.h header file for the the definition of struct cache_params
    char rw;                // variable holds read/write type read from input file. The array size is 2 because it holds 'r' or 'w' and '\0'. Make sure to adapt in future projects
    unsigned long int addr; // Variable holds the address read from input file
    int u = 0;
    addr_attrib addr_result1; //resulting tag and index from addr for L1 form gcc_trace file
    addr_attrib addr_result2; //resulting tag and index from addr for L2 form gcc_trace file

    Level l1, l2, VC;


    if(argc != 8)           // Checks if correct number of inputs have been given. Throw error and exit if wrong
    {
        printf("Error: Expected inputs:7 Given inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    // strtoul() converts char* to unsigned long. It is included in <stdlib.h>
    params.block_size       = strtoul(argv[1], NULL, 10);
    params.l1_size          = strtoul(argv[2], NULL, 10);
    params.l1_assoc         = strtoul(argv[3], NULL, 10);
    params.vc_num_blocks    = strtoul(argv[4], NULL, 10);
    params.l2_size          = strtoul(argv[5], NULL, 10);
    params.l2_assoc         = strtoul(argv[6], NULL, 10);
    trace_file              = argv[7];

    unsigned long int i = 0;

    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }
    
    // Print params
    printf("  ===== Simulator configuration =====\n"
            "  BLOCKSIZE:                        %lu\n"
            "  L1_SIZE:                          %lu\n"
            "  L1_ASSOC:                         %lu\n"
            "  VC_NUM_BLOCKS:                    %lu\n"
            "  L2_SIZE:                          %lu\n"
            "  L2_ASSOC:                         %lu\n"
            "  trace_file:                       %s\n", params.block_size, params.l1_size, params.l1_assoc, params.vc_num_blocks, params.l2_size, params.l2_assoc, trace_file);
	cout<<endl;
    char str[2];

    unsigned long int numSets1= params.l1_size/(params.l1_assoc*params.block_size);

   l1 =  l1.createCache(l1, numSets1, params.l1_assoc);

    if(params.l2_size!=0){
        cout<<"l2 params not 0";
        unsigned long int numSets2= params.l2_size/(params.l2_assoc*params.block_size);
      l2 =  l2.createCache(l2, numSets2, params.l2_assoc);
    }   
    
    while(fscanf(FP, "%s %lx", str, &addr) != EOF)
    {
        //cout <<"In while";
         rw = str[0];
        //if (rw == 'r')
         //   printf("%s %lx\n", "read", addr);           // Print and test if file is read correctly
        //else if (rw == 'w')
         //   printf("%s %lx\n", "write", addr);          // Print and test if file is read correctly
        /*************************************
                  Add cache code here
        **************************************/

       addr_result1 = l1.calculateParams(l1, addr, params.l1_size, params.l1_assoc, params.block_size );// calculating L1 params
    //    printf("Tag : %lx \n", addr_result1.tag);
    //    printf("Index : %lx \n", addr_result1.index);



      if(params.l2_size != 0){ //check for when L2 size not zero
            addr_result2 = l2.calculateParams(l2, addr, params.l2_size, params.l2_assoc, params.block_size); 
            // printf("Tag l2 : %lx \n", addr_result2.tag);
            // printf("Index l2: %lx \n", addr_result2.index);
       }
        
            
        l1.updateLevel(l1, l2, params.l1_assoc, addr_result1, params.l2_assoc, addr_result2, rw);
           
        

    }

    /*************Check L1 Contents*****************/
    cout<<"Calling L1"<<"\n";
    for(i=0;i<l1.l_config.numSets;i++){
        
        cout<<"  set    "<<dec<<i<<":      ";
        for(unsigned int j=0;j<params.l1_assoc;j++){

           cout<<hex<<l1.arrTagVal[i][j].tagValue;
        //    cout<<" Valit bit :"<<l1.arrTagVal[i][j].valid_bit;
			if(l1.arrTagVal[i][j].dirtyBit == 'D')
				cout<<"D"<<"  ";
			else
				cout<<" "<<"  ";
        }
        cout<<"\n";
        
    }
    /*************Check L1 Contents*****************/

    /*************Check L2 Contents*****************/
    
    cout<<"Calling L2"<<"\n";
    for(i=0;i<l2.l_config.numSets;i++){
        
        cout<<"  set    "<<dec<<i<<":      ";
        for(unsigned int j=0;j<params.l2_assoc;j++){

           cout<<hex<<l2.arrTagVal[i][j].tagValue;
        //    cout<<" Valit bit :"<<l2.arrTagVal[i][j].valid_bit;
			if(l2.arrTagVal[i][j].dirtyBit == 'D')
				cout<<"D"<<"  ";
			else
				cout<<" "<<"  ";
        }
        cout<<"\n";
        
    }
    /*************Check L2 Contents*****************/


     // to print simulaiton results
    cout<<"===== Simulation Results ====="<<endl;
    cout<<"  a.  number of L1 hits:  "<<dec<<l1.l_config.readHits<<endl;
    cout<<"  a.  number of L2 hits:  "<<dec<<l2.l_config.readHits<<endl;
    
}