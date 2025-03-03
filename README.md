# dataIntegrityPipelineCpp

Data Integrity Tester for Redis
This project provides a C++ application that tests data integrity for a single Redis server. The application issues pipelined SET and GET commands using multiple threads and maintains an in-memory key–value store. It verifies that data stored on Redis matches the expected value stored in memory.

Features
Pipelining: Sends batches of commands to Redis to improve performance.
Data Integrity Checks: For each GET command, compares the Redis response with the expected value.
Multi-threading: Uses multiple threads (connections) to simulate concurrent load.
Detailed Debug Output: On any failure (invalid reply, data mismatch, etc.), prints both the Redis response and the expected value.
Iteration Progress: Prints the iteration counter to stdout for easy progress tracking.
Prerequisites
Ubuntu Focal OS (or any similar Linux distribution)
hiredis library: Install it with:
bash
Copy
sudo apt-get update
sudo apt-get install libhiredis-dev
C++11 Compiler: e.g., g++.
Compilation
Save the source code into a file (e.g., dataIntegrity.cpp). Then compile using g++ with C++11 support and link against hiredis and pthread libraries:

bash
Copy
g++ -std=c++11 dataIntegrity.cpp -o dataIntegrity -lhiredis -pthread
Usage
Run the compiled binary with the following parameters:

bash
Copy
./dataIntegrity <redis_host:port> <pipeline_depth> <num_connections> <total_requests> <write_ratio> <iterations>
Parameters:

<redis_host:port>: Redis server address (e.g., 127.0.0.1:6379).
<pipeline_depth>: Number of commands to send in one pipelined batch.
<num_connections>: Number of worker threads (each with its own Redis connection).
<total_requests>: Total number of requests to process.
<write_ratio>: Fraction of requests that should be writes (e.g., 0.5 for 50% writes).
<iterations>: Number of iterations for the test. The iteration counter is printed to stdout.
Example:

bash
Copy
./dataIntegrity 127.0.0.1:6379 10 2 1000 0.5 5
License
This project is provided as-is without any warranty. Modify and use it as needed for your testing or development purposes.

