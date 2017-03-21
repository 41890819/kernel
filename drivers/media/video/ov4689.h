#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <mach/ovisp-v4l2.h>
#include <mach/regs-isp.h>
#include "sensor.h"

#define OV4689_MAX_WIDTH		2688
#define OV4689_MAX_HEIGHT		1520

#define OV4689_RG_Ratio_Typical        0x12b
#define OV4689_BG_Ratio_Typical        0x13a

#define OV4689_SETTING_PROFILE_2

#define OV4689_DEFAULT_REG_WRTIE_MODE   ISP_CMD_GROUP_WRITE

#if defined OV4689_SETTING_PROFILE_2
static struct regval_list ov4689_init_default[] = {
	// 2lane, 720Mbps, 30fps
	{0x0103, 0x01},
	{0x3638, 0x00},
	{0x0300, 0x00},
	{0x0302, 0x1e},
	{0x0303, 0x00},
	{0x0304, 0x03},
	{0x030b, 0x00},
	{0x030d, 0x1e},
	{0x030e, 0x04},
	{0x030f, 0x01},
	{0x0312, 0x01},
	{0x031e, 0x00},
	{0x3000, 0x20},
	{0x3002, 0x00},
	{0x3018, 0x32},
	{0x3020, 0x93},
	{0x3021, 0x03},
	{0x3022, 0x01},
#ifdef OV4689_CAMERA_RAW_8
	{0x3031, 0x08},
#else
	{0x3031, 0x0a},
#endif
	{0x303f, 0x0c},
	{0x3305, 0xf1},
	{0x3307, 0x04},
	{0x3309, 0x29},

	{0x350b, 0x00},
	/* {0x350c, 0x00}, */
	/* {0x350d, 0x00}, */
	/* {0x350e, 0x00}, */
	/* {0x350f, 0x80}, */
	/* {0x3510, 0x00}, */
	/* {0x3511, 0x00}, */
	{0x3512, 0x00},
	/* {0x3513, 0x00}, */
	/* {0x3514, 0x00}, */
	/* {0x3515, 0x80}, */
	/* {0x3516, 0x00}, */
	{0x3517, 0x00},
	/* {0x3518, 0x00}, */
	/* {0x3519, 0x00}, */
	/* {0x351a, 0x00}, */
	/* {0x351b, 0x80}, */
	/* {0x351c, 0x00}, */
	{0x351d, 0x00},
	/* {0x351e, 0x00}, */
	/* {0x351f, 0x00}, */
	/* {0x3520, 0x00}, */
	/* {0x3521, 0x80}, */
	{0x3522, 0x08},
	{0x3524, 0x08},
	{0x3526, 0x08},
	{0x3528, 0x08},
	{0x352a, 0x08},
	{0x3602, 0x00},
	/* {0x3603, 0x40}, */
	{0x3604, 0x02},
	{0x3605, 0x00},
	{0x3606, 0x00},
	{0x3607, 0x00},
	{0x3609, 0x12},
	{0x360a, 0x40},
	{0x360c, 0x08},
	{0x360f, 0xe5},
	{0x3608, 0x8f},
	{0x3611, 0x00},
	{0x3613, 0xf7},
	{0x3616, 0x58},
	{0x3619, 0x99},
	{0x361b, 0x60},
	{0x361c, 0x7a},
	{0x361e, 0x79},
	{0x361f, 0x02},
	{0x3632, 0x00},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x15},
	{0x3646, 0x86},
	{0x364a, 0x0b},
	{0x3700, 0x17},
	{0x3701, 0x22},
	{0x3703, 0x10},
	{0x370a, 0x37},
	{0x3705, 0x00},
	{0x3706, 0x63},
	{0x3709, 0x3c},
	{0x370b, 0x01},
	{0x370c, 0x30},
	{0x3710, 0x24},
	{0x3711, 0x0c},
	{0x3716, 0x00},
	{0x3720, 0x28},
	{0x3729, 0x7b},
	{0x372a, 0x84},
	{0x372b, 0xbd},
	{0x372c, 0xbc},
	{0x372e, 0x52},
	{0x373c, 0x0e},
	{0x373e, 0x33},
	{0x3743, 0x10},
	{0x3744, 0x88},
	{0x3745, 0xc0},
	{0x374a, 0x43},
	{0x374c, 0x00},
	{0x374e, 0x23},
	{0x3751, 0x7b},
	{0x3752, 0x84},
	{0x3753, 0xbd},
	{0x3754, 0xbc},
	{0x3756, 0x52},
	{0x375c, 0x00},
	{0x3760, 0x00},
	{0x3761, 0x00},
	{0x3762, 0x00},
	{0x3763, 0x00},
	{0x3764, 0x00},
	{0x3767, 0x04},
	{0x3768, 0x04},
	{0x3769, 0x08},
	{0x376a, 0x08},
	{0x376b, 0x20},
	{0x376c, 0x00},
	{0x376d, 0x00},
	{0x376e, 0x00},
	{0x3773, 0x00},
	{0x3774, 0x51},
	{0x3776, 0xbd},
	{0x3777, 0xbd},
	{0x3781, 0x18},
	{0x3783, 0x25},
	{0x3798, 0x1b},
	{0x3800, 0x00},
	{0x3801, 0x08},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x0a},
	{0x3805, 0x97},
	{0x3806, 0x05},
	{0x3807, 0xfb},
	{0x3808, 0x0a},
	{0x3809, 0x80},
	{0x380a, 0x05},
	{0x380b, 0xf0},
	/* {0x380c, 0x0a}, */
	/* {0x380d, 0x18}, */
	/* {0x380e, 0x07}, */
	/* {0x380f, 0x46}, */
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3819, 0x01},
	{0x3820, 0x06},
	{0x3821, 0x00},
	{0x3829, 0x00},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x382d, 0x7f},
	{0x3830, 0x04},
	{0x3836, 0x01},
	{0x3837, 0x00},
	{0x3841, 0x02},
	{0x3846, 0x08},
	{0x3847, 0x07},
	{0x3d85, 0x36},
	{0x3d8c, 0x71},
	{0x3d8d, 0xcb},
	{0x3f0a, 0x00},
	{0x4000, 0xf1},
	{0x4001, 0x40},
	{0x4002, 0x04},
	{0x4003, 0x14},
	{0x400e, 0x00},
	{0x4011, 0x00},
	{0x401a, 0x00},
	{0x401b, 0x00},
	{0x401c, 0x00},
	{0x401d, 0x00},
	{0x401f, 0x00},
	{0x4020, 0x00},
	{0x4021, 0x10},
	{0x4022, 0x07},
	{0x4023, 0xcf},
	{0x4024, 0x09},
	{0x4025, 0x60},
	{0x4026, 0x09},
	{0x4027, 0x6f},
	{0x4028, 0x00},
	{0x4029, 0x02},
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402c, 0x02},
	{0x402d, 0x02},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4302, 0xff},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4305, 0x00},
	{0x4306, 0x00},
	{0x4308, 0x02},
	{0x4500, 0x6c},
	{0x4501, 0xc4},
	{0x4502, 0x40},
	{0x4503, 0x01},
	{0x4601, 0x04},
	{0x4800, 0x04},
	{0x4813, 0x08},
	{0x481f, 0x40},
	{0x4825, 0x4a},
	{0x4829, 0x78},
	{0x4837, 0x16},
	{0x4b00, 0x2a},
	{0x4b0d, 0x00},
	{0x4d00, 0x04},
	{0x4d01, 0x42},
	{0x4d02, 0xd1},
	{0x4d03, 0x93},
	{0x4d04, 0xf5},
	{0x4d05, 0xc1},
	{0x5000, 0xf3},
	{0x5001, 0x11},
	{0x5004, 0x00},
	{0x500a, 0x00},
	{0x500b, 0x00},
	{0x5032, 0x00},
	{0x5040, 0x00},
	{0x5050, 0x0c},
	{0x5500, 0x00},
	{0x5501, 0x10},
	{0x5502, 0x01},
	{0x5503, 0x0f},
	{0x8000, 0x00},
	{0x8001, 0x00},
	{0x8002, 0x00},
	{0x8003, 0x00},
	{0x8004, 0x00},
	{0x8005, 0x00},
	{0x8006, 0x00},
	{0x8007, 0x00},
	{0x8008, 0x00},
	{0x3638, 0x00},
	{0x0300, 0x60},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov4689_2688_1520_profile_2[] = {

#ifdef OV4689_CAMERA_RAW_8
	{0x3031, 0x08},
#else
	{0x3031, 0x0a},
#endif
	{0x3632, 0x00}, // ADC & Analog
	{0x374a, 0x43},
	{0x376b, 0x20}, // Sensor control
	{0x3800, 0x00}, // H crop start H
	{0x3801, 0x08}, // H crop start L
	{0x3803, 0x04}, // V crop start L
	{0x3804, 0x0a}, // H crop end H
	{0x3805, 0x97}, // H crop end L
	{0x3807, 0xfb}, // V crop end L
	{0x3808, 0x0a}, // H output size H
	{0x3809, 0x80}, // H output size L
	{0x380a, 0x05}, // V output size H
	{0x380b, 0xf0}, // V output size L
	{0x380c, 0x0a}, // HTS H
	{0x380d, 0x10}, // HTS L
	{0x380e, 0x06}, // VTS H
	{0x380f, 0xb9}, // VTS L
	{0x3811, 0x08}, // H win off L
	{0x3813, 0x04}, // V win off L
	{0x3814, 0x01}, // H inc odd
	{0x3820, 0x06}, // flip off, bin off
	{0x3821, 0x00}, // mirror on, bin off
	{0x382a, 0x01}, // vertical subsample odd increase number
	{0x3830, 0x04}, // blc use num/2
	{0x3836, 0x01}, // r zline use num/2
	{0x4001, 0x40}, // debug mode
	{0x4003, 0x14},
	{0x4022, 0x07}, // Anchor left end H
	{0x4023, 0xcf}, // Anchor left end L
	{0x4024, 0x09}, // Anchor right start H
	{0x4025, 0x60}, // Andhor right start L
	{0x4026, 0x09}, // Anchor right end H
	{0x4027, 0x6f}, // Anchor right end L
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4502, 0x40}, // ADC sync control
	{0x4601, 0x04}, // V fifo read start
	{0x5050, 0x0c},

	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov4689_1920_1080_profile_2[] = {
#ifdef OV4689_CAMERA_RAW_8
	{0x3031, 0x08},
#else
	{0x3031, 0x0a},
#endif
	{0x3632, 0x00},  // ADC & Analog
	{0x374a, 0x43},
	{0x376b, 0x20},  // Sensor control
	{0x3800, 0x01},  // H crop start H
	{0x3801, 0x88},  // H crop start L
	{0x3803, 0xe0},  // V crop start L
	{0x3804, 0x09},  // H crop end H
	{0x3805, 0x17},  // H crop end L
	{0x3807, 0x1f},  // V crop end L
	{0x3808, 0x07},  // H output size H
	{0x3809, 0x80},  // H output size L
	{0x380a, 0x04},  // V output size H
	{0x380b, 0x38},  // V output size L
	{0x380c, 0x0a},  // HTS H
	{0x380d, 0x10},  // HTS L
	{0x380e, 0x07},  // VTS H
	{0x380f, 0x46},  // VTS L
	{0x3811, 0x08},  // H win off L
	{0x3813, 0x04},  // V win off L
	{0x3814, 0x01},  // H inc odd
	{0x3820, 0x06},  // flip off, bin off
	{0x3821, 0x00},  // mirror on, bin off
	{0x382a, 0x01},  // vertical subsample odd increase number
	{0x3830, 0x04},  // blc use num/2
	{0x3836, 0x01},  // r zline use num/2
	{0x4001, 0x40},  // debug mode
	{0x4003, 0x14},
	{0x4022, 0x06},  // Anchor left end H
	{0x4023, 0x13},  // Anchor left end L
	{0x4024, 0x07},  // Anchor right start H
	{0x4025, 0x40},  // Andhor right start L
	{0x4026, 0x07},  // Anchor right end H
	{0x4027, 0x50},  // Anchor right end L
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4502, 0x40},  // ADC sync control
	{0x4601, 0x77},  // V fifo read start
	{0x5050, 0x0c},

	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov4689_1792_1168_profile_2[] = {
#ifdef OV4689_CAMERA_RAW_8
	{0x3031, 0x08},
#else
	{0x3031, 0x0a},
#endif
	{0x3632, 0x00}, // ADC & Analog
	{0x374a, 0x43},
	{0x376b, 0x20}, // Sensor control
	{0x3800, 0x00}, // H crop start H
	{0x3801, 0x08}, // H crop start L
	{0x3803, 0x04}, // V crop start L
	{0x3804, 0x0a}, // H crop end H
	{0x3805, 0x97}, // H crop end L
	{0x3807, 0xfb}, // V crop end L
	{0x3808, 0x07}, // H output size H
	{0x3809, 0x00}, // H output size L
	{0x380a, 0x04}, // V output size H
	{0x380b, 0x90}, // V output size L
	{0x380c, 0x0a}, // HTS H
	{0x380d, 0x10}, // HTS L
	{0x380e, 0x06}, // VTS H
	{0x380f, 0xb9}, // VTS L
	{0x3811, 0x08}, // H win off L
	{0x3813, 0x04}, // V win off L
	{0x3814, 0x01}, // H inc odd
	{0x3820, 0x06}, // flip off, bin off
	{0x3821, 0x00}, // mirror on, bin off
	{0x382a, 0x01}, // vertical subsample odd increase number
	{0x3830, 0x04}, // blc use num/2
	{0x3836, 0x01}, // r zline use num/2
	{0x4001, 0x40}, // debug mode
	{0x4003, 0x14},
	{0x4022, 0x07}, // Anchor left end H
	{0x4023, 0xcf}, // Anchor left end L
	{0x4024, 0x09}, // Anchor right start H
	{0x4025, 0x60}, // Andhor right start L
	{0x4026, 0x09}, // Anchor right end H
	{0x4027, 0x6f}, // Anchor right end L
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4502, 0x40}, // ADC sync control
	{0x4601, 0x04}, // V fifo read start
	{0x5050, 0x0c},

	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov4689_1280_720_profile_2[] = {

#ifdef OV4689_CAMERA_RAW_8
	{0x3031, 0x08},
#else
	{0x3031, 0x0a},
#endif
	{0x3632, 0x05}, // ADC & Analog
	{0x374a, 0x43},
	{0x376b, 0x40}, // Sensor control
	{0x3800, 0x00}, // H crop start H
	{0x3801, 0x48}, // H crop start L
	{0x3803, 0x2c}, // V crop start L
	{0x3804, 0x0a}, // H crop end H
	{0x3805, 0x57}, // H crop end L
	{0x3807, 0xd3}, // V crop end L
	{0x3808, 0x05}, // H output size H
	{0x3809, 0x00}, // H output size L
	{0x380a, 0x02}, // V output size H
	{0x380b, 0xd0}, // V output size L
	{0x380c, 0x0a}, // HTS H
	{0x380d, 0x10}, // HTS L
	{0x380e, 0x06}, // VTS H
	{0x380f, 0x12}, // VTS L
	{0x3811, 0x04}, // H win off L
	{0x3813, 0x02}, // V win off L
	{0x3814, 0x03}, // H inc odd
	{0x3820, 0x16}, // flip off, bin on
	{0x3821, 0x01}, // mirror on, bin on
	{0x382a, 0x03}, // vertical subsample odd increase number
	{0x3830, 0x08}, // blc use num/2
	{0x3836, 0x02}, // r zline use num/2
	{0x4001, 0x50}, // debug mode
	{0x4003, 0x14},
	{0x4022, 0x03}, // Anchor left end H
	{0x4023, 0x93}, // Anchor left end L
	{0x4024, 0x04}, // Anchor right start H
	{0x4025, 0xc0}, // Andhor right start L
	{0x4026, 0x04}, // Anchor right end H
	{0x4027, 0xd0}, // Anchor right end L
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4502, 0x44}, // ADC sync control
	{0x4601, 0x4f}, // V fifo read start
	{0x5050, 0x3c},

	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov4689_1280_720_lowfps_profile_2[] = {

#ifdef OV4689_CAMERA_RAW_8
	{0x3031, 0x08},
#else
	{0x3031, 0x0a},
#endif
	{0x3632, 0x05}, // ADC & Analog
	{0x374a, 0x43},
	{0x376b, 0x40}, // Sensor control
	{0x3800, 0x00}, // H crop start H
	{0x3801, 0x48}, // H crop start L
	{0x3803, 0x2c}, // V crop start L
	{0x3804, 0x0a}, // H crop end H
	{0x3805, 0x57}, // H crop end L
	{0x3807, 0xd3}, // V crop end L
	{0x3808, 0x05}, // H output size H
	{0x3809, 0x00}, // H output size L
	{0x380a, 0x02}, // V output size H
	{0x380b, 0xd0}, // V output size L
	{0x380c, 0x0a}, // HTS H
	{0x380d, 0x10}, // HTS L
	{0x380e, 0x0c}, // VTS H
	{0x380f, 0x20}, // VTS L
	{0x3811, 0x04}, // H win off L
	{0x3813, 0x02}, // V win off L
	{0x3814, 0x03}, // H inc odd
	{0x3820, 0x16}, // flip off, bin on
	{0x3821, 0x01}, // mirror on, bin on
	{0x382a, 0x03}, // vertical subsample odd increase number
	{0x3830, 0x08}, // blc use num/2
	{0x3836, 0x02}, // r zline use num/2
	{0x4001, 0x50}, // debug mode
	{0x4003, 0x14},
	{0x4022, 0x03}, // Anchor left end H
	{0x4023, 0x93}, // Anchor left end L
	{0x4024, 0x04}, // Anchor right start H
	{0x4025, 0xc0}, // Andhor right start L
	{0x4026, 0x04}, // Anchor right end H
	{0x4027, 0xd0}, // Anchor right end L
	{0x402a, 0x06},
	{0x402b, 0x04},
	{0x402e, 0x0e},
	{0x402f, 0x04},
	{0x4502, 0x44}, // ADC sync control
	{0x4601, 0x4f}, // V fifo read start
	{0x5050, 0x3c},

	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};
#endif

static struct regval_list ov4689_softreset[] = {
	{0x0103, 0x01},

	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov4689_stream_on[] = {
	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov4689_stream_off[] = {
	/* {0x4202, 0x0f}, */
	{0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

#if defined  OV4689_SETTING_PROFILE_2
static struct sensor_win_setting ov4689_1280_720_lowfps_win = {
	.width		= 1280,
	.height		= 720,
#ifdef OV4689_CAMERA_RAW_8
	.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
	.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.regs 		= ov4689_1280_720_lowfps_profile_2,
	.lunch_regs 	= NULL,
	.reg_sum        = ARRAY_SIZE(ov4689_1280_720_lowfps_profile_2),
	.sysclk         = 120,
	.low_fps_win    = NULL,
	.vts            = 0xc20,
	.hts            = 0xa10,
};
#endif
static struct sensor_win_setting ov4689_win_sizes[] = {
#if defined  OV4689_SETTING_PROFILE_2
	{
		.width		= 2688,
		.height		= 1520,
#ifdef OV4689_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov4689_2688_1520_profile_2,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov4689_2688_1520_profile_2),
		.sysclk         = 120,
		.low_fps_win    = NULL,
		.vts            = 0x6b9,
		.hts            = 0xa10,
	},
/* 	{ */
/* 		.width		= 1792, */
/* 		.height		= 1168, */
/* #ifdef OV4689_CAMERA_RAW_8 */
/* 		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8, */
/* #else */
/* 		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10, */
/* #endif */
/* 		.colorspace	= V4L2_COLORSPACE_SRGB, */
/* 		.regs 		= ov4689_1792_1168_profile_2, */
/* 		.lunch_regs 	= NULL, */
/* 		.reg_sum        = ARRAY_SIZE(ov4689_1792_1168_profile_2), */
/* 		.sysclk         = 120, */
/* 		.low_fps_win    = NULL, */
/* 		.vts            = 0x6b9, */
/* 		.hts            = 0xa10, */
/* 	}, */
	{
		.width		= 1280,
		.height		= 720,
#ifdef OV4689_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov4689_1280_720_profile_2,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov4689_1280_720_profile_2),
		.sysclk         = 120,
		.low_fps_win    = &ov4689_1280_720_lowfps_win,
		.vts            = 0x612,
		.hts            = 0xa10,
	},
#endif
};

#define OV4689_N_WIN_SIZES (ARRAY_SIZE(ov4689_win_sizes))

static struct sensor_format_struct ov4689_formats[] = {
	{
#ifdef OV4689_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
	},
	{
#ifdef OV4689_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_YUYV10_2X10,
#endif
		.colorspace	 = V4L2_COLORSPACE_BT878,
	}
};

#define N_OV4689_FMTS ARRAY_SIZE(ov4689_formats)

int ov4689_read(struct v4l2_subdev *sd, unsigned short reg, unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret = -1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0) {
		ret = 0;
	}
	return ret;
}

int ov4689_write(struct v4l2_subdev *sd, unsigned short reg, unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret = 0;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0) {
		ret = 0;
	}
	return ret;
}

static int ov4689_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ)) {
				msleep(vals->value);
			} else {
				mdelay(vals->value);
			}
		} else {
			ret = ov4689_read(sd, vals->reg_num, &val);
			if (ret < 0) {
				return ret;
			}
		}
		vals++;
	}
	ov4689_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov4689_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ)) {
				msleep(vals->value);
			} else {
				mdelay(vals->value);
			}
		} else {
			ret = ov4689_write(sd, vals->reg_num, vals->value);
			if (ret < 0) {
				return ret;
			}
		}
		vals++;
	}
	return 0;
}


static int ov4689_s_power(struct v4l2_subdev *sd, int on)
{
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	info->write_mode = OV4689_DEFAULT_REG_WRTIE_MODE;

	return 0;
}

static int ov4689_reset(struct v4l2_subdev *sd, u32 val)
{
#if 0
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	ret = ov4689_write_array(sd, ov4689_softreset);
	if (ret < 0) {
		SENSOR_PRINT(CAMERA_WARNING,"ov4689 failed to reset, return %d\n",ret);
		return ret;
	}
#endif
	return 0;
}

static int ov4689_init(struct v4l2_subdev *sd, u32 val)
{
	int ret = -1;

	ret = ov4689_write_array(sd, ov4689_softreset);
	if (ret < 0) {
		SENSOR_PRINT(CAMERA_ERROR, "ov4689 failed to reset, return %d\n",ret);
		return ret;
	}
	/* chose init setting */
	ret = ov4689_write_array(sd, ov4689_init_default);
	if (ret < 0) {
		SENSOR_PRINT(CAMERA_ERROR, "ov4689 failed to set common reg value, return %d\n",ret);
		return ret;
	}
	return 0;
}

static int ov4689_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index, enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV4689_FMTS)
		return -EINVAL;

	*code = ov4689_formats[index].mbus_code;
	return 0;
}

static int ov4689_try_fmt_internal(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt,
				   struct sensor_win_setting **ret_wsize)
{
	struct sensor_win_setting *wsize;

	if (fmt->width > OV4689_MAX_WIDTH || fmt->height > OV4689_MAX_HEIGHT)
		return -EINVAL;
	for (wsize = ov4689_win_sizes; wsize < ov4689_win_sizes + OV4689_N_WIN_SIZES; wsize++)
		if (fmt->width > wsize->width && fmt->height > wsize->height)
			break;
	wsize--;   /* Take the smallest one */
	if (fmt->priv == V4L2_CAP_LOWFPS_MODE && wsize->low_fps_win != NULL) {
		wsize = wsize->low_fps_win;
	}
	if ((!(fmt->priv & V4L2_CAP_QUITAK_MODE)) && (wsize->width == 800 && wsize->height == 600)
	    && (fmt->width != 0 && fmt->height != 0)) {
		wsize--;
	}
	if (ret_wsize != NULL) {
		*ret_wsize = wsize;
	}
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->code = wsize->mbus_code;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = wsize->colorspace;
	return 0;
}

static int ov4689_g_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	data->i2cflags = V4L2_I2C_ADDR_16BIT;
	data->sensor_sys_pclk = info->trying_win->sysclk;
	data->vts = info->trying_win->vts;
	data->hts = info->trying_win->hts;
	data->lans = 2;

	return 0;
}

static int ov4689_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	ret = ov4689_try_fmt_internal(sd, fmt, &info->trying_win);
	SENSOR_PRINT(CAMERA_INFO, "info->trying_win->width = %d, info->trying_win->height = %d,\n", 
		     info->trying_win->width, info->trying_win->height);
	return ret;
}

static int ov4689_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	int ret = -1;
	if (info->using_win != info->trying_win) {
		if (info->write_mode == GROUP_LUNCH) {
			ret = ov4689_write_array(sd, info->trying_win->lunch_regs);
		} else {
			ret = ov4689_write_array(sd, info->trying_win->regs);
		}
		if (ret)
			return ret;
	}
	info->using_win = info->trying_win;
	ov4689_g_mbus_fmt(sd, fmt);

	return 0;
}

static int ov4689_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	if (enable) {
		ret = ov4689_write_array(sd, ov4689_stream_on);
		if (ret < 0) {
			SENSOR_PRINT(CAMERA_ERROR,"ov4689 stream on failed ret = %d\n",ret);
		} else {
			SENSOR_PRINT(CAMERA_INFO,"ov4689 stream on\n");
		}
	} else {
		ret = ov4689_write_array(sd, ov4689_stream_off);
		if (ret < 0) {
			SENSOR_PRINT(CAMERA_ERROR,"ov4689 stream off failed ret = %d\n",ret);
		} else {
			SENSOR_PRINT(CAMERA_INFO,"ov4689 stream off\n");
		}
	}
	return ret;
}

static int ov4689_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov4689_enum_frameintervals(struct v4l2_subdev *sd, struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov4689_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov4689_frame_rates[interval->index];
	return 0;
}

static int ov4689_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	__u32 index = fsize->index;
	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	if (index < OV4689_N_WIN_SIZES) {
		struct sensor_win_setting *win = &ov4689_win_sizes[index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = win->width;
		fsize->discrete.height = win->height;
		return 0;
	} else {
		return -EINVAL;
	}

	return -EINVAL;
}

static int ov4689_write_testself_mode(struct v4l2_subdev *sd, struct regval_list *vals,
				      unsigned char config_value)
{
	int ret = -1;
	while (vals->reg_num !=SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_TESTMODE) {
			ret = ov4689_write(sd,vals->reg_num,config_value);
			SENSOR_PRINT(CAMERA_INFO,"config_value is %d\n",config_value);
			if (ret < 0)
				return -1;
			ov4689_read(sd, vals->reg_num, &config_value);
			SENSOR_PRINT(CAMERA_INFO," This is %d\n",config_value);
		}
		vals++;
	}
	return 0;
}

static int ov4689_s_ctrl_1(struct v4l2_subdev *sd, struct v4l2_control *ctrl, unsigned char select_mode)
{
        int ret = -1;
        if (select_mode == TURN_OVSENSOR_TESTMODE) {
		ret = ov4689_write_testself_mode(sd,ov4689_init_default,SENSOR_TESTMODE_VALUE);
		if (ret < 0) {
			SENSOR_PRINT(CAMERA_ERROR,"functiong:%s, line:%d failed\n", __func__, __LINE__);
			return ret;
		}
        }
        if (select_mode == TURN_OVSENSOR_NORMALMODE) {
		ret = ov4689_write_testself_mode(sd,ov4689_init_default,SENSOR_NORMALMODE_VALUE);
		if (ret < 0) {
			SENSOR_PRINT(CAMERA_ERROR, "functiong:%s, line:%d failed\n", __func__, __LINE__);
			return ret;
		}
	}
	return 0;
}

static int ov4689_soft_reset(struct v4l2_subdev *sd, int cmd)
{
	int ret = -1;

	ret = ov4689_write_array(sd, ov4689_softreset);
	if (ret < 0) {
		goto err;
	}
	ret = ov4689_write_array(sd, ov4689_init_default);
	if (ret < 0) {
		goto err;
	}
	if (cmd == 0x02 | cmd == 0x06) {
		ret = ov4689_write_array(sd, ov4689_1280_720_profile_2);
	} else if (cmd == 0x04) {
		ret = ov4689_write_array(sd, ov4689_2688_1520_profile_2);
	} else {
		SENSOR_PRINT(CAMERA_ERROR, "%s : don't support command %d\n", __func__, cmd);
		ret = -1;
		goto err;
	}
	if (ret < 0) {
		goto err;
	}

#ifndef OV4689_SETTING_PROFILE_2
	ret = ov4689_write_array(sd, ov4689_stream_on);
#endif

 err:
	if (ret < 0) {
		SENSOR_PRINT(CAMERA_ERROR, "%s : reset Senor failed!!!");
	}
	return ret;
}
