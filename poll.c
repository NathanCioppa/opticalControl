#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

int main(void) {
    int fd = open("/sys/block/sr0/events_async", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct pollfd pfd = { .fd = fd, .events = POLLPRI };

    char buf[128];
    while (1) {
        // wait for change
        int ret = poll(&pfd, 1, -1);
        if (ret > 0 && (pfd.revents & POLLPRI)) {
            lseek(fd, 0, SEEK_SET); // rewind
            int len = read(fd, buf, sizeof(buf)-1);
            buf[len] = '\0';
            printf("Event triggered: %s\n", buf);
        }
    }

    close(fd);
    return 0;
}
