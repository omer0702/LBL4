#ifndef MAPS_H
#define MAPS_H

#include "../core/vmlinux.h"
#include <bpf/bpf_helpers.h>
#include "common_structs.h"

#define MAX_SERVICES 100
#define MAX_BACKENDS 256
#define SIZE 65537

struct inner_map{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, SIZE);
    __type(key, __u32);//offset
    __type(value, __u32);//backend ID
}inner_map_template SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH_OF_MAPS);
    __uint(max_entries, MAX_SERVICES);
    __type(key, __u32); //service IP
    __array(values, struct inner_map); //inner map fd
}services_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, MAX_BACKENDS);//change to 256*100
    __type(key, __u32);//backend ID
    __type(value, struct backend_info);
}backends_map SEC(".maps");


struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, MAX_BACKENDS);
    __type(key, __u32);//backend_id(logical id)
    __type(value, struct backend_stats);
} stats_map SEC(".maps");

#endif