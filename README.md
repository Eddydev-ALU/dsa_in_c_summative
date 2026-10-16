# DSA Final Project (C)

Five standalone C programs, one per question.

| Folder | Question | Data structure / algorithm |
|---|---|---|
| `q1_event_logger` | Embedded Device Event Logger | Doubly linked list (max 25) |
| `q2_command_registry` | Network Device Command Registry | Binary search tree + edit distance |
| `q3_dependency_graph` | Server Dependency Network Analyzer | Directed graph (adjacency matrix) |
| `q4_supply_routing` | Emergency Medical Supply Routing | Weighted graph + Dijkstra |
| `q5_log_compression` | Firmware Log Compression | Huffman coding |

## Build and run

```sh
make                 # builds all five programs
make test            # 40 automated checks + memory-leak checks

cd q1_event_logger && ./event_logger
cd q2_command_registry && ./command_registry [commands_file]
cd q3_dependency_graph && ./dependency_graph
cd q4_supply_routing && ./supply_routing
cd q5_log_compression && ./huffman
```

No `make`? Compile one program directly: `cc -std=c11 -Wall -Wextra -o event_logger event_logger.c`.

Type `h` in Q1–Q3 to see the commands. Q3 and Q4 accept a location or server by name (case-insensitive) or by its list number.
