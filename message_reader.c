#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "message_slot.h"

int main(int argc, char** argv) {
    int fd;
    unsigned int channel_id;
    const char* device;
    char buffer[MAX_MESSAGE_LENGTH];
    ssize_t bytes_read;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device_file> <channel_id>\n", argv[0]);
        return 1;
    }

    device = argv[1];
    channel_id = atoi(argv[2]);

    if (channel_id == 0) {
        fprintf(stderr, "Invalid channel ID.\n");
        return 1;
    }

    fd = open(device, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0) {
        perror("ioctl MSG_SLOT_CHANNEL");
        close(fd);
        return 1;
    }

    bytes_read = read(fd, buffer, MAX_MESSAGE_LENGTH);
    if (bytes_read < 0) {
        perror("read");
        close(fd);
        return 1;
    }
    if (bytes_read == 0) {
        fprintf(stderr, "Error: Channel is empty.\n");
        close(fd);
        return 1;
    }

    if (write(STDOUT_FILENO, buffer, bytes_read) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}