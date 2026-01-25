CLANG :=clang
BPFTOOL :=bpftool
TARGET_ARCH := x86

CORE_DIR :=../core
COMMON_DIR :=../common
XDP_DIR :=../xdp_main
SKEL_DIR := ../../../controller/user_ebpf/ebpf_loader
VMLINUX := $(CORE_DIR)/vmlinux.h

BPF_OBJ := balancer.bpf.o
SKEL_H := $(SKEL_DIR)/balancer.skel.h

CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_$(TARGET_ARCH) \
			-I$(CORE_DIR) \
			-I$(COMMON_DIR) \
			-I$(XDP_DIR)/include \
			-I/usr/include/$(shell uname -m)-linux-gnu

all: $(SKEL_H)

$(VMLINUX):
	@echo "Generating vmlinux.h..."
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $(VMLINUX)

$(BPF_OBJ): $(XDP_DIR)/src/balancer.bpf.c $(VMLINUX)
	@echo "Compiling BPF program..."
	$(CLANG) $(CFLAGS) -c $< -o $@
$(SKEL_H): $(BPF_OBJ)
	@echo "Generating skeleton header..."
	$(BPFTOOL) gen skeleton $< > $@

clean:
	@echo "Cleaning up..."
	rm -f $(BPF_OBJ) $(VMLINUX) $(SKEL_H)

.PHONY: all clean