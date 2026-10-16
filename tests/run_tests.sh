#!/bin/sh
# Automated tests: feeds scripted input to each program, checks the output,
# then checks for memory leaks (leaks on macOS, valgrind on Linux).
# Usage: ./tests/run_tests.sh        (run from the project root)

cd "$(dirname "$0")/.." || exit 1
ROOT=$(pwd)
CC=${CC:-cc}
PASS=0; FAIL=0

build() { $CC -std=c11 -Wall -Wextra -pedantic -g -o "$1" "$1.c" || { echo "BUILD FAILED: $1"; exit 1; }; }

# check <name> <expected text> <output>
check() {
    if printf '%s' "$3" | grep -qF -- "$2"; then PASS=$((PASS+1)); echo "  PASS  $1"
    else FAIL=$((FAIL+1)); echo "  FAIL  $1  (expected: $2)"; fi
}

# leakcheck <name> <binary> <input> [args]
leakcheck() {
    if command -v leaks >/dev/null 2>&1; then
        out=$(printf "$3" | MallocStackLogging=1 leaks --atExit -- "$2" $4 2>&1)
        check "$1 no leaks" " 0 leaks for 0 total leaked bytes" "$out"
    elif command -v valgrind >/dev/null 2>&1; then
        out=$(printf "$3" | valgrind --leak-check=full "$2" $4 2>&1)
        check "$1 no leaks" "All heap blocks were freed" "$out"
    else echo "  SKIP  $1 leak check (no leaks/valgrind)"; fi
}

echo "== Q1 Event Logger (doubly linked list) =="
cd "$ROOT/q1_event_logger" && build event_logger
out=$(printf 'c\nn\np\nx\na\n1\nT_01\n3\nc\ns\n30\nl\np\nx\nc\nq\n' | ./event_logger)
check "empty list handled"          "Log is empty."               "$out"
check "first add sets current"      "TEMPERATURE_WARNING | T_01"  "$out"
check "26th event evicts oldest"    "Oldest event #1 removed"     "$out"
check "list capped at 25"           "25/25 events"                "$out"
check "cannot go before oldest"     "Already at the oldest event" "$out"
check "clear releases all"          "All events cleared"          "$out"
leakcheck "Q1" ./event_logger 's\n60\np\nx\ns\n10\nq\n'

echo "== Q2 Command Registry (BST) =="
cd "$ROOT/q2_command_registry" && build command_registry
rm -f audit.log
out=$(printf 'v\nSHOW_STATUS\nv\nSHOW_STATU\nv\nZZZ\nv\nbad input!\np\nx\nv\nSET_IP\nq\n' | ./command_registry)
check "exact match approved"        "COMMAND APPROVED"            "$out"
check "similar -> suggestion"       "Did you mean: SHOW_STATUS?"  "$out"
check "invalid input rejected"      "Invalid input"               "$out"
check "in-order is alphabetical"    "1. ADD_ROUTE"                "$out"
check "search on empty tree"        "registry is empty"           "$out"
check "unknown written to audit"    "| UNKNOWN | ZZZ"             "$(cat audit.log)"
check "approved not in audit"       "0"                           "$(grep -c '| SHOW_STATUS$' audit.log)"
check "duplicates skipped"          "Skipped 1 duplicate"         "$(printf 'q\n' | ./command_registry ../tests/data/commands_duplicates.txt)"
check "empty file handled"          "command file is empty"       "$(printf 'q\n' | ./command_registry ../tests/data/commands_empty.txt)"
check "50 command limit"            "Limit of 50 reached"         "$(printf 'q\n' | ./command_registry ../tests/data/commands_60.txt)"
leakcheck "Q2" ./command_registry 'v\nSHOW_STATU\nx\nl\nq\n'

echo "== Q3 Dependency Graph (adjacency matrix) =="
cd "$ROOT/q3_dependency_graph" && build dependency_graph
out=$(printf 'q\nAPI Server\na\nAPI SERVER\nc\nWeb Server\nAPI Server\nc\nGhost\nd\n6\n1\nr\nDatabase Server\nq\nAPI Server\nm\nr\n1\nr\n1\nr\n1\nr\n1\nr\n1\nq\n1\nm\nx\n' | ./dependency_graph)
check "outgoing query"              "    - Cache Server"          "$out"
check "incoming query"              "Services depending on API Server:" "$out"
check "duplicate server rejected"   "already exists"              "$out"
check "duplicate edge rejected"     "dependency already exists"   "$out"
check "missing server handled"      "'Ghost' does not exist"      "$out"
check "missing edge handled"        "dependency does not exist"   "$out"
check "remove server drops edges"   "Server 'Database Server' and all its dependencies removed" "$out"
check "empty graph handled"         "The graph is empty."         "$out"

echo "== Q4 Supply Routing (Dijkstra) =="
cd "$ROOT/q4_supply_routing" && build supply_routing
out=$(printf '2\n1\nHealth Center C\n2\nMars\n2\n1\n1\n5\nMobile Clinic\n2\n1\nMobile Clinic\n4\n1\n2\n-3\n4\n1\n2\nx\n0\n' | ./supply_routing)
check "shortest route path"         "Central Warehouse -> Distribution Center -> Health Center A -> Health Center C" "$out"
check "shortest route distance"     "Total Distance: 17 km"       "$out"
check "unknown location"            "'Mars' does not exist"       "$out"
check "same source and destination" "are the same"                "$out"
check "unreachable destination"     "No route exists"             "$out"
check "negative distance rejected"  "must be greater than 0"      "$out"
check "non-numeric distance"        "enter a whole number"        "$out"

echo "== Q5 Huffman Compression =="
cd "$ROOT/q5_log_compression" && build huffman
rm -f device_log.huff device_log_restored.txt
out=$(printf '2\n3\n1\n2\n9\n4\n' | ./huffman)
check "missing .huff handled"       "not found"                   "$out"
check "frequency table shown"       "Frequency"                   "$out"
check "stats shown"                 "Compression ratio:"          "$out"
check "lossless round trip"         "Verification: SUCCESS"       "$out"
check "byte-identical (cmp)"        "same" "$(cmp -s device_log.txt device_log_restored.txt && echo same)"
check "invalid menu choice"         "Invalid choice"              "$out"
leakcheck "Q5" ./huffman '1\n2\n4\n'

echo
echo "Result: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
