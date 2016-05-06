
# Kernel makefile for motions sensors
#
#

# MPU
obj-$(CONFIG_MPU_SENSORS_MPU3050_V333)	+= mpu3050.o
mpu3050-objs += mpuirq.o \
	slaveirq.o \
	mpu-dev.o \
	mpu-i2c.o \
	mlsl-kernel.o \
	mlos-kernel.o \
	mpu-accel.o \
	$(MLLITE_DIR)mldl_cfg.o

EXTRA_CFLAGS += -I$(M)/$(MLLITE_DIR) \
                -I$(M)/../../include \
		-Idrivers/misc/mpu3050 \
                -Iinclude/linux

obj-$(CONFIG_MPU_SENSORS_TIMERIRQ_V333)+= timerirq.o

ifdef CONFIG_MPU_SENSORS_DEBUG_V333
EXTRA_CFLAGS += -DDEBUG
endif
