//Error Simulator Program / ORESAT
//Cameron Eng, CapStone, 03.08.2021
//Opens a file as a binary file and injects errors and packet loss
//Will return a file with corruption

//Required Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//Main
int main(int argc, const char * argv[]){

    //Open files
    //Arg Handleing
    //Exit if given too little args
    if(argc < 5){
        puts("Not enough args\n");
        puts("args: (1) File In, (2) File Out, (3) Error Rate (4) Packet Loss Rate");
        exit(1);
    }

    //Check if files exist & can be modified correctly (0 = Success)
    if(access (argv[1], R_OK) == 0){
        puts("Read OK, First File\n");
    }
    else{
        puts("Read NOT POSSIBLE, First File\n");
        exit(1);
    }

    if(access (argv[2],W_OK) == 0){
        puts("Write OK, Second File\n");
    }
    else{
        puts("Write NOT POSSIBLE, Second File\n");
        exit(1);
    }

    //Get int values for Packet Loss and Error Rate
    int packetLoss, errorRate;
    sscanf(argv[3],"%d", &errorRate); //Convert from char* to int
    sscanf(argv[4],"%d", &packetLoss); //Convert from char* to int

    //target files and open them
    FILE *fileIn  = fopen(argv[1], "rb"); //input file
    FILE *fileOut = fopen(argv[2], "w"); //output file

    //Get File Length
    fseek(fileIn, 0, SEEK_END); //Seek to EOF
    long fileLength = ftell(fileIn); //Get byte offset @ EOF
    rewind(fileIn); //Seek to Beginning

    //Buffer Handling, derived from file length
    char *buffer = (char *)malloc(fileLength * sizeof(char)); //Malloc enough in buffer for total file size 
    fread(buffer, fileLength, 1, fileIn); // Read in the entire file

    //Apply packet loss

    //Apply error injection

    //Write out to files
    fwrite (buffer , sizeof(char), sizeof(buffer), fileOut);

    //now close the files
    fclose(fileIn);
    fclose(fileOut);
    //return success state
    return 0;
} 