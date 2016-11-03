# FTP Client and Server

This project implements a very rudimentary client and server which can be
used to transfer files across a network. It is executed via the command line,
and can be used with Troll to simulate volatile network conditions.

## System Diagram


                           +--------+                          +--------+
                           | ftpc   |                          | ftps   |
                           |--------|                          |--------|
                           | rocket |                          | rocket |
                           +--A--+--+                          +--A--|--+
                              |  |                                |  |
                       +------|--|------+                  +------|--|------+
                       |   +--|--V--+   |                  |   +--|--V--+   |
                       |   |  tcpd  |   |                  |   |  tcpd  |   |
                       |   +----|---+   |                  |   +----|---+   |
                       |   +----V---+   |                  |   +----V---+   |
                       |   | troll  |<------------------------>| troll  |   |
                       |   +--------+   |                  |   +--------+   |
                       | CLIENT MACHINE |                  | SERVER MACHINE |
                       +----------------+                  +----------------+

## Subsystem Descriptions

The primary subsystems of this project are given their own subdirectory within
the `src` directory. These components are described in greater detail below.

### Client (ftpc)

The client is utilized by a user to transmit an input file across a network.

#### Usage

```
$ client <file_path>
```

### Server (ftps)

The server receives an input file from the client and stores it in a directory.

```
$ server
```

### TCP Daemon (tcpd)


The TCP daemon does a lot of the bulk work, adding reliability to the UDP
communication between the FTP client and server.

#### Usage

Client-side TCPD

```
$ tcpd 1
```

Server-side TCPD

```
$ tcpd 0
```

### Delta Timer (timer)

The timer is used to keep track of packet transmission information, necessary
for timeouts, RTO calculation, and more. It is run on client-side.

#### Usage

```
$ timer
```
