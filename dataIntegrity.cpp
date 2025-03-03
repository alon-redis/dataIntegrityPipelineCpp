#include <hiredis/hiredis.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <algorithm>
#include <utility>
#include <string>

using namespace std;
using namespace std::chrono;

// Global mutex to synchronize printing to stdout.
mutex printMutex;

// Utility: Generate a unique key using a timestamp and a random number.
string generateUniqueKey() {
    auto now = high_resolution_clock::now();
    auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
    stringstream ss;
    ss << "key_" << now_ms << "_" << rand();
    return ss.str();
}

// Structure to record each command in a pipelined batch.
struct BatchCommand {
    bool isWrite;         // true if command is a write; false if read.
    string key;           // the key used for the command.
    string expectedValue; // for read commands: the expected value.
};

// Helper function to connect to a Redis server.
redisContext* connectSingleRedis(const string &hostport) {
    size_t colonPos = hostport.find(":");
    string host = hostport.substr(0, colonPos);
    int port = stoi(hostport.substr(colonPos + 1));
    redisContext* ctx = redisConnect(host.c_str(), port);
    if (!ctx || ctx->err) {
        cerr << "Connection error (" << hostport << "): " 
             << (ctx ? ctx->errstr : "Cannot allocate redis context") << endl;
        exit(1);
    }
    return ctx;
}

// Worker function that performs pipelined requests and maintains an in-memory key–value store.
void redisWorker(const string &hostport, int totalRequests, int pipelineDepth, double writeRatio, int iterations) {
    // Connect to the single Redis server.
    redisContext* ctx = connectSingleRedis(hostport);
    
    // In-memory store of key–value pairs.
    vector<pair<string, string>> inMemoryData;
    
    int requestsPerIteration = totalRequests / iterations;
    
    for (int iter = 0; iter < iterations; ++iter) {
        {
            // Synchronized printing of the iteration counter.
            lock_guard<mutex> lock(printMutex);
            cout << "Iteration: " << iter + 1 << endl;
        }
        
        for (int i = 0; i < requestsPerIteration; i += pipelineDepth) {
            int currentBatch = min(pipelineDepth, requestsPerIteration - i);
            // Record details of each queued command.
            vector<BatchCommand> batchCommands;
            batchCommands.reserve(currentBatch);
            
            // Queue commands in pipelined mode.
            for (int j = 0; j < currentBatch; ++j) {
                double rnd = static_cast<double>(rand()) / RAND_MAX;
                // Force a write if there is no key in memory.
                bool isWrite = (rnd < writeRatio) || inMemoryData.empty();
                
                if (isWrite) {
                    // Generate a unique key and corresponding value.
                    string key = generateUniqueKey();
                    string value = "value_" + key;
                    // Save the pair in the in-memory store.
                    inMemoryData.push_back({key, value});
                    // Append the SET command.
                    redisAppendCommand(ctx, "SET %s %s", key.c_str(), value.c_str());
                    
                    BatchCommand cmd;
                    cmd.isWrite = true;
                    cmd.key = key;
                    batchCommands.push_back(cmd);
                } else {
                    // For read commands, choose a random key from the in-memory store.
                    int index = rand() % inMemoryData.size();
                    string key = inMemoryData[index].first;
                    redisAppendCommand(ctx, "GET %s", key.c_str());
                    
                    BatchCommand cmd;
                    cmd.isWrite = false;
                    cmd.key = key;
                    cmd.expectedValue = inMemoryData[index].second;
                    batchCommands.push_back(cmd);
                }
            }
            
            // Flush the pipeline and process responses.
            for (int j = 0; j < currentBatch; ++j) {
                redisReply* reply;
                if (redisGetReply(ctx, (void**)&reply) != REDIS_OK) {
                    lock_guard<mutex> lock(printMutex);
                    cerr << "Error retrieving reply from Redis server." << endl;
                    continue;
                }
                
                BatchCommand &cmd = batchCommands[j];
                if (cmd.isWrite) {
                    // For SET commands, we expect a status reply "OK".
                    if (reply->type == REDIS_REPLY_STATUS) {
                        if (string(reply->str) != "OK") {
                            lock_guard<mutex> lock(printMutex);
                            cerr << "Invalid reply for SET command for key: " << cmd.key 
                                 << " | Expected: OK"
                                 << " | Redis response: " << reply->str
                                 << " [Detailed: reply type " << reply->type << "]" << endl;
                        }
                    } else {
                        // Not a status reply.
                        string redisResp = (reply->type == REDIS_REPLY_ERROR && reply->str) ? reply->str : to_string(reply->type);
                        lock_guard<mutex> lock(printMutex);
                        cerr << "Invalid reply type for SET command for key: " << cmd.key 
                             << " | Expected: OK"
                             << " | Redis response: " << redisResp
                             << " [Detailed: reply type " << reply->type << "]" << endl;
                    }
                } else {
                    // For GET commands, we expect a string reply matching the expected value.
                    if (reply->type == REDIS_REPLY_STRING) {
                        string returnedValue(reply->str);
                        if (returnedValue != cmd.expectedValue) {
                            lock_guard<mutex> lock(printMutex);
                            cerr << "Data mismatch for key: " << cmd.key 
                                 << " | Expected: " << cmd.expectedValue
                                 << " | Redis response: " << returnedValue
                                 << " [Detailed: reply type " << reply->type << "]" << endl;
                        }
                    } else if (reply->type == REDIS_REPLY_NIL) {
                        lock_guard<mutex> lock(printMutex);
                        cerr << "Data missing for key: " << cmd.key 
                             << " | Expected: " << cmd.expectedValue
                             << " | Redis response: NIL"
                             << " [Detailed: reply type " << reply->type << "]" << endl;
                    } else {
                        string redisResp = (reply->str) ? reply->str : to_string(reply->type);
                        lock_guard<mutex> lock(printMutex);
                        cerr << "Invalid reply type for GET command for key: " << cmd.key 
                             << " | Expected: " << cmd.expectedValue
                             << " | Redis response: " << redisResp
                             << " [Detailed: reply type " << reply->type << "]" << endl;
                    }
                }
                freeReplyObject(reply);
            }
        }
    }
    redisFree(ctx);
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        cerr << "Usage: " << argv[0] 
             << " <redis_host:port> <pipeline_depth> <num_connections> <total_requests> <write_ratio> <iterations>" 
             << endl;
        return 1;
    }
    
    string redisHostPort = argv[1];
    int pipelineDepth = atoi(argv[2]);
    int numConnections = atoi(argv[3]);
    int totalRequests = atoi(argv[4]);
    double writeRatio = atof(argv[5]); // e.g. 0.5 for 50% writes.
    int iterations = atoi(argv[6]);
    
    // Seed random number generator.
    srand(static_cast<unsigned>(time(nullptr)));
    
    // Launch worker threads, each with its own Redis connection.
    vector<thread> workers;
    // Divide total requests among threads.
    int requestsPerThread = totalRequests / numConnections;
    
    for (int i = 0; i < numConnections; i++) {
        workers.push_back(thread(redisWorker, redisHostPort, requestsPerThread, pipelineDepth, writeRatio, iterations));
    }
    
    // Wait for all threads to finish.
    for (auto& worker : workers) {
        worker.join();
    }
    
    {
        lock_guard<mutex> lock(printMutex);
        cout << "All requests processed." << endl;
    }
    return 0;
}
