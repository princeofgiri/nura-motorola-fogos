#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdint.h>

#define RMNET_IOCTL_EXTENDED                 0x000089FD
#define RMNET_IOCTL_SET_EGRESS_DATA_FORMAT   0x0006
#define RMNET_IOCTL_SET_INGRESS_DATA_FORMAT  0x0007
#define RMNET_IOCTL_ADD_MUX_CHANNEL          0x0005

struct rmnet_ioctl_extended_s {
    uint32_t   extended_ioctl;
    union {
        uint32_t data;
        int8_t if_name[16];
        struct {
            uint32_t  mux_id;
            int8_t   vchannel_name[16];
        } rmnet_mux_val;
        struct {
            uint8_t   flow_mode;
            uint8_t   mux_id;
        } flow_control_prop;
        struct {
            uint32_t   consumer_pipe_num;
            uint32_t   producer_pipe_num;
        } ipa_ep_pair;
        struct {
            uint32_t __data;
            uint32_t agg_size;
            uint32_t agg_count;
        } ingress_format;
    } u;
};

int main(int argc, char **argv) {
    const char *ifname = "rmnet_ipa0";
    uint32_t mux_id = 1;
    const char *vname = "qmapmux0.0";

    if (argc >= 2) {
        if (strncmp(argv[1], "qmapmux", 7) == 0) {
            vname = argv[1];
            if (argc >= 3) mux_id = atoi(argv[2]);
        } else {
            ifname = argv[1];
            if (argc >= 3) mux_id = atoi(argv[2]);
            if (argc >= 4) vname = argv[3];
        }
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return 1;
    }

    struct ifreq ifr;
    struct rmnet_ioctl_extended_s ext;

    // 1. Ingress data format (4-byte QMAP header, no aggregation)
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    memset(&ext, 0, sizeof(ext));
    ext.extended_ioctl = RMNET_IOCTL_SET_INGRESS_DATA_FORMAT;
    ext.u.data = 0;
    ifr.ifr_data = (void *)&ext;
    ioctl(fd, RMNET_IOCTL_EXTENDED, &ifr);

    // 2. Egress data format (4-byte QMAP header, bypass aggregation)
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    memset(&ext, 0, sizeof(ext));
    ext.extended_ioctl = RMNET_IOCTL_SET_EGRESS_DATA_FORMAT;
    ext.u.data = 0;
    ifr.ifr_data = (void *)&ext;
    ioctl(fd, RMNET_IOCTL_EXTENDED, &ifr);

    // 3. Add mux channel
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    memset(&ext, 0, sizeof(ext));
    ext.extended_ioctl = RMNET_IOCTL_ADD_MUX_CHANNEL;
    ext.u.rmnet_mux_val.mux_id = mux_id;
    strncpy((char *)ext.u.rmnet_mux_val.vchannel_name, vname, 15);
    ifr.ifr_data = (void *)&ext;
    ioctl(fd, RMNET_IOCTL_EXTENDED, &ifr);

    close(fd);
    return 0;
}
