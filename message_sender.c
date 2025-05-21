#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "message_slot.h"

int main(int argc, char** argv) {
    int fd;
    unsigned int channel_id;
    unsigned int censorship;
    const char* device;
    const char* message;

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <device_file> <channel_id> <censorship> <message>\n", argv[0]);
        return 1;
    }

    device = argv[1];
    channel_id = atoi(argv[2]);
    censorship = atoi(argv[3]);
    message = argv[4];

    if (channel_id == 0 || (censorship != 0 && censorship != 1)) {
        fprintf(stderr, "Invalid channel ID or censorship value.\n");
        return 1;
    }

    fd = open(device, O_WRONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_SET_CEN, censorship) < 0) {
        perror("ioctl MSG_SLOT_SET_CEN");
        close(fd);
        return 1;
    }

    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0) {
        perror("ioctl MSG_SLOT_CHANNEL");
        close(fd);
        return 1;
    }

    if (write(fd, message, strlen(message)) < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}