# Makefile for OVISP video driver

ifeq ($(CONFIG_VIDEO_OVISP_STUB),y)
obj-$(CONFIG_VIDEO_OVISP) += ovisp-videostub.o ovisp-videobuf.o isp-debug.o
else
obj-$(CONFIG_VIDEO_OVISP) += ovisp-base.o ovisp-video.o ovisp-videobuf.o ovisp-isp.o ovisp-csi.o isp-i2c.o isp-ctrl.o ovisp-debugtool.o \
				isp-debug.o
obj-$(CONFIG_VIDEO_JZ_HS_ISP) += jz_hs_isp.o
obj-$(CONFIG_VIDEO_OV5645) += isp-sensor-ov5645-setting.o
obj-$(CONFIG_VIDEO_OV6211) += isp-sensor-ov6211-setting.o
obj-$(CONFIG_VIDEO_OV9724) += isp-sensor-ov9724-setting.o
obj-$(CONFIG_VIDEO_OV8858) += isp-sensor-ov8858-setting.o
obj-$(CONFIG_VIDEO_OV8856) += isp-sensor-ov8856-setting.o
obj-$(CONFIG_VIDEO_OV8865) += isp-sensor-ov8865-setting.o
obj-$(CONFIG_VIDEO_OV5648) += isp-sensor-ov5648-setting.o
obj-$(CONFIG_VIDEO_OV4689) += isp-sensor-ov4689-setting.o
obj-$(CONFIG_DVP_OV9712) += isp-sensor-ov9712-setting.o
obj-$(CONFIG_DVP_BF3116) += isp-sensor-bf3116-setting.o
endif
