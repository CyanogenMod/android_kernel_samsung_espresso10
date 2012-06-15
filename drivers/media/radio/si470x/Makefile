#
# Makefile for radios with Silicon Labs Si470x FM Radio Receivers
#

radio-usb-si470x-objs	:= radio-si470x-usb.o radio-si470x-common.o
radio-i2c-si470x-objs	:= radio-si470x-i2c.o radio-si470x-common.o
radio-dev-si470x-objs   := radio-si470x-dev.o $(radio-i2c-si470x-objs)

obj-$(CONFIG_USB_SI470X) += radio-usb-si470x.o
obj-$(CONFIG_I2C_SI470X) += radio-i2c-si470x.o
obj-$(CONFIG_DEV_SI470X) += radio-dev-si470x.o
