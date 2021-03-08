# error-simulator

### Summary
This error simulator is designed to injected but flips and packet loss into a data file and return a new file with currupted information. The error rate and packet loss are defined in the argument list for the program.


### Cmake Instructions
ToDo

### File Information
ToDo

### Error Injection Summary
Error injection is defined by the error rate given from the command line. The command line argument is the negative factor of 10. See CMake instuctions for example. The error injection is preformed by iterating though the data in blocks defined by the rate. In each block one bit is randomly selected to be flipped.

### Packet Loss Summary
Packet loss is defined by the rate given from the command line. The command line argument is the negative factor of 10. See CMake instuctions for example. The packet loss will be preformed by ommiting packets from the write out process.
