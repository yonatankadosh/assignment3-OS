//#ifndef MESSAGE_SLOT_H
//#define MESSAGE_SLOT_H

#include <linux/ioctl.h>

#define MAJOR_NUM 235
#define DEVICE_RANGE_NAME "message_slot"
#define MAX_MESSAGE_LENGTH 128

#define MSG_SLOT_CHANNEL   _IOW(MAJOR_NUM, 0, unsigned int)
#define MSG_SLOT_SET_CEN   _IOW(MAJOR_NUM, 1, unsigned int)

//#endif