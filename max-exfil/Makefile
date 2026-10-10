obj-m += speedtest-max-exfil.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(~/speedtest-data-exfil/max-exfil) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(~/speedtest-data-exfil/max-exfil) clean
