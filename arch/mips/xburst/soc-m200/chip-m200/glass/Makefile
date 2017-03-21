
CFLAGS_KERNEL	+= -I$(srctree)/arch/mips/xburst/soc-m200/chip-m200/$(CONFIG_PRODUCT_NAME)/common
ifdef CONFIG_BOARD_VERSION
CFLAGS_KERNEL	+= -I$(srctree)/arch/mips/xburst/soc-m200/chip-m200/$(CONFIG_PRODUCT_NAME)/$(CONFIG_BOARD_NAME)/$(CONFIG_BOARD_NAME)_$(CONFIG_BOARD_VERSION)
else
CFLAGS_KERNEL	+= -I$(srctree)/arch/mips/xburst/soc-m200/chip-m200/$(CONFIG_PRODUCT_NAME)/$(CONFIG_BOARD_NAME)
endif

obj-y		+= common/
obj-y		+= $(CONFIG_BOARD_NAME)/
