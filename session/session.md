# Session Manager

Implements PostgreSQL-style session management for minidb.

## What it does
- Each client connection gets its own process (pid) — same as PostgreSQL
- Tracks all active sessions in a shared pg_stat_activity view
- Max 10 concurrent sessions
- Each session can run one transaction at a time

## Module Structure
session/
├── include/
│   ├── session.h           -- Session struct (pid, state, client_addr, xact_start)
│   ├── session_manager.h   -- Add/remove/list sessions, shared memory
│   └── pg_stat_activity.h  -- Shared view of all active sessions
└── src/
    ├── session.cpp          -- Session create/destroy logic
    ├── session_manager.cpp  -- Shared memory operations
    └── pg_stat_activity.cpp -- Print/query all sessions

## Session States
- idle                        -- connected, no active transaction
- active                      -- query currently running
- idle in transaction         -- BEGIN open, waiting for next command
- idle in transaction aborted -- error occurred, needs ROLLBACK

## Integration
- Session calls into engine/transaction_manager on BEGIN/COMMIT/ROLLBACK
- server/ module creates a session on every new TCP connection

## Coming Next
- Lock manager integration
- Transaction state tracking per session