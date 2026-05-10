# minidb

A from-scratch implementation of a PostgreSQL-compatible storage engine in C++.

Built to understand — and prove — how databases work internally. Every component 
is validated against a live PostgreSQL 15 instance using pageinspect, pgstattuple, 
and txid_current_snapshot().

---

## What's been built

### Storage layer
- **Heap page layout** — 8KB pages with PageHeader and LinePointer array, 
  byte-compatible with Postgres bufpage.h. Validated via `heap_page_items()`.
- **Tuple header** — xmin/xmax/cmin/cmax union, infomask hint bits, 
  mirrors Postgres HeapTupleHeaderData exactly.
- **FilePageStore** — disk I/O via pread/pwrite/fsync. Abstracted behind 
  PageStore interface for future swap-in.

### MVCC engine
- **tuple_is_visible()** — full snapshot isolation visibility rules: 
  frozen tuples, active transactions, hint bit checks, clog integration.
- **TransactionManager** — XID allocation, begin/commit/abort, snapshot 
  capture. Thread-safe with std::atomic XID counter and std::mutex.
- **Snapshot** — xmin/xmax/active_xids/current_xid/current_cid, 
  mirrors Postgres GetSnapshotData() semantics.

### Buffer manager
- **BufferManager** — fixed-size page cache with parallel descriptor and 
  page arrays. Clock sweep eviction, dirty page writeback, pin count tracking.
- **Acceptance test** — fetch → modify → unpin dirty → evict → re-fetch → 
  data integrity verified after disk round-trip.

---

## Validation against PostgreSQL

| Component | Postgres diagnostic |
|-----------|-------------------|
| Page layout | `heap_page_items(get_raw_page(...))` |
| Free space | `pg_freespace()` |
| Tuple visibility | `txid_current_snapshot()` |
| Dead tuples | `pgstattuple()` |
| Buffer state | `pg_buffercache` (upcoming) |

---

## Build

```bash
git clone https://github.com/Saumya837/minidb
cd minidb && mkdir build && cd build
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64
make
```

## Run tests

```bash
./heap_test
./txn_test
./tuple_visiblity_test
./insert_tuple_test
./buffer_test
```

---

## Roadmap

- [x] Heap page layout
- [x] Tuple header + MVCC visibility  
- [x] FilePageStore — disk I/O
- [x] TransactionManager — thread-safe XID lifecycle
- [x] BufferManager — clock sweep, dirty writeback
- [ ] Lock manager — row-level locking, deadlock detection
- [ ] Free space map
- [ ] Visibility map
- [ ] Isolation level anomaly harness
- [ ] WAL — write-ahead logging
- [ ] Benchmark vs PostgreSQL

## Architecture

```
┌─────────────────────────────────────────┐
│           Transaction Manager           │
│   XID allocation · snapshots · mutex    │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│             Lock Manager                │
│   row-level locks · deadlock detection  │
│              (in progress)              │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│            Buffer Manager               │
│   clock sweep · pin counts · dirty      │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│          PageStore Interface            │
│    FilePageStore (pread/pwrite/fsync)   │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Tuple + Infomask Layer          │
│  xmin · xmax · cmin/cmax · hint bits    │
│       tuple_is_visible() · MVCC         │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│          Slotted Page Layout            │
│  PageHeader · LinePointers · 8KB pages  │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│                 Disk                    │
└─────────────────────────────────────────┘
```
## Status 

- Actively under Devlopment

---

Built by [@Saumya837](https://github.com/Saumya837)
