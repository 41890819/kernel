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

#define OV8865_MAX_WIDTH		3264
#define OV8865_MAX_HEIGHT		2448

#define OV8865_RG_Ratio_Typical        0x10F
#define OV8865_BG_Ratio_Typical        0x161

/* #define OV8865_CAMERA_RAW_8 */

#define OV8865_DEFAULT_REG_WRTIE_MODE   ISP_CMD_GROUP_WRITE
/* #define OV8865_DEFAULT_REG_WRTIE_MODE   ISP_GROUP_WRITE */

static struct regval_list ov8865_init_default_regs[] = {

	// Initial to Raw 10bit 1632x1224 30fps 4lane 720M bps/lane
	// XVCLK=24Mhz, sysclk=72Mhz
	// MIPI 2 lane, 720Mbps/lane
	{0x3638, 0xff}, // analog control
	{0x0302, 0x1e}, // PLL
	{0x0303, 0x00}, // PLL
	{0x0304, 0x03}, // PLL
	{0x030e, 0x00}, // PLL
	{0x030f, 0x09}, // PLL
	{0x0312, 0x01}, // PLL
	{0x031e, 0x0c}, // PLL
	{0x3015, 0x01}, // clock Div
	//{0x3018, 0x72}, // MIPI 4 lane
	{0x3020, 0x93}, // clock normal, pclk/1
	{0x3022, 0x01}, // pd_mini enable when rst_sync

#ifdef OV8865_CAMERA_RAW_8
	{0x3031, 0x08}, //8-bit
#else
	{0x3031, 0x0a}, //10-bit
#endif
	{0x3106, 0x01},   // PLL
	{0x3305, 0xf1},
	{0x3308, 0x00},
	{0x3309, 0x28},
	{0x330a, 0x00},
	{0x330b, 0x20},
	{0x330c, 0x00},
	{0x330d, 0x00},
	{0x330e, 0x00},
	{0x330f, 0x40},
	{0x3307, 0x04},
	{0x3604, 0x04}, // analog control
	{0x3602, 0x30},
	{0x3605, 0x00},
	{0x3607, 0x20},
	{0x3608, 0x11},
	{0x3609, 0x68},
	{0x360a, 0x40},
	{0x360c, 0xdd},
	{0x360e, 0x0c},
	{0x3610, 0x07},
	{0x3612, 0x86},
	{0x3613, 0x58},
	{0x3614, 0x28},
	{0x3617, 0x40},
	{0x3618, 0x5a},
	{0x3619, 0x9b},
	{0x361c, 0x00},
	{0x361d, 0x60},
	{0x3631, 0x60},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3641, 0x55}, // MIPI settings
	{0x3646, 0x86}, // MIPI settings
	{0x3647, 0x27}, // MIPI settings
	{0x364a, 0x1b}, // MIPI settings
	/* {0x3500, 0x00}, // exposurre HH */
	/* {0x3501, 0x4c}, // expouere H */
	/* {0x3502, 0x00}, // exposure L */
	{0x3503, 0x80}, // gain no delay, exposure no delay
	/* {0x3508, 0x02}, // gain H */
	/* {0x3509, 0x00}, // gain L */
	{0x3700, 0x24}, // sensor control
	{0x3701, 0x0c},
	{0x3702, 0x28},
	{0x3703, 0x19},
	{0x3704, 0x14},
	{0x3705, 0x00},
	{0x3706, 0x38},
	{0x3707, 0x04},
	{0x3708, 0x24},
	{0x3709, 0x40},
	{0x370a, 0x00},
	{0x370b, 0xb8},
	{0x370c, 0x04},
	{0x3718, 0x12},
	{0x3719, 0x31},
	{0x3712, 0x42},
	{0x3714, 0x12},
	{0x371e, 0x19},
	{0x371f, 0x40},
	{0x3720, 0x05},
	{0x3721, 0x05},
	{0x3724, 0x02},
	{0x3725, 0x02},
	{0x3726, 0x06},
	{0x3728, 0x05},
	{0x3729, 0x02},
	{0x372a, 0x03},
	{0x372b, 0x53},
	{0x372c, 0xa3},
	{0x372d, 0x53},
	{0x372e, 0x06},
	{0x372f, 0x10},
	{0x3730, 0x01},
	{0x3731, 0x06},
	{0x3732, 0x14},
	{0x3733, 0x10},
	{0x3734, 0x40},
	{0x3736, 0x20},
	{0x373a, 0x02},
	{0x373b, 0x0c},
	{0x373c, 0x0a},
	{0x373e, 0x03},
	{0x3755, 0x40},
	{0x3758, 0x00},
	{0x3759, 0x4c},
	{0x375a, 0x06},
	{0x375b, 0x13},
	{0x375c, 0x40},
	{0x375d, 0x02},
	{0x375e, 0x00},
	{0x375f, 0x14},
	{0x3767, 0x1c},
	{0x3768, 0x04},
	{0x3769, 0x20},
	{0x376c, 0xc0},
	{0x376d, 0xc0},
	{0x376a, 0x08},
	{0x3761, 0x00},
	{0x3762, 0x00},
	{0x3763, 0x00},
	{0x3766, 0xff},
	{0x376b, 0x42},
	{0x3772, 0x23},
	{0x3773, 0x02},
	{0x3774, 0x16},
	{0x3775, 0x12},
	{0x3776, 0x08},
	{0x37a0, 0x44},
	{0x37a1, 0x3d},
	{0x37a2, 0x3d},
	{0x37a3, 0x01},
	{0x37a4, 0x00},
	{0x37a5, 0x08},
	{0x37a6, 0x00},
	{0x37a7, 0x44},
	{0x37a8, 0x58},
	{0x37a9, 0x58},
	{0x3760, 0x00},
	{0x376f, 0x01},
	{0x37aa, 0x44},
	{0x37ab, 0x2e},
	{0x37ac, 0x2e},
	{0x37ad, 0x33},
	{0x37ae, 0x0d},
	{0x37af, 0x0d},
	{0x37b0, 0x00},
	{0x37b1, 0x00},
	{0x37b2, 0x00},
	{0x37b3, 0x42},
	{0x37b4, 0x42},
	{0x37b5, 0x33},
	{0x37b6, 0x00},
	{0x37b7, 0x00},
	{0x37b8, 0x00},
	{0x37b9, 0xff}, // sensor control
	{0x3800, 0x00}, // X start H
	{0x3801, 0x0c}, // X start L
	{0x3802, 0x00}, // Y start H
	{0x3803, 0x0c}, // Y start L
	{0x3804, 0x0c}, // X end H
	{0x3805, 0xd3}, // X end L
	{0x3806, 0x09}, // Y end H
	{0x3807, 0xa3}, // Y end L
	{0x3808, 0x06}, // X output size H
	{0x3809, 0x60}, // X output size L
	{0x380a, 0x04}, // Y output size H
	{0x380b, 0xc8}, // Y output size L
	//{0x380c, 0x07}, // HTS H
	//{0x380d, 0x83}, // HTS L
	//{0x380e, 0x04}, // VTS H
	//{0x380f, 0xe0}, // VTS L
	{0x3810, 0x00}, // ISP X win H
	{0x3811, 0x04}, // ISP X win L
	//{0x3813, 0x04}, // ISP Y win L
	{0x3814, 0x03}, // X inc odd
	{0x3815, 0x01}, // X inc even
	{0x3820, 0x00}, // flip off
	{0x3821, 0x67}, // hsync_en_o, fst_vbin, mirror on
	{0x382a, 0x03}, // Y inc odd
	{0x382b, 0x01}, // Y inc even
	{0x3830, 0x08}, // ablc_use_num[5:1]
	{0x3836, 0x02}, // zline_use_num[5:1]
	{0x3837, 0x18}, // vts_add_dis, cexp_gt_vts_offs=8
	{0x3841, 0xff}, // auto size
	{0x3846, 0x88}, // Y/X boundary pixel numbber for auto size mode
	{0x3d85, 0x06}, // OTP power up load data enable, OTP power up load setting enable
	{0x3d8c, 0x75}, // OTP setting start address H
	{0x3d8d, 0xef}, // OTP setting start address L
	{0x3f08, 0x0b}, 
	{0x4000, 0xf1}, // our range trig en, format chg en, gan chg en, exp chg en, median en
	{0x4001, 0x14}, // left 32 column, final BLC offset limitation enable
	{0x4005, 0x10}, // BLC target
	{0x400b, 0x0c}, // start line =0, offset limitation en, cut range function en
	{0x400d, 0x10}, // offset trigger threshold
	{0x401b, 0x00}, 
	{0x401d, 0x00}, 
	{0x4020, 0x01}, // anchor left start H
	{0x4021, 0x20}, // anchor left start L
	{0x4022, 0x01}, // anchor left end H
	{0x4023, 0x9f}, // anchor left end L
	{0x4024, 0x03}, // anchor right start H
	{0x4025, 0xe0}, // anchor right start L
	{0x4026, 0x04}, // anchor right end H
	{0x4027, 0x5f}, // anchor right end L
	{0x4028, 0x00}, // top zero line start
	{0x4029, 0x02}, // top zero line number
	{0x402a, 0x04}, // top black line start
	{0x402b, 0x04}, // top black line number
	{0x402c, 0x02}, // bottom zero line start
	{0x402d, 0x02}, // bottom zero line number
	{0x402e, 0x08}, // bottom black line start
	{0x402f, 0x02}, // bottom black line number
	{0x401f, 0x00}, // anchor one disable
	{0x4034, 0x3f}, // limitation BLC offset
	{0x4300, 0xff}, // clip max H
	{0x4301, 0x00}, // clip min H
	{0x4302, 0x0f}, // clip min L/clip max L
	{0x4500, 0x40}, // ADC sync control
	{0x4503, 0x10}, 
	{0x4601, 0x74}, // V FIFO control
	{0x481f, 0x32}, // clk_prepare_min
	{0x4825, 0x4a}, // it's so important, MIPI信号LP01的时间长度,最小50ns,现在的0x32就是50ns,0x3a对应58ns
	{0x4837, 0x16}, // clock period
	{0x4850, 0x10}, // lane select
	{0x4851, 0x32}, // lane select
	{0x4b00, 0x2a}, // LVDS settings
	{0x4b0d, 0x00}, // LVDS settings
	{0x4d00, 0x04}, // temperature sensor
	{0x4d01, 0x18}, // temperature sensor
	{0x4d02, 0xc3}, // temperature sensor
	{0x4d03, 0xff}, // temperature sensor
	{0x4d04, 0xff}, // temperature sensor
	{0x4d05, 0xff}, // temperature sensor
	{0x5000, 0x96}, // LENC on, MWB on, BPC on, WPC on
	{0x5001, 0x01}, // BLC on
	{0x5002, 0x08}, // vario pixel off
	{0x5901, 0x00}, 
	{0x5e00, 0x00}, // test pattern off
	{0x5e01, 0x41}, // window cut enable
	{0x5b00, 0x02}, // OTP DPC start address H
	{0x5b01, 0xd0}, // OTP DPC start address L
	{0x5b02, 0x03}, // OTP DPC end address H
	{0x5b03, 0xff}, // OTP DPC end address L
	{0x5b05, 0x6c}, // Recover method 11, use 0x3ff to recover cluster, flip option enable
	{0x5780, 0xfc}, // DPC
	{0x5781, 0xdf},
	{0x5782, 0x3f},
	{0x5783, 0x08},
	{0x5784, 0x0c},
	{0x5786, 0x20},
	{0x5787, 0x40},
	{0x5788, 0x08},
	{0x5789, 0x08},
	{0x578a, 0x02},
	{0x578b, 0x01},
	{0x578c, 0x01},
	{0x578d, 0x0c},
	{0x578e, 0x02},
	{0x578f, 0x01},
	{0x5790, 0x01}, // DPC
	{0x5800, 0x1d}, // lens correction
	{0x5801, 0x0e}, 
	{0x5802, 0x0c}, 
	{0x5803, 0x0c}, 
	{0x5804, 0x0f}, 
	{0x5805, 0x22}, 
	{0x5806, 0x0a}, 
	{0x5807, 0x06}, 
	{0x5808, 0x05}, 
	{0x5809, 0x05}, 
	{0x580a, 0x07}, 
	{0x580b, 0x0a}, 
	{0x580c, 0x06}, 
	{0x580d, 0x02}, 
	{0x580e, 0x00}, 
	{0x580f, 0x00}, 
	{0x5810, 0x03}, 
	{0x5811, 0x07}, 
	{0x5812, 0x06}, 
	{0x5813, 0x02}, 
	{0x5814, 0x00}, 
	{0x5815, 0x00}, 
	{0x5816, 0x03}, 
	{0x5817, 0x07}, 
	{0x5818, 0x09}, 
	{0x5819, 0x06}, 
	{0x581a, 0x04}, 
	{0x581b, 0x04}, 
	{0x581c, 0x06}, 
	{0x581d, 0x0a}, 
	{0x581e, 0x19}, 
	{0x581f, 0x0d}, 
	{0x5820, 0x0b}, 
	{0x5821, 0x0b}, 
	{0x5822, 0x0e}, 
	{0x5823, 0x22}, 
	{0x5824, 0x23}, 
	{0x5825, 0x28}, 
	{0x5826, 0x29}, 
	{0x5827, 0x27}, 
	{0x5828, 0x13}, 
	{0x5829, 0x26}, 
	{0x582a, 0x33}, 
	{0x582b, 0x32}, 
	{0x582c, 0x33}, 
	{0x582d, 0x16}, 
	{0x582e, 0x14}, 
	{0x582f, 0x30}, 
	{0x5830, 0x31}, 
	{0x5831, 0x30}, 
	{0x5832, 0x15}, 
	{0x5833, 0x26}, 
	{0x5834, 0x23}, 
	{0x5835, 0x21}, 
	{0x5836, 0x23}, 
	{0x5837, 0x05}, 
	{0x5838, 0x36}, 
	{0x5839, 0x27}, 
	{0x583a, 0x28}, 
	{0x583b, 0x26}, 
	{0x583c, 0x24}, 
	{0x583d, 0xdf}, // lens correction

	{SENSOR_REG_END, 0x00},	/* END MARKER */

};

static struct regval_list ov8865_init_3264_2448_regs[] = {

	// Raw 10bit 3264x2448 15fps 2lane 720M bps/lane
	// XVCLK=24Mhz, sysclk=72Mhz
	// MIPI 4 lane, 720Mbps/lane

	{0x030f, 0x04},    // PLL
	{0x3018, 0x32},
	{0x3106, 0x21},
	/* {0x3501, 0x98},    //expouere H */
	/* {0x3502, 0x60},    //exposure L */
	/* {0x3700, 0x24},    //sensor control */
	/* {0x3701, 0x0c}, */
	/* {0x3702, 0x28}, */
	/* {0x3703, 0x19}, */
	/* {0x3704, 0x14}, */
	/* {0x3706, 0x38}, */
	/* {0x3707, 0x04}, */
	/* {0x3708, 0x24}, */
	/* {0x3709, 0x40}, */
	/* {0x370a, 0x00}, */
	/* {0x370b, 0xb8}, */
	/* {0x370c, 0x04}, */
	/* {0x3718, 0x12}, */
	/* {0x3712, 0x42}, */
	/* {0x371e, 0x19}, */
	/* {0x371f, 0x40}, */
	/* {0x3720, 0x05}, */
	/* {0x3721, 0x05}, */
	/* {0x3724, 0x02}, */
	/* {0x3725, 0x02}, */
	/* {0x3726, 0x06}, */
	/* {0x3728, 0x05}, */
	/* {0x3729, 0x02}, */
	/* {0x372a, 0x03}, */
	/* {0x372b, 0x53}, */
	/* {0x372c, 0xa3}, */
	/* {0x372d, 0x53}, */
	/* {0x372e, 0x06}, */
	/* {0x372f, 0x10}, */
	/* {0x3730, 0x01}, */
	/* {0x3731, 0x06}, */
	/* {0x3732, 0x14}, */
	/* {0x3736, 0x20}, */
	/* {0x373a, 0x02}, */
	/* {0x373b, 0x0c}, */
	/* {0x373c, 0x0a}, */
	/* {0x373e, 0x03}, */
	/* {0x375a, 0x06}, */
	/* {0x375b, 0x13}, */
	/* {0x375d, 0x02}, */
	/* {0x375f, 0x14}, */
	{0x3767, 0x1e},
	/* {0x3772, 0x23}, */
	/* {0x3773, 0x02}, */
	/* {0x3774, 0x16}, */
	/* {0x3775, 0x12}, */
	/* {0x3776, 0x08}, */
	/* {0x37a0, 0x44}, */
	/* {0x37a1, 0x3d}, */
	/* {0x37a2, 0x3d}, */
	{0x37a3, 0x02},
	{0x37a5, 0x09},
	/* {0x37a7, 0x44}, */
	/* {0x37a8, 0x58}, */
	/* {0x37a9, 0x58}, */
	/* {0x37aa, 0x44}, */
	/* {0x37ab, 0x2e}, */
	/* {0x37ac, 0x2e}, */
	/* {0x37ad, 0x33}, */
	/* {0x37ae, 0x0d}, */
	/* {0x37af, 0x0d}, */
	/* {0x37b3, 0x42}, */
	/* {0x37b4, 0x42}, */
	/* {0x37b5, 0x33}, */
	{0x3808, 0x0c},     //X output size H
	{0x3809, 0xc0},     //X output size L
	{0x380a, 0x09},     //Y output size H
	{0x380b, 0x90},     //Y output size L
	{0x380c, 0x07},     //HTS H
	{0x380d, 0x40},     //HTS L
	{0x380e, 0x09},     //VTS H
	{0x380f, 0xa6},     //VTS L
	{0x3813, 0x02},     //ISP Y win L
	{0x3814, 0x01},     //X inc odd
	{0x3821, 0x46},     //hsync_en_o, fst_vbin, mirror on
	{0x382a, 0x01},     //Y inc odd
	{0x382b, 0x01},     //Y inc even
	{0x3830, 0x04},     //ablc_use_num[5:1]
	{0x3836, 0x01},     //zline_use_num[5:1]
	{0x3846, 0x48},     //Y/X boundary pixel numbber for auto size mode
	{0x3f08, 0x0b},
	{0x4000, 0xf1},     //our range trig en, format chg en, gan chg en, exp chg en, median en
	{0x4001, 0x04},     //left 32 column, final BLC offset limitation enable */
	{0x4020, 0x02},     //anchor left start H
	{0x4021, 0x40},     //anchor left start L
	{0x4022, 0x03},     //anchor left end H
	{0x4023, 0x3f},     //anchor left end L
	{0x4024, 0x07},     //anchor right start H
	{0x4025, 0xc0},     //anchor right start L
	{0x4026, 0x08},     //anchor right end H
	{0x4027, 0xbf},     //anchor right end L
	{0x402a, 0x04},     //top black line start
	{0x402b, 0x04},     //top black line number
	{0x402c, 0x02},     //bottom zero line start
	{0x402d, 0x02},     //bottom zero line number
	{0x402e, 0x08},     //bottom black line start
	{0x4500, 0x68},     //ADC sync control
	{0x4601, 0x10},     //V FIFO control , if use this ,image is error.
	/* {0x4601, 0x74},	   // V FIFO control */

	{0x5002, 0x08},     //vario pixel off
	{0x5901, 0x00},

	{0x4202, 0x00},	 // MIPI stream on when new frame starts
	{0x0100, 0x01},	 // Sensor wake up from standby, MIPI stream on

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_init_1600_1200_regs[] = {

	//Raw 8bit 1600x1200 30fps 2lane 720M bps/lane
	// XVCLK=24Mhz, sysclk=72Mhz
	// MIPI 2 lane, 720Mbps/lane
	{0x030f, 0x09},   // PLL
	{0x3018, 0x32},   // MIPI 2 lane
	{0x3106, 0x01},
	/* {0x3501, 0x4c},   // expouere H */
	/* {0x3502, 0x00},   // exposure L */
	/* {0x3700, 0x24},   // sensor control */
	/* {0x3701, 0x0c}, */
	/* {0x3702, 0x28}, */
	/* {0x3703, 0x19}, */
	/* {0x3704, 0x14}, */
	/* {0x3706, 0x38}, */
	/* {0x3707, 0x04}, */
	/* {0x3708, 0x24}, */
	/* {0x3709, 0x40}, */
	/* {0x370a, 0x00}, */
	/* {0x370b, 0xb8}, */
	/* {0x370c, 0x04}, */
	/* {0x3718, 0x12}, */
	/* {0x3712, 0x42}, */
	/* {0x371e, 0x19}, */
	/* {0x371f, 0x40}, */
	/* {0x3720, 0x05}, */
	/* {0x3721, 0x05}, */
	/* {0x3724, 0x02}, */
	/* {0x3725, 0x02}, */
	/* {0x3726, 0x06}, */
	/* {0x3728, 0x05}, */
	/* {0x3729, 0x02}, */
	/* {0x372a, 0x03}, */
	/* {0x372b, 0x53}, */
	/* {0x372c, 0xa3}, */
	/* {0x372d, 0x53}, */
	/* {0x372e, 0x06}, */
	/* {0x372f, 0x10}, */
	/* {0x3730, 0x01}, */
	/* {0x3731, 0x06}, */
	/* {0x3732, 0x14}, */
	/* {0x3736, 0x20}, */
	/* {0x373a, 0x02}, */
	/* {0x373b, 0x0c}, */
	/* {0x373c, 0x0a}, */
	/* {0x373e, 0x03}, */
	/* {0x375a, 0x06}, */
	/* {0x375b, 0x13}, */
	/* {0x375d, 0x02}, */
	/* {0x375f, 0x14}, */
	{0x3767, 0x1c},
	/* {0x3772, 0x23}, */
	/* {0x3773, 0x02}, */
	/* {0x3774, 0x16}, */
	/* {0x3775, 0x12}, */
	/* {0x3776, 0x08}, */
	/* {0x37a0, 0x44}, */
	/* {0x37a1, 0x3d}, */
	/* {0x37a2, 0x3d}, */
	{0x37a3, 0x01},
	{0x37a5, 0x08},
	/* {0x37a7, 0x44}, */
	/* {0x37a8, 0x58}, */
	/* {0x37a9, 0x58}, */
	/* {0x37aa, 0x44}, */
	/* {0x37ab, 0x2e}, */
	/* {0x37ac, 0x2e}, */
	/* {0x37ad, 0x33}, */
	/* {0x37ae, 0x0d}, */
	/* {0x37af, 0x0d}, */
	/* {0x37b3, 0x42}, */
	/* {0x37b4, 0x42}, */
	/* {0x37b5, 0x33}, */
	/* {0x3808, 0x06},    // X output size H */
	/* {0x3809, 0x60},    // X output size L */
	/* {0x380a, 0x04},    // Y output size H */
	/* {0x380b, 0xc8},    // Y output size L */
	{0x3808, 0x06},    // X output size H
	{0x3809, 0x40},    // X output size L
	{0x380a, 0x04},    // Y output size H
	{0x380b, 0xb0},    // Y output size L

	{0x380c, 0x07},    // HTS H
	{0x380d, 0x88},    // HTS L
	{0x380e, 0x06},    // VTS H
	{0x380f, 0x12},    // VTS L
	{0x3813, 0x04},    // ISP Y win L
	{0x3814, 0x03},    // X inc odd
	{0x3821, 0x67},    // hsync_en_o, fst_vbin, mirror on
	{0x382a, 0x03},    // Y inc odd
	{0x382b, 0x01},    // Y inc even
	{0x3830, 0x08},    // ablc_use_num[5:1]
	{0x3836, 0x02},    // zline_use_num[5:1]
	{0x3846, 0x88},    // Y/X boundary pixel numbber for auto size mode
	{0x3f08, 0x0b},
	{0x4000, 0xf1},    // our range trig en, format chg en, gan chg en, exp chg en, median en
	{0x4001, 0x14},    // left 32 column, final BLC offset limitation enable
	{0x4020, 0x01},    // anchor left start H
	{0x4021, 0x20},    // anchor left start L
	{0x4022, 0x01},    // anchor left end H
	{0x4023, 0x9f},    // anchor left end L
	{0x4024, 0x03},    // anchor right start H
	{0x4025, 0xe0},    // anchor right start L
	{0x4026, 0x04},    // anchor right end H
	{0x4027, 0x5f},    // anchor right end L
	{0x402a, 0x04},    // top black line start
	{0x402b, 0x04},    // top black line number
	{0x402c, 0x02},    // bottom zero line start
	{0x402d, 0x02},    // bottom zero line number
	{0x402e, 0x08},    // bottom black line start
	{0x4500, 0x40},    // ADC sync control

	{0x4601, 0x74},    // V FIFO control
	{0x5002, 0x08},    // vario pixel off
	{0x5901, 0x00},

	{0x4202, 0x00},	 // MIPI stream on when new frame starts
	{0x0100, 0x01},	 // Sensor wake up from standby, MIPI stream on

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_init_1600_1200_lowfps_regs[] = {

	//Raw 8bit 1600x1200 30fps 2lane 720M bps/lane
	// XVCLK=24Mhz, sysclk=72Mhz
	// MIPI 2 lane, 720Mbps/lane
	{0x030f, 0x09},   // PLL
	{0x3018, 0x32},   // MIPI 2 lane
	{0x3106, 0x01},
	/* {0x3501, 0x4c},   // expouere H */
	/* {0x3502, 0x00},   // exposure L */
	/* {0x3700, 0x24},   // sensor control */
	/* {0x3701, 0x0c}, */
	/* {0x3702, 0x28}, */
	/* {0x3703, 0x19}, */
	/* {0x3704, 0x14}, */
	/* {0x3706, 0x38}, */
	/* {0x3707, 0x04}, */
	/* {0x3708, 0x24}, */
	/* {0x3709, 0x40}, */
	/* {0x370a, 0x00}, */
	/* {0x370b, 0xb8}, */
	/* {0x370c, 0x04}, */
	/* {0x3718, 0x12}, */
	/* {0x3712, 0x42}, */
	/* {0x371e, 0x19}, */
	/* {0x371f, 0x40}, */
	/* {0x3720, 0x05}, */
	/* {0x3721, 0x05}, */
	/* {0x3724, 0x02}, */
	/* {0x3725, 0x02}, */
	/* {0x3726, 0x06}, */
	/* {0x3728, 0x05}, */
	/* {0x3729, 0x02}, */
	/* {0x372a, 0x03}, */
	/* {0x372b, 0x53}, */
	/* {0x372c, 0xa3}, */
	/* {0x372d, 0x53}, */
	/* {0x372e, 0x06}, */
	/* {0x372f, 0x10}, */
	/* {0x3730, 0x01}, */
	/* {0x3731, 0x06}, */
	/* {0x3732, 0x14}, */
	/* {0x3736, 0x20}, */
	/* {0x373a, 0x02}, */
	/* {0x373b, 0x0c}, */
	/* {0x373c, 0x0a}, */
	/* {0x373e, 0x03}, */
	/* {0x375a, 0x06}, */
	/* {0x375b, 0x13}, */
	/* {0x375d, 0x02}, */
	/* {0x375f, 0x14}, */
	{0x3767, 0x1c},
	/* {0x3772, 0x23}, */
	/* {0x3773, 0x02}, */
	/* {0x3774, 0x16}, */
	/* {0x3775, 0x12}, */
	/* {0x3776, 0x08}, */
	/* {0x37a0, 0x44}, */
	/* {0x37a1, 0x3d}, */
	/* {0x37a2, 0x3d}, */
	{0x37a3, 0x01},
	{0x37a5, 0x08},
	/* {0x37a7, 0x44}, */
	/* {0x37a8, 0x58}, */
	/* {0x37a9, 0x58}, */
	/* {0x37aa, 0x44}, */
	/* {0x37ab, 0x2e}, */
	/* {0x37ac, 0x2e}, */
	/* {0x37ad, 0x33}, */
	/* {0x37ae, 0x0d}, */
	/* {0x37af, 0x0d}, */
	/* {0x37b3, 0x42}, */
	/* {0x37b4, 0x42}, */
	/* {0x37b5, 0x33}, */
	/* {0x3808, 0x06},    // X output size H */
	/* {0x3809, 0x60},    // X output size L */
	/* {0x380a, 0x04},    // Y output size H */
	/* {0x380b, 0xc8},    // Y output size L */
	{0x3808, 0x06},    // X output size H
	{0x3809, 0x40},    // X output size L
	{0x380a, 0x04},    // Y output size H
	{0x380b, 0xb0},    // Y output size L

	{0x380c, 0x07},    // HTS H
	{0x380d, 0x88},    // HTS L
	{0x380e, 0x09},    // VTS H
	{0x380f, 0xb0},    // VTS L
	{0x3813, 0x04},    // ISP Y win L
	{0x3814, 0x03},    // X inc odd
	{0x3821, 0x67},    // hsync_en_o, fst_vbin, mirror on
	{0x382a, 0x03},    // Y inc odd
	{0x382b, 0x01},    // Y inc even
	{0x3830, 0x08},    // ablc_use_num[5:1]
	{0x3836, 0x02},    // zline_use_num[5:1]
	{0x3846, 0x88},    // Y/X boundary pixel numbber for auto size mode
	{0x3f08, 0x0b},
	{0x4000, 0xf1},    // our range trig en, format chg en, gan chg en, exp chg en, median en
	{0x4001, 0x14},    // left 32 column, final BLC offset limitation enable
	{0x4020, 0x01},    // anchor left start H
	{0x4021, 0x20},    // anchor left start L
	{0x4022, 0x01},    // anchor left end H
	{0x4023, 0x9f},    // anchor left end L
	{0x4024, 0x03},    // anchor right start H
	{0x4025, 0xe0},    // anchor right start L
	{0x4026, 0x04},    // anchor right end H
	{0x4027, 0x5f},    // anchor right end L
	{0x402a, 0x04},    // top black line start
	{0x402b, 0x04},    // top black line number
	{0x402c, 0x02},    // bottom zero line start
	{0x402d, 0x02},    // bottom zero line number
	{0x402e, 0x08},    // bottom black line start
	{0x4500, 0x40},    // ADC sync control

	{0x4601, 0x74},    // V FIFO control
	{0x5002, 0x08},    // vario pixel off
	{0x5901, 0x00},

	{0x4202, 0x00},	 // MIPI stream on when new frame starts
	{0x0100, 0x01},	 // Sensor wake up from standby, MIPI stream on

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_init_1280_720_regs[] = {

	// Raw 10bit 1280x720 30fps 2lane 720M bps/lane
	// XVCLK=24Mhz, sysclk=72Mhz
	{0x030f, 0x09},	  // PLL
	{0x3018, 0x32},	  // MIPI 2 lane
	{0x3106, 0x01},
	/* {0x3501, 0x4c},	  // expouere H */
	/* {0x3502, 0x00},	  // exposure L */
	/* {0x3700, 0x24},	  // sensor control */
	/* {0x3701, 0x0c}, */
	/* {0x3702, 0x28}, */
	/* {0x3703, 0x19}, */
	/* {0x3704, 0x14}, */
	/* {0x3706, 0x38}, */
	/* {0x3707, 0x04}, */
	/* {0x3708, 0x24}, */
	/* {0x3709, 0x40}, */
	/* {0x370a, 0x00}, */
	/* {0x370b, 0xb8}, */
	/* {0x370c, 0x04}, */
	/* {0x3718, 0x12}, */
	/* {0x3712, 0x42}, */
	/* {0x371e, 0x19}, */
	/* {0x371f, 0x40}, */
	/* {0x3720, 0x05}, */
	/* {0x3721, 0x05}, */
	/* {0x3724, 0x02}, */
	/* {0x3725, 0x02}, */
	/* {0x3726, 0x06}, */
	/* {0x3728, 0x05}, */
	/* {0x3729, 0x02}, */
	/* {0x372a, 0x03}, */
	/* {0x372b, 0x53}, */
	/* {0x372c, 0xa3}, */
	/* {0x372d, 0x53}, */
	/* {0x372e, 0x06}, */
	/* {0x372f, 0x10}, */
	/* {0x3730, 0x01}, */
	/* {0x3731, 0x06}, */
	/* {0x3732, 0x14}, */
	/* {0x3736, 0x20}, */
	/* {0x373a, 0x02}, */
	/* {0x373b, 0x0c}, */
	/* {0x373c, 0x0a}, */
	/* {0x373e, 0x03}, */
	/* {0x375a, 0x06}, */
	/* {0x375b, 0x13}, */
	/* {0x375d, 0x02}, */
	/* {0x375f, 0x14}, */
	{0x3767, 0x1c},
	/* {0x3772, 0x23}, */
	/* {0x3773, 0x02}, */
	/* {0x3774, 0x16}, */
	/* {0x3775, 0x12}, */
	/* {0x3776, 0x08}, */
	/* {0x37a0, 0x44}, */
	/* {0x37a1, 0x3d}, */
	/* {0x37a2, 0x3d}, */
	{0x37a3, 0x01},
	{0x37a5, 0x08},
	/* {0x37a7, 0x44}, */
	/* {0x37a8, 0x58}, */
	/* {0x37a9, 0x58}, */
	/* {0x37aa, 0x44}, */
	/* {0x37ab, 0x2e}, */
	/* {0x37ac, 0x2e}, */
	/* {0x37ad, 0x33}, */
	/* {0x37ae, 0x0d}, */
	/* {0x37af, 0x0d}, */
	/* {0x37b3, 0x42}, */
	/* {0x37b4, 0x42}, */
	/* {0x37b5, 0x33}, */
	{0x3808, 0x05},   // X output size H
	{0x3809, 0x00},   // X output size L
	{0x380a, 0x02},   // Y output size H
	{0x380b, 0xd0},   // Y output size L
	{0x380c, 0x06},	  // HTS H
	{0x380d, 0x40},	  // HTS L
	{0x380e, 0x03},	  // VTS H
	{0x380f, 0xa8},	  // VTS L
	{0x3813, 0x04},	  // ISP Y win L
	{0x3814, 0x03},	  // X inc odd
	{0x3821, 0x67},	  // hsync_en_o, fst_vbin, mirror on
	{0x382a, 0x03},	  // Y inc odd
	{0x382b, 0x01},	  // Y inc even
	{0x3830, 0x08},	  // ablc_use_num[5:1]
	{0x3836, 0x02},	  // zline_use_num[5:1]
	{0x3846, 0x88},	  // Y/X boundary pixel numbber for auto size mode
	{0x3f08, 0x0b},
	{0x4000, 0xf1},	   // our range trig en, format chg en, gan chg en, exp chg en, median en
	{0x4001, 0x14},	   // left 32 column, final BLC offset limitation enable
	{0x4020, 0x01},	   // anchor left start H
	{0x4021, 0x20},	   // anchor left start L
	{0x4022, 0x01},	   // anchor left end H
	{0x4023, 0x9f},	   // anchor left end L
	{0x4024, 0x03},	   // anchor right start H
	{0x4025, 0xe0},	   // anchor right start L
	{0x4026, 0x04},	   // anchor right end H
	{0x4027, 0x5f},	   // anchor right end L
	{0x402a, 0x04},	   // top black line start
	{0x402b, 0x04},	   // top black line number
	{0x402c, 0x02},	   // bottom zero line start
	{0x402d, 0x02},	   // bottom zero line number
	{0x402e, 0x08},	   // bottom black line start
	{0x4500, 0x40},	   // ADC sync control
	{0x4601, 0x74},	   // V FIFO control
	{0x5002, 0x08},	   // vario pixel off
	{0x5901, 0x00},

	{0x4202, 0x00},	 // MIPI stream on when new frame starts
	{0x0100, 0x01},	 // Sensor wake up from standby, MIPI stream on
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

#if 0
static struct regval_list ov8865_init_800_600_raw8_regs[] = {
	// Raw 8bit 800x600 90fps 2lane 720M bps/lane
	// XVCLK=24Mhz, sysclk=72Mhz
	// MIPI 2 lane, 720Mbps/lane
	{0x030f, 0x09},    // PLL
	{0x3018, 0x32},
	{0x3106, 0x01},
	{0x3501, 0x26},    // expouere H
	{0x3502, 0x00},    // exposure L
	{0x3700, 0x24},    // sensor control
	{0x3701, 0x0c},
	{0x3702, 0x28},
	{0x3703, 0x19},
	{0x3704, 0x14},
	{0x3706, 0x38},
	{0x3707, 0x04},
	{0x3708, 0x24},
	{0x3709, 0x40},
	{0x370a, 0x00},
	{0x370b, 0xb8},
	{0x370c, 0x04},
	{0x3718, 0x12},
	{0x3712, 0x42},
	{0x371e, 0x19},
	{0x371f, 0x40},
	{0x3720, 0x05},
	{0x3721, 0x05},
	{0x3724, 0x02},
	{0x3725, 0x02},
	{0x3726, 0x06},
	{0x3728, 0x05},
	{0x3729, 0x02},
	{0x372a, 0x03},
	{0x372b, 0x53},
	{0x372c, 0xa3},
	{0x372d, 0x53},
	{0x372e, 0x06},
	{0x372f, 0x10},
	{0x3730, 0x01},
	{0x3731, 0x06},
	{0x3732, 0x14},
	{0x3736, 0x20},
	{0x373a, 0x02},
	{0x373b, 0x0c},
	{0x373c, 0x0a},
	{0x373e, 0x03},
	{0x375a, 0x06},
	{0x375b, 0x13},
	{0x375d, 0x02},
	{0x375f, 0x14},
	{0x3767, 0x18},
	{0x3772, 0x23},
	{0x3773, 0x02},
	{0x3774, 0x16},
	{0x3775, 0x12},
	{0x3776, 0x08},
	{0x37a0, 0x44},
	{0x37a1, 0x3d},
	{0x37a2, 0x3d},
	{0x37a3, 0x01},
	{0x37a5, 0x08},
	{0x37a7, 0x44},
	{0x37a8, 0x58},
	{0x37a9, 0x58},
	{0x37aa, 0x44},
	{0x37ab, 0x2e},
	{0x37ac, 0x2e},
	{0x37ad, 0x33},
	{0x37ae, 0x0d},
	{0x37af, 0x0d},
	{0x37b3, 0x42},
	{0x37b4, 0x42},
	{0x37b5, 0x33},
	{0x3808, 0x03},    // X output size H
	{0x3809, 0x20},    // X output size L
	{0x380a, 0x02},    // Y output size H
	{0x380b, 0x58},    // Y output size L
	{0x380c, 0x04},    // HTS H
	{0x380d, 0xe2},    // HTS L
	/* {0x380e, 0x02},    // VTS H */
	/* {0x380f, 0x80},    // VTS L */
	{0x380e, 0x0a},    // VTS H
	{0x380f, 0x80},    // VTS L
	{0x3813, 0x04},    // ISP Y win L
	{0x3814, 0x03},    // X inc odd
	{0x3821, 0x6f},    // hsync_en_o, fst_vbin, mirror on
	{0x382a, 0x05},    // Y inc odd
	{0x382b, 0x03},    // Y inc even
	{0x3830, 0x08},    // ablc_use_num[5:1]
	{0x3836, 0x02},    // zline_use_num[5:1]
	{0x3846, 0x88},    // Y/X boundary pixel numbber for auto size mode
	{0x3f08, 0x0b},
	{0x4000, 0xf1},    // our range trig en, format chg en, gan chg en, exp chg en, median en
	{0x4001, 0x14},    // left 32 column, final BLC offset limitation enable
	{0x4020, 0x01},    // anchor left start H
	{0x4021, 0x20},    // anchor left start L
	{0x4022, 0x01},    // anchor left end H
	{0x4023, 0x9f},    // anchor left end L
	{0x4024, 0x03},    // anchor right start H
	{0x4025, 0xe0},    // anchor right start L
	{0x4026, 0x04},    // anchor right end H
	{0x4027, 0x5f},    // anchor right end L
	{0x402a, 0x02},    // top black line start
	{0x402b, 0x02},    // top black line number
	{0x402c, 0x00},    // bottom zero line start
	{0x402d, 0x00},    // bottom zero line number
	{0x402e, 0x04},    // bottom black line start
	{0x4500, 0x40},    // ADC sync control
	{0x4601, 0x50},    // V FIFO control
	{0x5002, 0x0c},    // vario pixel off
	{0x5901, 0x04},

	{SENSOR_REG_END, 0x00},	/* END MARKER */

};
#endif
static struct regval_list ov8865_stream_on[] = {

	{0x4202, 0x00},	 // MIPI stream on when new frame starts
	{0x0100, 0x01},	 // Sensor wake up from standby, MIPI stream on

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_stream_off[] = {

	{0x4202, 0x0f},	 // MIPI stream off when current frame finish. Both clock and data lane in LP11 mode
	{0x0100, 0x00},	 // Sensor go to standby. MIPI stream off, both clock and data lane in LP11 mode.

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8865_software_reset[] = {
	{0x0103, 01},	    //software reset
	//delay 2ms
	{0x0100, 0x00},	     // software standby
	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct sensor_win_setting ov8865_1600_1200_lowfps_win = {
		.width		= 1600,
		.height	        = 1200,
#ifdef OV8865_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs		= ov8865_init_1600_1200_lowfps_regs,
		.reg_sum        = ARRAY_SIZE(ov8865_init_1600_1200_lowfps_regs),
		.sysclk	        = 72,	//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x9b0,
		.hts            = 0x788,
};

static struct sensor_win_setting ov8865_win_sizes[] = {
	/* 3264*2448 */
	
	{
		.width		= 3264,
		.height	        = 2448,
#ifdef OV8865_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace     = V4L2_COLORSPACE_SRGB,
		.regs		= ov8865_init_3264_2448_regs,
		.reg_sum        = ARRAY_SIZE(ov8865_init_3264_2448_regs),
		.sysclk	        = 72,	//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x9a6,
		.hts            = 0x740,
	},
#if 0
	/* 1600x1200 */
	{
		.width		= 1600,
		.height	        = 1200,
#ifdef OV8865_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs		= ov8865_init_1600_1200_regs,
		.reg_sum        = ARRAY_SIZE(ov8865_init_1600_1200_regs),
		.sysclk	        = 72,	//M for caculate banding step
		.low_fps_win    = &ov8865_1600_1200_lowfps_win,
		.vts            = 0x612,
		.hts            = 0x788,
	},
#endif
	/* 1280x720 */
	{
		.width	        = 1280,
		.height	        = 720,
#ifdef OV8865_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace     = V4L2_COLORSPACE_SRGB,
		.regs	        = ov8865_init_1280_720_regs,
		.reg_sum        = ARRAY_SIZE(ov8865_init_1280_720_regs),
		.sysclk	        = 72,	//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x3a8,
		.hts            = 0x640,
	},

	/* 800*600 */
	/* { */
	/* 	.width		= 800, */
	/* 	.height	        = 600, */
	/* 	.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8, */
	/* 	.colorspace	= V4L2_COLORSPACE_SRGB, */
	/* 	.regs		= ov8865_init_800_600_raw8_regs, */
	/* 	.reg_sum        = ARRAY_SIZE(ov8865_init_800_600_raw8_regs), */
	/* 	.sysclk	        = 72,	 //M for caculate banding step */
	/* } */

};
#define OV8865_N_WIN_SIZES (ARRAY_SIZE(ov8865_win_sizes))

static struct sensor_format_struct ov8865_formats[] = {
	{
		/*RAW8 FORMAT, 8 bit per pixel*/
#ifdef OV8865_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
	},
	{
#ifdef OV8865_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_YUYV10_2X10,
#endif
		.colorspace	 = V4L2_COLORSPACE_BT878,/*don't know*/
	}
	/*add other forat supported*/
};
#define N_OV8865_FMTS ARRAY_SIZE(ov8865_formats)

static int ov8865_read(struct v4l2_subdev *sd, unsigned short reg,
		       unsigned char *value)
{
	int ret;
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

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

/* static  */
static int ov8865_write(struct v4l2_subdev *sd, unsigned short reg, unsigned char value)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int ov8865_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov8865_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		//mdelay(200);
		vals++;
	}
	ov8865_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov8865_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov8865_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		//mdelay(200);
		vals++;
	}
	//ov8865_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov8865_write_testself_mode(struct v4l2_subdev *sd, struct regval_list *vals,  
				      unsigned char config_value)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if(vals->reg_num == SENSOR_REG_TESTMODE) {
			ret = ov8865_write(sd, vals->reg_num, config_value);
			printk("config_value is %d\n", config_value);
			if (ret < 0)
				return -1;
			ov8865_read(sd, vals->reg_num, &config_value);
			printk(" This is %d\n", config_value);
		}
            
		vals++;
	}
	return 0;
}

struct ov8865_otp_struct {
	int flag; // bit[7]: info, bit[6]:wb, bit[5]:vcm, bit[4]:lenc
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int lenc[62];
	int VCM_start;
	int VCM_end;
	int VCM_dir;
};

static int ov8865_read_otp(struct v4l2_subdev *sd, unsigned short reg)
{
	int ret, value;
	unsigned char tmp = 0;
	ret = ov8865_read(sd, reg, &tmp);
	value = tmp;
	return value;
}

static int ov8865_write_otp(struct v4l2_subdev *sd, unsigned short reg, int value)
{
	int ret = 0;
	unsigned char tmp = value;
	ret = ov8865_write(sd, reg, tmp);
	return ret;
}

// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
int read_otp(struct v4l2_subdev *sd, struct ov8865_otp_struct *otp_ptr)
{
	int otp_flag, otp_base, temp, i;
	// Pre-processing
	ov8865_write_otp(sd, 0x5002, 0x00); // disable OTP_DPC
	// read OTP into buffer
	ov8865_write_otp(sd, 0x3d84, 0xC0);
	ov8865_write_otp(sd, 0x3d88, 0x70); // OTP start address
	ov8865_write_otp(sd, 0x3d89, 0x10);
	ov8865_write_otp(sd, 0x3d8A, 0x70); // OTP end address
	ov8865_write_otp(sd, 0x3d8B, 0xf4);
	ov8865_write_otp(sd, 0x3d81, 0x01); // load otp into buffer
	mdelay(10);
	// OTP into
	otp_flag = ov8865_read_otp(sd, 0x7010);
	otp_base = 0;
	if((otp_flag & 0xc0) == 0x40) {
		otp_base = 0x7011; // base address of info group 1
	}
	else if((otp_flag & 0x30) == 0x10) {
		otp_base = 0x7016; // base address of info group 2
	}
	else if((otp_flag & 0x0c) == 0x04) {
		otp_base = 0x701b; // base address of info group 3
	}
	if(otp_base != 0) {
		(*otp_ptr).flag = 0x80; // valid info in OTP
		(*otp_ptr).module_integrator_id = ov8865_read_otp(sd, otp_base);
		(*otp_ptr).lens_id = ov8865_read_otp(sd, otp_base + 1);
		(*otp_ptr).production_year = ov8865_read_otp(sd, otp_base + 2);
		(*otp_ptr).production_month = ov8865_read_otp(sd, otp_base + 3);
		(*otp_ptr).production_day = ov8865_read_otp(sd, otp_base + 4);
	} else {
		(*otp_ptr).flag = 0x00; // not info in OTP
		(*otp_ptr).module_integrator_id = 0;
		(*otp_ptr).lens_id = 0;
		(*otp_ptr).production_year = 0;
		(*otp_ptr).production_month = 0;
		(*otp_ptr).production_day = 0;
	}

	// OTP WB Calibration
	otp_flag = ov8865_read_otp(sd, 0x7020);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40) {
		otp_base = 0x7021; // base address of WB Calibration group 1
	} else if ((otp_flag & 0x30) == 0x10) {
		otp_base = 0x7026; // base address of WB Calibration group 2
	} else if ((otp_flag & 0x0c) == 0x04) {
		otp_base = 0x702b; // base address of WB Calibration group 3
	}
	if (otp_base != 0) {
		(*otp_ptr).flag |= 0x40;
		temp = ov8865_read_otp(sd, otp_base + 4);
		(*otp_ptr).rg_ratio = (ov8865_read_otp(sd, otp_base)<<2) + ((temp>>6) & 0x03);
		(*otp_ptr).bg_ratio = (ov8865_read_otp(sd, otp_base + 1)<<2) + ((temp>>4) & 0x03);
		(*otp_ptr).light_rg = (ov8865_read_otp(sd, otp_base + 2)<<2) + ((temp>>2) & 0x03);
		(*otp_ptr).light_bg = (ov8865_read_otp(sd, otp_base + 3)<<2) + (temp & 0x03);
	} else {
		(*otp_ptr).rg_ratio = 0;
		(*otp_ptr).bg_ratio = 0;
		(*otp_ptr).light_rg = 0;
		(*otp_ptr).light_bg = 0;
	}

	// OTP VCM Calibration
	otp_flag = ov8865_read_otp(sd, 0x7030);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40) {
		otp_base = 0x7031; // base address of VCM Calibration group 1
	} else if ((otp_flag & 0x30) == 0x10) {
		otp_base = 0x7034; // base address of VCM Calibration group 2
	} else if ((otp_flag & 0x0c) == 0x04) {
		otp_base = 0x7037; // base address of VCM Calibration group 3
	}
	if (otp_base != 0) {
		(*otp_ptr).flag |= 0x20;
		temp = ov8865_read_otp(sd, otp_base + 2);
		(* otp_ptr).VCM_start = (ov8865_read_otp(sd, otp_base)<<2) | ((temp>>6) & 0x03);
		(* otp_ptr).VCM_end = (ov8865_read_otp(sd, otp_base + 1) << 2) | ((temp>>4) & 0x03);
		(* otp_ptr).VCM_dir = (temp>>2) & 0x03;
	} else {
		(* otp_ptr).VCM_start = 0;
		(* otp_ptr).VCM_end = 0;
		(* otp_ptr).VCM_dir = 0;
	}

	// OTP Lenc Calibration
	otp_flag = ov8865_read_otp(sd, 0x703a);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40) {
		otp_base = 0x703b; // base address of Lenc Calibration group 1
	} else if ((otp_flag & 0x30) == 0x10) {
		otp_base = 0x7079; // base address of Lenc Calibration group 2
	} else if ((otp_flag & 0x0c) == 0x04) {
		otp_base = 0x70b7; // base address of Lenc Calibration group 3
	}
	if (otp_base != 0) {
		(*otp_ptr).flag |= 0x10;
		for (i=0;i<62;i++) {
			(* otp_ptr).lenc[i]=ov8865_read_otp(sd, otp_base + i);
		}
	} else {
		for (i=0;i<62;i++) {
			(* otp_ptr).lenc[i]=0;
		}
	}
	// Post-processing
	for(i=0x7010;i<=0x70f4;i++) {
		ov8865_write_otp(sd, i,0); // clear OTP buffer, recommended use continuous write to accelarate
	}
	ov8865_write_otp(sd, 0x5002, 0x08); // enable OTP_DPC

	return (*otp_ptr).flag;
}

// return value:
// bit[7]: 0 no otp info, 1 valid otp info
// bit[6]: 0 no otp wb, 1 valib otp wb
// bit[5]: 0 no otp vcm, 1 valid otp vcm
// bit[4]: 0 no otp lenc, 1 valid otp lenc
static int ov8865_apply_otp(struct v4l2_subdev *sd)
{
	int rg, bg, R_gain, G_gain, B_gain, Base_gain, i;
	struct ov8865_otp_struct current_otp;
	struct ov8865_otp_struct *otp_ptr = &current_otp;

	read_otp(sd, &current_otp);

	// apply OTP WB Calibration
	if ((*otp_ptr).flag & 0x40) {
		if (current_otp.light_rg == 0) {
			// no light source information in OTP, light factor = 1
			rg = current_otp.rg_ratio;
		} else {
			rg = current_otp.rg_ratio * (current_otp.light_rg + 512) / 1024;
		}
		if (current_otp.light_bg == 0) {
			// not light source information in OTP, light factor = 1
			bg = current_otp.bg_ratio;
		} else {
			bg = current_otp.bg_ratio * (current_otp.light_bg + 512) / 1024;
		}
		//calculate sensor WB gain, 0x400 = 1x gain
		R_gain = 0x400 * OV8865_RG_Ratio_Typical / rg;
		G_gain = 0x400;
		B_gain = 0x400 * OV8865_BG_Ratio_Typical / bg;
		// find gain<0x400
		Base_gain = G_gain;
		if (R_gain<Base_gain) {
			Base_gain = R_gain;
		}
		if (B_gain<Base_gain) {
			Base_gain = B_gain;
		}
		// set min gain to 0x400
		R_gain = 0x400 * R_gain / Base_gain;
		G_gain = 0x400 * G_gain / Base_gain;
		B_gain = 0x400 * B_gain / Base_gain;
		// update sensor WB gain
		if (R_gain>0x400) {
			ov8865_write_otp(sd, 0x5018, R_gain>>6);
			ov8865_write_otp(sd, 0x5019, R_gain & 0x003f);
		}
		if (G_gain>0x400) {
			ov8865_write_otp(sd, 0x501A, G_gain>>6);
			ov8865_write_otp(sd, 0x501B, G_gain & 0x003f);
		}
		if (B_gain>0x400) {
			ov8865_write_otp(sd, 0x501C, B_gain>>6);
			ov8865_write_otp(sd, 0x501D, B_gain & 0x003f);
		}
	}

	// apply OTP VCM Calibration
	if ((*otp_ptr).flag & 0x20) {
		// VCM calibration data should be applied to ISP, not sensor
	}

	// apply OTP Lenc Calibration
	if ((*otp_ptr).flag & 0x10) {
		for (i=0;i<62;i++) {
			ov8865_write_otp(sd, 0x5800 + i, (*otp_ptr).lenc[i]);
		}
	}

	return (*otp_ptr).flag;
}

static int ov8865_reset(struct v4l2_subdev *sd, u32 val)
{
	return 0;
}

static int ov8865_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;

	ret = ov8865_write_array(sd, ov8865_software_reset);
	if (ret < 0) {
		printk("ov8865 failed to sofware reset, return %d\n",ret);
		return ret;
	}

	ret = ov8865_write_array(sd, ov8865_init_default_regs);
	if (ret < 0) {
		printk("ov8865 failed to set common reg value, return %d\n",ret);
		return ret;
	}
	
	ret = ov8865_write_array(sd, ov8865_stream_on);
	if (ret < 0) {
		printk("ov8865 stream on failed ret = %d\n",ret);
	} else {
		printk("ov8865 stream on\n");
	}

	// load otp data
	ret = ov8865_apply_otp(sd);
	if (ret & 0x80)
		printk("valid otp info\n");
	else if (ret & 0x40) 
		printk("valib otp wb\n");
	else if (ret & 0x20)
		printk("valid otp vcm\n");
	else if (ret & 0x10)
		printk("valid otp lenc\n");
	else
		printk("no valid otp data\n");

	ret = ov8865_write_array(sd, ov8865_stream_off);
	if (ret < 0) {
		printk("ov8865 stream off failed ret = %d\n",ret);
	} else {
		printk("ov8865 stream off\n");
	}

	return 0;
}

static int ov8865_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
				enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV8865_FMTS)
		return -EINVAL;

	*code = ov8865_formats[index].mbus_code;
	return 0;
}

static int ov8865_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   struct sensor_win_setting **ret_wsize)
{
	struct sensor_win_setting *wsize;

	if(fmt->width > OV8865_MAX_WIDTH || fmt->height > OV8865_MAX_HEIGHT)
		return -EINVAL;
	for (wsize = ov8865_win_sizes; wsize < ov8865_win_sizes + OV8865_N_WIN_SIZES; wsize++)
		if (fmt->width > wsize->width && fmt->height > wsize->height)
			break;
	/* if (wsize >= ov8865_win_sizes + OV8865_N_WIN_SIZES) */
		wsize--;   /* Take the smallest one */
	if (fmt->priv == V4L2_CAP_LOWFPS_MODE && wsize->low_fps_win != NULL){
		wsize = wsize->low_fps_win;
	}
	if (ret_wsize != NULL)
		*ret_wsize = wsize;

	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->code = wsize->mbus_code;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = wsize->colorspace;
	printk("%s(%d) : fmt->code : %08x, fmt->width : %d, fmt->height : %d\n",
	       __func__, __LINE__, fmt->code, fmt->width, fmt->height);

	return 0;
}

static int ov8865_g_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
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

static int ov8865_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	ret = ov8865_try_fmt_internal(sd, fmt, &info->trying_win);
	SENSOR_PRINT(CAMERA_INFO,"info->trying_win- >width = %d, info->trying_win- >height = %d,\n", info->trying_win->width, info->trying_win->height);

	return ret;
}

static int ov8865_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{

	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	/* struct sensor_win_setting *wsize; */
	int ret;

	/* SENSOR_PRINT(CAMERA_WARNING,"[ov8858], problem function:%s, line:%d\n", __func__, __LINE__); */
	/* ret = ov8858_try_fmt_internal(sd, fmt, &wsize); */
	/* if (ret) */
	/* 	return ret; */
	/* if (info->using_win) */
	/* 	SENSOR_PRINT(CAMERA_WARNING,"info->using_win->width =%d \n",info->using_win->width); */
	/* if (info->trying_win) */
	/* 	SENSOR_PRINT(CAMERA_WARNING,"info->trying_win->width =%d \n",info->trying_win->width); */

	if (info->using_win != info->trying_win) {
		if(info->write_mode == GROUP_LUNCH){
		    ret = ov8865_write_array(sd, info->trying_win->lunch_regs);
		}else{
		    ret = ov8865_write_array(sd, info->trying_win->regs);
		}
		if (ret)
			return ret;
	}
	info->using_win = info->trying_win;

	ov8865_g_mbus_fmt(sd, fmt);

	return 0;
}

static int ov8865_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	printk("--------%s: %d enable:%d\n", __func__, __LINE__, enable);
	if (enable) {
		ret = ov8865_write_array(sd, ov8865_stream_on);
		if (ret < 0) {
			printk("ov8865 stream on failed ret = %d\n",ret);
		} else {
			printk("ov8865 stream on\n");
		}
	} else {
		ret = ov8865_write_array(sd, ov8865_stream_off);
		if (ret < 0) {
			printk("ov8865 stream off failed ret = %d\n",ret);
		} else {
			printk("ov8865 stream off\n");
		}
		info->write_mode = OV8865_DEFAULT_REG_WRTIE_MODE;
	}
	return ret;
}

static int ov8865_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov8865_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *interval)
{
	printk("functiong:%s, line:%d\n", __func__, __LINE__);
	if (interval->index >= ARRAY_SIZE(ov8865_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov8865_frame_rates[interval->index];
	return 0;
}

static int ov8865_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	__u32 index = fsize->index;

	printk("functiong:%s, line:%d\n", __func__, __LINE__);
	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	if (index < OV8865_N_WIN_SIZES) {
		struct sensor_win_setting *win = &ov8865_win_sizes[index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = win->width;
		fsize->discrete.height = win->height;
		return 0;
	} else {
		return -EINVAL;
	}

	return -EINVAL;
}

static int ov8865_s_ctrl_1(struct v4l2_subdev *sd, struct v4l2_control *ctrl,unsigned char select_mode)
{
        int ret = -1;
        if (select_mode == TURN_OVSENSOR_TESTMODE) {
		ret = ov8865_write_testself_mode(sd, ov8865_init_default_regs, SENSOR_TESTMODE_VALUE);
		if(ret < 0) {
			printk("functiong:%s, line:%d failed\n", __func__, __LINE__);
			return ret;
		}
	}
     
        if (select_mode == TURN_OVSENSOR_NORMALMODE) {
		ret = ov8865_write_testself_mode(sd, ov8865_init_default_regs, SENSOR_NORMALMODE_VALUE);
		if (ret < 0) {
			printk("functiong:%s, line:%d failed\n", __func__, __LINE__);
			return ret;
		}
	}
 
        return 0;
}

