CC     ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2

BINS = q1_event_logger/event_logger \
       q2_command_registry/command_registry \
       q3_dependency_graph/dependency_graph \
       q4_supply_routing/supply_routing \
       q5_log_compression/huffman

all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	./tests/run_tests.sh

clean:
	rm -f $(BINS)

.PHONY: all test clean
