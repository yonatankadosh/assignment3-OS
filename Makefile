obj-m += message_slot.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -O3 -Wall -std=c11 message_sender.c -o message_sender
	gcc -O3 -Wall -std=c11 message_reader.c -o message_reader

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f message_sender message_reader