# Server

Implements PostgreSQL-style TCP server for minidb.

## What it does
- Listens on a port for incoming client connections
- Performs TCP handshake for every new connection
- Forks a new OS process per connection — same as PostgreSQL postmaster
- Each forked process gets a pid which becomes the session id
- Max 10 concurrent connections

## Module Structure
server/
├── include/
│   └── server.h       -- Server struct, start/stop, accept connection
└── src/
    └── server.cpp     -- TCP socket, bind, listen, accept, fork

## Connection Flow
Client connects
    ↓
TCP handshake (SYN, SYN-ACK, ACK)
    ↓
server accepts connection
    ↓
fork() new backend process
    ↓
pid assigned → session created in session_manager
    ↓
ReadyForQuery → client can now send SQL

## Integration
- server/ calls session_manager on every new connection
- server/ calls session_manager on disconnect to clean up
- pid from fork() is passed directly to session as session id

## Coming Next
- Authentication (username, password)
- SSL support
- Connection pooling layer