#include "ebpf_loader.hpp"
#include <iostream>
#include <net/if.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <linux/if_link.h>

EbpfLoader::EbpfLoader(): skel(nullptr), index(-1), is_attached(false) {
}

EbpfLoader::~EbpfLoader() {
    std::cout << "[LOADER]in EbpfLoader destructor" << std::endl;
    detachProgram();
    if (skel) {
        balancer_bpf__destroy(skel);
    }
}

bool EbpfLoader::loadProgram(){
    skel = balancer_bpf__open();
    if(!skel){
        std::cerr << "[LOADER]Failed to open BPF skeleton" << std::endl;
        return false;
    }

    int error = balancer_bpf__load(skel);
    if(error){
        std::cerr << "[LOADER]Failed to load BPF program: " << strerror(-error) << std::endl;
        return false;
    }

    std::cout << "[LOADER]BPF program loaded successfully" << std::endl;
    return true;
}

bool EbpfLoader::attachProgram(const std::string& name){
    if(!skel){
        return false;
    }

    index = if_nametoindex(name.c_str());
    if(index == 0){
        std::cerr << "[LOADER] invalid interface name: " << name << std::endl;
        return false;
    }

    int prog_fd = bpf_program__fd(skel->progs.xdp_balancer_prog);
    
    int error = bpf_xdp_attach(index, prog_fd, XDP_FLAGS_SKB_MODE, NULL);

    if(error < 0){
        std::cerr << "[LOADER]Failed to attach XDP program: " << strerror(-error) << std::endl;
        return false;
    }

    is_attached = true;
    std::cout << "[LOADER]successfully attached XDP program to interface: " << name << std::endl;
    return true;
}

void EbpfLoader::detachProgram(){
    std::cout << "[LOADER] in detachProgram" << std::endl;
    if(is_attached && skel){
        int error = bpf_xdp_attach(index, -1, XDP_FLAGS_SKB_MODE, NULL);
        if(error < 0){
            std::cerr << "[LOADER] failed to detach XDP program:" << strerror(-error) << std::endl;
        }
        else{
            std::cout << "[LOADER] succesfully detached XDP program" << std::endl;
        }

        bpf_link__destroy(skel->links.xdp_balancer_prog);
        skel->links.xdp_balancer_prog = nullptr;
        
        is_attached = false;
    }
}