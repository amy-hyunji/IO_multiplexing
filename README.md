## 1. Introduction
Design and implement a web server which is not only running with multi-threads but also uses I/O multiplexing to handle concurrent requests from clients. Your ultimate goal is to build a high-performance web server that can process about 100,000 requests per second. 

### (1) an event-based server with I/O multiplexing to handle requests, and 
Bootstrapping: Your server should make inverted index of words in all documents in the folder. This should be done when the server is started, and the server should store the generated indices in the memory. 
Searching: Your server program should be able to handle large number of request with multi-thread, and it should response relevant documents and lines by the queried words.

### (2) 2 client programs that send multiple requests to a Web server. 
multi-thread text search server to handle large number of search requests from multiple clients. The implementation may contain two main parts.

## 2. Server
### (1) Bootstrapping
Your server should get an argument that specify the running folder when it started:
[your_server_name] [absolute_path_to_target_folder] [port number]
The server should parse all files in the folder into inverted index and store it in its own memory. (refer to “lecture 5” on KLMS) Because we will only check text files, you don’t have to consider binary files.

Notes:
You can only use pointer arithmetics when parsing. In other words, you should not use string library.
Generated inverted index should store line number with the file name.
When parsing, you should deal with only alphabetic words. All white spaces and special characters should be considered as delimiter of words.  
It is the same with HW1. You can just use what you implemented.


Example)

Text
Parsed Words
abc bcd
[abc, bcd]
abc”b cd”
[abc, b, cd]
abc[bcd]
[abc, bcd]
abc                     “bcd”
[abc, bcd]


### (2) Event-based server with I/O multiplexing to handle requests
In the server, there is a listener thread which receives requests from the client.
Listener thread manages multiple socket descriptors. Thus, you should use the “select()” function to pick pending inputs.
When the listener thread received a request, it delivers that request to a specific thread in a thread pool. Listener thread makes the connection with the client and receives a request from the client.
Idle thread in the thread pool should process request, and returns the search result to the client.
The tasks should be distributed to the 10 threads in the thread pool.
The protocol should be follow the section 4. protocol.

Notes:
You have to use memory allocation when you make packet. If you use tcmalloc that you implemented in HW2, you will get extra 30 points.
The program should return same result with HW1. You can just use what you implemented.

## 3. Implementation
### (1) Client1
The client should send multiple requests.
To generate simultaneous requests, we will use simple multiple threads using POSIX Threads (Pthreads) Interface.
After execution, the main process generates threads as the number of request from the arguments.
Then, each thread creates a new socket, connects the server.
You should receive two parameters for client code: number of threads & number of requests per thread.
Thus, the command line should be “./client1 [server_ip] [server_port] [number_of_threads] [number_of_requests_per_thread] [word_to_search]”, (e.g., ./client 127.0.0.1 8080 100 1000 abcd)
### (2) Client2
You should print “(your_program_name)>” to identify CLI just like HW1.
In this part, you will implement CLI client program for implemented server.
Your input for searching word should follow below form exactly.
search [word]
Your output for corresponded input should follow below form exactly.
[filename1]: line #[line number1]
[filename1]: line #[line number2]
[filename2]: line #[line number3]
[filename3]: line #[line number4]
The command line should be “./client2 [server_ip] [server_port]”

## 4. Protocol

Total Length (4 byte)
MSG Type (4 byte)
Payload (variable)

This denotes the protocol for this application, and server program should understand this protocol.

Total Length: all payload length (application data including this header)
MSG type
0x00000010: REQ from client to server
0x00000011: RESP from server to client
0x00000020: ERROR from server to client, when the file is not in server.
Payload: Text that contains the input or output of HW1.
[filename1]: line #[line number1]
[filename1]: line #[line number2]
[filename2]: line #[line number3]
[filename3]: line #[line number4]

