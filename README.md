# Redis Pipelining Benchmark

## Overview

This utility provides a high-performance, thread-safe benchmarking tool for evaluating Redis server performance under various workload conditions with a focus on pipelined operations. The implementation leverages C++11 threading primitives and the hiredis client library to simulate concurrent workloads with configurable read/write ratios and pipeline depths.

## Technical Architecture

The benchmark implements a multi-threaded client that:

1. Establishes multiple concurrent connections to a Redis instance
2. Executes parameterized workloads using Redis pipelining
3. Maintains an in-memory verification store to validate data integrity
4. Provides detailed error reporting with command-specific context

## Key Features

- **Configurable Pipeline Depth**: Optimizes Redis network utilization by batching multiple commands before requiring responses
- **Concurrent Connection Simulation**: Utilizes C++11 thread management to simulate multiple clients
- **Parameterized Workload Generation**: Configurable read/write ratios to simulate various application patterns
- **Data Consistency Verification**: Maintains local state to verify Redis responses against expected values
- **Thread-Safe Diagnostics**: Synchronized error reporting with contextual command information
- **Iteration-Based Execution**: Support for multi-phase benchmark execution

## Dependencies

- C++11 or higher compiler support
- hiredis library (Redis C client)
- Standard Template Library (STL)

## Prerequisites
```bash
sudo apt-get update
sudo apt-get install libhiredis-dev g++
```

## Compilation

```bash
g++ -std=c++11 -o dataIntegrity dataIntegrity.cpp -lhiredis -pthread
```

## Usage

```
./dataIntegrity <redis_host:port> <pipeline_depth> <num_connections> <total_requests> <write_ratio> <iterations>
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `redis_host:port` | Redis server address (e.g., "127.0.0.1:6379") |
| `pipeline_depth` | Number of commands to batch in a single pipeline (1-n) |
| `num_connections` | Number of concurrent client connections |
| `total_requests` | Total number of operations to perform |
| `write_ratio` | Proportion of write operations (0.0-1.0) |
| `iterations` | Number of benchmark phases to execute |

## Implementation Details

### Command Pipelining

The benchmark leverages Redis pipelining to reduce network round-trip latency by:
1. Queuing multiple commands with `redisAppendCommand()`
2. Flushing the TCP buffer implicitly when reading responses
3. Processing responses in order with `redisGetReply()`

### Thread Synchronization

Thread safety is maintained through:
- A global mutex (`printMutex`) for synchronized console output
- Thread-local connection contexts and in-memory stores
- Immutable thread configuration parameters

### Memory Management

The implementation manages memory through:
- RAII principles for C++ resources
- Explicit freeing of Redis replies via `freeReplyObject()`
- Connection cleanup with `redisFree()`

### Workload Generation

Each thread maintains its own workload pattern:
- Deterministic distribution of read/write operations based on configured ratio
- Unique key generation using high-resolution timestamps and random values
- Forced write operations when the in-memory store is empty

## Error Handling

The benchmark provides detailed error reporting for:
- Connection failures
- Command execution errors
- Data integrity violations (value mismatches)
- Invalid response types

## Performance Considerations

- **Optimal Pipeline Depth**: Vary pipeline depth to find the optimal value for your network conditions
- **Connection Count**: Tune the number of connections based on Redis server capacity
- **Write Ratio Impact**: Higher write ratios typically result in lower throughput
- **Memory Pressure**: Be aware that the in-memory verification store grows proportionally with unique keys

## Example

```bash
./dataIntegrity 127.0.0.1:6379 100 4 1000000 0.3 5
```

This command benchmarks a Redis server at 127.0.0.1:6379 with:
- 100 commands per pipeline
- 4 concurrent connections
- 1,000,000 total operations
- 30% write operations (70% reads)
- 5 iterations of the benchmark
