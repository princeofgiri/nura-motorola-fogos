#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

int main(void) {
    mkdir("/dev/socket", 0755);
    unlink("/dev/socket/logdw");
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return 1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/dev/socket/logdw", sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    chmod("/dev/socket/logdw", 0777);

    FILE *out = fopen("/var/log/android_log.log", "a");
    if (!out) out = stderr;

    char buf[4096];
    while (1) {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n > 12) {
            buf[n] = '\0';
            char *tag = buf + 12;
            char *msg = tag + strlen(tag) + 1;
            if (msg < buf + n) {
                fprintf(out, "[%s] %s\n", tag, msg);
            } else {
                fprintf(out, "[%s]\n", tag);
            }
            fflush(out);
        }
    }
    return 0;
}
