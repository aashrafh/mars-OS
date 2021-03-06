BUILD_DIR = build
CC = gcc

CFLAGS ?= -g -Wall
SRCS := $(shell find . -name "*.c")
OBJS := $(SRCS:.c=.out)

.PHONY: all
all:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) priority_queue.c buddy.c ready_queue.c scheduler.c -o $(BUILD_DIR)/scheduler.out -lm
	$(CC) $(CFLAGS) process_generator.c -o $(BUILD_DIR)/process_generator.out
	$(CC) $(CFLAGS) test_generator.c -o $(BUILD_DIR)/test_generator.out
	$(CC) $(CFLAGS) process.c -o $(BUILD_DIR)/process.out
	$(CC) $(CFLAGS) clk.c -o $(BUILD_DIR)/clk.out
	

scheduler.out: scheduler.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) priority_queue.c buddy.c  ready_queue.c scheduler.c -o $(BUILD_DIR)/scheduler.out -lm

process_generator.out: process_generator.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) process_generator.c -o $(BUILD_DIR)/process_generator.out

test_generator.out: test_generator.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) test_generator.c -o $(BUILD_DIR)/test_generator.out

process.out: process.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) process.c -o $(BUILD_DIR)/process.out

clk.out: clk.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) clk.c -o $(BUILD_DIR)/clk.out


.PHONY: clean
clean:
	rm -f -r $(BUILD_DIR) 

.PHONY: run
run:
	./$(BUILD_DIR)/process_generator.out

.PHONY: run_valgrind
run_valgrind:	
	valgrind ./$(BUILD_DIR)/process_generator.out


.PHONY: generate_test
generate_test:
	./$(BUILD_DIR)/test_generator.out