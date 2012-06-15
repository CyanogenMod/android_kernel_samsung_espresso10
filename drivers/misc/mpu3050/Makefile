
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

#
# Accel options
#
ifdef CONFIG_MPU_SENSORS_ADXL346_V333
mpu3050-objs += $(MLLITE_DIR)accel/adxl346.o
endif

ifdef CONFIG_MPU_SENSORS_BMA150_V333
mpu3050-objs += $(MLLITE_DIR)accel/bma150.o
endif

ifdef CONFIG_MPU_SENSORS_BMA222_V333
mpu3050-objs += $(MLLITE_DIR)accel/bma222.o
endif

ifdef CONFIG_MPU_SENSORS_BMA250_V333
mpu3050-objs += $(MLLITE_DIR)accel/bma250.o
endif

ifdef CONFIG_MPU_SENSORS_KXSD9_V333
mpu3050-objs += $(MLLITE_DIR)accel/kxsd9.o
endif

ifdef CONFIG_MPU_SENSORS_KXTF9_V333
mpu3050-objs += $(MLLITE_DIR)accel/kxtf9.o
endif

ifdef CONFIG_MPU_SENSORS_LIS331DLH_V333
mpu3050-objs += $(MLLITE_DIR)accel/lis331.o
endif

ifdef CONFIG_MPU_SENSORS_LIS3DH_V333
mpu3050-objs += $(MLLITE_DIR)accel/lis3dh.o
endif

ifdef CONFIG_MPU_SENSORS_LSM303DLHA_V333
mpu3050-objs += $(MLLITE_DIR)accel/lsm303a.o
endif

ifdef CONFIG_MPU_SENSORS_MMA8450_V333
mpu3050-objs += $(MLLITE_DIR)accel/mma8450.o
endif

ifdef CONFIG_MPU_SENSORS_MMA845X_V333
mpu3050-objs += $(MLLITE_DIR)accel/mma845x.o
endif

#
# Compass options
#
ifdef CONFIG_MPU_SENSORS_AK8975_V333
mpu3050-objs += $(MLLITE_DIR)compass/ak8975.o
endif

ifdef CONFIG_MPU_SENSORS_AMI30X_V333
mpu3050-objs += $(MLLITE_DIR)compass/ami30x.o
endif

ifdef CONFIG_MPU_SENSORS_HMC5883_V333
mpu3050-objs += $(MLLITE_DIR)compass/hmc5883.o
endif

ifdef CONFIG_MPU_SENSORS_LSM303DLHM_V333
mpu3050-objs += $(MLLITE_DIR)compass/lsm303m.o
endif

ifdef CONFIG_MPU_SENSORS_MMC314X_V333
mpu3050-objs += $(MLLITE_DIR)compass/mmc314x.o
endif

ifdef CONFIG_MPU_SENSORS_MMC328X_V333
mpu3050-objs += $(MLLITE_DIR)compass/mmc328x.o
endif

ifdef CONFIG_MPU_SENSORS_YAS529_V333
mpu3050-objs += $(MLLITE_DIR)compass/yas529-kernel.o
endif

ifdef CONFIG_MPU_SENSORS_YAS530_V333
mpu3050-objs += $(MLLITE_DIR)compass/yas530.o
endif

ifdef CONFIG_MPU_SENSORS_HSCDTD002B_V333
mpu3050-objs += $(MLLITE_DIR)compass/hscdtd002b.o
endif

ifdef CONFIG_MPU_SENSORS_HSCDTD004A_V333
mpu3050-objs += $(MLLITE_DIR)compass/hscdtd004a.o
endif

ifdef CONFIG_MPU_SENSORS_AMS0303_V333
mpu3050-objs += $(MLLITE_DIR)compass/ams0303.o
endif

EXTRA_CFLAGS += -I$(M)/$(MLLITE_DIR) \
                -I$(M)/../../include \
		-Idrivers/misc/mpu3050 \
                -Iinclude/linux

obj-$(CONFIG_MPU_SENSORS_MPU6000_V333)+= mpu6000.o
mpu6000-objs += mpuirq.o \
	slaveirq.o \
	mpu-dev.o \
	mpu-i2c.o \
	mlsl-kernel.o \
	mlos-kernel.o \
	$(MLLITE_DIR)mldl_cfg.o \
	$(MLLITE_DIR)accel/mantis.o

ifdef CONFIG_MPU_SENSORS_AK8975_V333
mpu6000-objs += $(MLLITE_DIR)compass/ak8975.o
endif

ifdef CONFIG_MPU_SENSORS_MPU6000_V333
EXTRA_CFLAGS += -DM_HW
endif

obj-$(CONFIG_MPU_SENSORS_TIMERIRQ_V333)+= timerirq.o

ifdef CONFIG_MPU_SENSORS_DEBUG_V333
EXTRA_CFLAGS += -DDEBUG
endif

