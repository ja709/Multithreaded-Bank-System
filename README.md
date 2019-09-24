# Multithreaded-Bank-System

## Introduction
This is a server program that emulates a bank and a client program to communicate with it. The server program supports multiple simultaneous client TCP/IP connections. This requires multithreading. Multiple clients accessing the same account database at the same time will utilizes the use of mutexes to protect all shared data. The server globally locks the account data at regular intervals to print diagnostic information to its STDOUT.
The client side connects to the server then reads in user commands from STDIN. The client and server send messages back and forth exchanging request and resposes. The client side checks the commands the user is entering to make sure they are correctly formed and formatted.


## Client and Server functionalities
The command syntax allows the user to; create accounts, start sessions to serve specific accounts,and to exit the client process altogether: 
* create <accountname (char) >
* serve <accountname (char) >
* deposit <amount (double) >
* withdraw <amount (double) >
* query
* end
* quit


## Running the tests
Local test does not work. (Only remote)

```
./bankingServer <name of the machine/address> <port number>
```
```
./bankingServer <port number>
```


## Built by
* [Hongju Shin]
* [Jeeho Ahn]

rutgers cs214 project
