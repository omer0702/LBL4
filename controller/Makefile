.PHONY: all clean proto

CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -I./proto/gen -I/usr/include -I./user_ebpf/include
CFLAGS = -Wall -Wextra -I./proto/gen -I/usr/include I./user_ebpf/include
LDFLAGS = $(shell pkg-config --libs protobuf)

# קבצי proto
PROTO_SRC = proto/service.proto
PROTO_GEN_DIR = proto/gen
PROTO_CPP = $(PROTO_GEN_DIR)/service.pb.cc

# קבצי קוד
SRC_C = user_ebpf/src/io_epoll.c user_ebpf/src/main.c
SRC_CPP = $(PROTO_CPP)

# פלט סופי
TARGET = lb_app

all: proto $(TARGET)

proto:
	@mkdir -p $(PROTO_GEN_DIR)
	protoc -I=proto --cpp_out=$(PROTO_GEN_DIR) $(PROTO_SRC)
	@echo "[OK] Proto generated → $(PROTO_GEN_DIR)"

$(TARGET): $(SRC_C) $(SRC_CPP)
	$(CXX) $(SRC_C) $(SRC_CPP) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)
	@echo "[OK] Built target $(TARGET)"

clean:
	rm -f $(TARGET)
	rm -f $(PROTO_GEN_DIR)/*.pb.cc
	rm -f $(PROTO_GEN_DIR)/*.pb.h
	@echo "[OK] Clean done"
