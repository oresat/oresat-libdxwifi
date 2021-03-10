//Error Simulator Program / ORESAT
//Cameron Eng, CapStone, 03.08.2021
//Opens a file as a binary file and injects errors and packet loss
//Will return a file with corruption

//Required Libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

//Function declarations
char * flipBit(char * byte);

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
    int packetLossExponent, errorRateExponent;
    sscanf(argv[3],"%d", &errorRateExponent); //Convert from char* to int
    sscanf(argv[4],"%d", &packetLossExponent); //Convert from char* to int
    
    //Convert exponent values into actual values base ten
    double errorRate = pow(10,errorRateExponent);
    double packetLoss = pow(10,packetLossExponent);

    //target files and open them
    FILE *fileIn  = fopen(argv[1], "rb"); //input file
    FILE *fileOut = fopen(argv[2], "wb"); //output file

    //Get File Length
    fseek(fileIn, 0, SEEK_END); //Seek to EOF
    long fileLength = ftell(fileIn); //Get byte offset @ EOF
    rewind(fileIn); //Seek to Beginning

    //Buffer Handling, derived from file length
    char *buffer = (char *)malloc(fileLength * sizeof(char)); //Malloc enough in buffer for total file size 
    fread(buffer, fileLength, 1, fileIn); // Read in the entire file

    //Test Binary file read
    //display the first 64 bytes in a file and display the simular to HexEd.it
    printf("%ld\n", fileLength);
    for(int i = 0; i < 64; i++){
        if(i % 16 == 0 && i != 0){
            printf("\n%x ", buffer[i]);
        }
        else{
            printf("%x ", buffer[i]);
        }
    }

    //Seed for random num generator
    srand(time(0));
    
    //Apply error injection
    long errorLocation = 0;
    if(errorRate >= fileLength){ //if the rate of error is larger than the file size
        errorLocation = (rand() % (fileLength - 1)); //Select one bit in the range between 0 and EOF

    }

    //Apply packet loss

    //Write out to files
    fwrite (buffer , fileLength, 1,fileOut);

    //now close the files
    fclose(fileIn);
    fclose(fileOut);
    //return success state
    return 0;
} 

//Funtions

char * flipBit(char * byte){
    int byteLength = sizeof(byte) / sizeof(char*);
    int chosen = (rand() % (byteLength - 1)); //Pick a nibble to flip
    int bit = (rand()% 3); //pick which bit in a nibble to flip
    switch(byte[chosen]) {
        case '0': 
            switch(bit){
                case 0:
                    byte[chosen] = '1';
                    break;
                case 1:
                    byte[chosen] = '2';
                    break;
                case 2:
                    byte[chosen] = '4';
                    break;
                case 3:
                    byte[chosen] = '8';
                    break;
            }  
            break;
        case '1':
            switch(bit){
                case 0:
                    byte[chosen] = '0';
                    break;
                case 1:
                    byte[chosen] = '3';
                    break;
                case 2:
                    byte[chosen] = '5';
                    break;
                case 3:
                    byte[chosen] = '9';
                    break;
            }
            break;
        case '2':
            switch(bit){
                case 0:
                    byte[chosen] = '3';
                    break;
                case 1:
                    byte[chosen] = '0';
                    break;
                case 2:
                    byte[chosen] = '6';
                    break;
                case 3:
                    byte[chosen] = 'a';
                    break;
            }
            break;
        case '3':
            switch(bit){
                case 0:
                    byte[chosen] = '2';
                    break;
                case 1:
                    byte[chosen] = '1';
                    break;
                case 2:
                    byte[chosen] = '7';
                    break;
                case 3:
                    byte[chosen] = 'b';
                    break;
            }
            break;
        case '4':
            switch(bit){
                case 0:
                    byte[chosen] = '5';
                    break;
                case 1:
                    byte[chosen] = '6';
                    break;
                case 2:
                    byte[chosen] = '0';
                    break;
                case 3:
                    byte[chosen] = 'c';
                    break;
            }
            break;
        case '5':
            switch(bit){
                case 0:
                    byte[chosen] = '4';
                    break;
                case 1:
                    byte[chosen] = '7';
                    break;
                case 2:
                    byte[chosen] = '1';
                    break;
                case 3:
                    byte[chosen] = 'd';
                    break;
            }
            break;
        case '6':
            switch(bit){
                case 0:
                    byte[chosen] = '7';
                    break;
                case 1:
                    byte[chosen] = '4';
                    break;
                case 2:
                    byte[chosen] = '2';
                    break;
                case 3:
                    byte[chosen] = 'e';
                    break;
            }
            break;
        case '7':
            switch(bit){
                case 0:
                    byte[chosen] = '6';
                    break;
                case 1:
                    byte[chosen] = '5';
                    break;
                case 2:
                    byte[chosen] = '3';
                    break;
                case 3:
                    byte[chosen] = 'f';
                    break;
            }
            break;
        case '8':
            switch(bit){
                case 0:
                    byte[chosen] = '9';
                    break;
                case 1:
                    byte[chosen] = 'a';
                    break;
                case 2:
                    byte[chosen] = 'c';
                    break;
                case 3:
                    byte[chosen] = '0';
                    break;
            }
            break;
        case '9':
            switch(bit){
                case 0:
                    byte[chosen] = '8';
                    break;
                case 1:
                    byte[chosen] = 'b';
                    break;
                case 2:
                    byte[chosen] = 'd';
                    break;
                case 3:
                    byte[chosen] = '1';
                    break;
            }
            break;
        case 'a':
            switch(bit){
                case 0:
                    byte[chosen] = 'b';
                    break;
                case 1:
                    byte[chosen] = '8';
                    break;
                case 2:
                    byte[chosen] = 'e';
                    break;
                case 3:
                    byte[chosen] = '2';
                    break;
            }
            break;
        case 'b':
            switch(bit){
                case 0:
                    byte[chosen] = 'a';
                    break;
                case 1:
                    byte[chosen] = '9';
                    break;
                case 2:
                    byte[chosen] = 'f';
                    break;
                case 3:
                    byte[chosen] = '3';
                    break;
            }
            break;
        case 'c':
            switch(bit){
                case 0:
                    byte[chosen] = '9';
                    break;
                case 1:
                    byte[chosen] = 'a';
                    break;
                case 2:
                    byte[chosen] = 'c';
                    break;
                case 3:
                    byte[chosen] = '0';
                    break;
            }
            break;
        case 'd':
            switch(bit){
                case 0:
                    byte[chosen] = 'c';
                    break;
                case 1:
                    byte[chosen] = 'f';
                    break;
                case 2:
                    byte[chosen] = 'b';
                    break;
                case 3:
                    byte[chosen] = '5';
                    break;
            }
            break;
        case 'e':
            switch(bit){
                case 0:
                    byte[chosen] = 'f';
                    break;
                case 1:
                    byte[chosen] = 'c';
                    break;
                case 2:
                    byte[chosen] = 'a';
                    break;
                case 3:
                    byte[chosen] = '6';
                    break;
            }
            break;
        case 'f':
            switch(bit){
                case 0:
                    byte[chosen] = 'e';
                    break;
                case 1:
                    byte[chosen] = 'd';
                    break;
                case 2:
                    byte[chosen] = 'b';
                    break;
                case 3:
                    byte[chosen] = '7';
                    break;
            }
            break;
    }
    return byte;
}
