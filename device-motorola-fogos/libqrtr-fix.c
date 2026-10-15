#include <sys/types.h>
#include <sys/socket.h>
#include <linux/qrtr.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int qrtr_new_server(int sock, uint32_t service, uint32_t version, uint32_t instance)
{
    struct sockaddr_qrtr sq;
    socklen_t sl = sizeof(sq);
    struct qrtr_ctrl_pkt pkt;
    int ret;

    if (getsockname(sock, (struct sockaddr *)&sq, &sl) < 0) {
        perror("getsockname");
        return -1;
    }

    if (sq.sq_port == 0) {
        sq.sq_family = AF_QIPCRTR;
        if (bind(sock, (struct sockaddr *)&sq, sizeof(sq)) < 0) {
            perror("bind");
            return -1;
        }
        sl = sizeof(sq);
        if (getsockname(sock, (struct sockaddr *)&sq, &sl) < 0) {
            perror("getsockname after bind");
            return -1;
        }
    }

    struct sockaddr_qrtr ctrl_sq;
    memset(&ctrl_sq, 0, sizeof(ctrl_sq));
    ctrl_sq.sq_family = AF_QIPCRTR;
    ctrl_sq.sq_node = sq.sq_node;
    ctrl_sq.sq_port = QRTR_PORT_CTRL;

    memset(&pkt, 0, sizeof(pkt));
    pkt.cmd = QRTR_TYPE_NEW_SERVER;
    pkt.server.service = service;
    pkt.server.instance = (instance << 8) | (version & 0xff);
    pkt.server.node = sq.sq_node;
    pkt.server.port = sq.sq_port;

    ret = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&ctrl_sq, sizeof(ctrl_sq));
    fprintf(stderr, "[libqrtr-fix] published s=%u v=%u i=%u (encoded=0x%x) node=%u port=%u ret=%d\n",
            service, version, instance, pkt.server.instance, sq.sq_node, sq.sq_port, ret);

    // If service 64 (SERVREG_LOC), ALSO announce Instance 1 (APSS)
    if (service == 64 && instance != 1) {
        pkt.server.instance = (1 << 8) | (version & 0xff);
        ret = sendto(sock, &pkt, sizeof(pkt), 0, (struct sockaddr *)&ctrl_sq, sizeof(ctrl_sq));
        fprintf(stderr, "[libqrtr-fix] ALSO published s=64 instance=1 (encoded=0x%x) ret=%d\n",
                pkt.server.instance, ret);
    }

    return ret >= 0 ? 0 : -1;
}

int qrtr_publish(int sock, uint32_t service, uint32_t version, uint32_t instance)
{
    return qrtr_new_server(sock, service, version, instance);
}
