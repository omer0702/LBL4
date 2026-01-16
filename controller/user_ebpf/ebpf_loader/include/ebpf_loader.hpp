#pragma once

#include <string>
#include <memory>
#include "../balancer.skel.h"

class EbpfLoader {
public:
    EbpfLoader();
    ~EbpfLoader();

    bool loadProgram();
    bool attachProgram(const std::string& interface_name);
    void detachProgram();

    struct balancer_bpf* get_skel() const {
        return skel;
    }
    
private:
    struct balancer_bpf* skel;
    int index;//network interface index
    bool is_attached;
};

    

