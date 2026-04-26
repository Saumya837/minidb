# MiniDB

MiniDB is a ground-up implementation of a database storage and execution engine, built to understand how systems like PostgreSQL work internally.

Most developers interact with databases as black boxes. This project breaks that abstraction by rebuilding core components from first principles — focusing on storage, transactions, and concurrency.

---

## Motivation

Databases are critical infrastructure, yet their internals remain opaque to most engineers.
MiniDB is an effort to deeply understand:

* How data is physically stored on disk
* How transactions ensure correctness
* How MVCC provides isolation
* How concurrency is handled internally

---

## Architecture

### Storage Layer

* Slotted page design (implemented)
* Tuple layout with MVCC metadata (implemented)
* PageStore abstraction (implemented)
* MemoryPageStore (implemented)
* FilePageStore (implemented)

### Transaction Layer

* TransactionManager (implemented)

  * XID allocation
  * begin / commit / abort
  * active transaction tracking
  * snapshot creation (`xmin`, `xmax`, active set)
  * testing in progress

### MVCC Engine

* Tuple visibility logic (`xmin`, `xmax`)
* Snapshot-based reads
* `tuple_is_visible()` implementation

---

## Validation Against PostgreSQL

MiniDB is validated against a live PostgreSQL 15 instance to ensure correctness.

Validation tools used:

* `pageinspect` (page-level inspection)
* `pgstattuple` (tuple statistics)
* `pg_stat_activity` (transaction behavior)

This ensures that internal behaviors such as page layout, tuple visibility, and transaction semantics align with real-world systems.

---

## Building

```bash
git clone https://github.com/Saumya837/minidb
cd minidb
mkdir build && cd build
cmake ..
make
```

### Run tests

```bash
./heap_test
./validator
```

---

## Roadmap

### In Progress

* TransactionManager testing
* MVCC visibility engine refinement

### Planned

* Buffer Pool (page caching, replacement)
* Write-Ahead Logging (WAL)
* Concurrency Control (locks, deadlock detection)
* Query Execution Engine (sequential scan, filtering)
* Basic Query Planner

---

## Design Philosophy

MiniDB prioritizes:

* correctness over performance
* simplicity over completeness
* learning through implementation

The goal is not to replicate PostgreSQL, but to understand the core ideas behind database systems.

---

## Status

Actively under development.
