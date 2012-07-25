obj-m += dislevitate.o

all:
	make -C /home/jann/software/linux-defy/kernel/ M=$(PWD) modules

clean:
	make -C /home/jann/software/linux-defy/kernel/ M=$(PWD) clean
