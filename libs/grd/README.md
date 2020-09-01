# Overview
Library: grd 
Purpose: Distributed computing library.

# Features

- SMP processing using several gate types
- work queue
- persistent queue (stored in DB)
- job processing (restartable, distributed tasks)
- Python RPC
- remote SQLite SQL execution
- process alive status monitoring
- Win32 service support (registration, execution)

Gate types:
- Boost message queue
- ZeroMQ
- wx client/server
- HTTP 
