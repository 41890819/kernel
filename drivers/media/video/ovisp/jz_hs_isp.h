#ifndef __JZ_HS_ISP_H__
#define __JZ_HS_ISP_H__
#include "ovisp-isp.h"
#include "../sensor.h"

/* you need to check next file~~
arch/mips/xburst/soc-m200/chip-m200/glass/????/pm.c
put CIM_MCLK, AF_EN, CAMERA_PWDN_N, ISP_SDA, ISP_SCK, ==>> GSS_IGNORE..
 
arch/mips/xburst/soc-m200/common/gpio.c
keep camera PWDN and AF_EN status.
 
arch/mips/xburst/soc-m200/common/pm_p0.c
set hstp camera key
 */

/* #define HSTP_CAMERA_RAW_8 */

#define HS_ISP_ENABLE     (0x01)
#define HS_ISP_DISABLE    (0x00)

#define HS_ISP_WM_OFFSET     (1<<1)

#ifdef CONFIG_BOARD_COLDWAVE
#ifdef CONFIG_BOARD_VERSION_V23
#define HS_PWDN_NO_POWER_OFF 1
#endif
#define HS_BASE_CAMERA_EN  0xb0010000
#define HS_OFFSET_CAMERA_EN  (1 << 13)
#define HS_BASE_AF_EN  0xb0010300
#define HS_OFFSET_AF_EN  (1 << 28)
#define HS_CAMERA_I2C_ADDR 0x6c
#endif

#if (defined CONFIG_BOARD_MARKEN) || (defined CONFIG_BOARD_ZBRV23)
#define HS_PWDN_NO_POWER_OFF 1
#define HS_BASE_CAMERA_EN  0xb0010000
#define HS_OFFSET_CAMERA_EN  (1 << 13)
#define HS_BASE_AF_EN  0xb0010300
#define HS_OFFSET_AF_EN  (1 << 28)
#define HS_CAMERA_I2C_ADDR 0x6c
#endif

#ifdef CONFIG_BOARD_CRUISE
#define HS_BASE_CAMERA_EN  0xb0010000
#define HS_OFFSET_CAMERA_EN  (1 << 13)
#define HS_CAMERA_I2C_ADDR 0x6c
#endif

#ifdef CONFIG_BOARD_WESEEING
#define HS_BASE_CAMERA_EN  0xb0010000
#define HS_OFFSET_CAMERA_EN  (1 << 13)
#define HS_CAMERA_I2C_ADDR 0x20
#endif

#if (defined CONFIG_BOARD_M200S_ZBRZFY) || (defined CONFIG_BOARD_M200S_ZBRZFY_C) || (defined CONFIG_BOARD_M200S_ZBRZFY_D) || (defined CONFIG_BOARD_M200S_ZBRZFY_A)
#define HS_PWDN_NO_POWER_OFF 1
#define HS_BASE_CAMERA_EN  0xb0010300
#define HS_OFFSET_CAMERA_EN  (1 << 4)
#define HS_CAMERA_I2C_ADDR 0x6c
#endif

#ifndef HS_BASE_CAMERA_EN
#define HS_BASE_CAMERA_EN  0xb0010000
#define HS_OFFSET_CAMERA_EN  (1 << 13)
#define HS_CAMERA_I2C_ADDR 0x6c
#endif

#define HS_CAMERA_I2C_FLAG (SELECT_I2C_PRIMARY | SELECT_I2C_WRITE | SELECT_I2C_16BIT_ADDR)

#define HS_ISP_DROP_FRAMES       (10)
#define HS_ISP_FRAME_INTERVAL    (30)

#if 1
#if (defined CONFIG_BOARD_M200S_ZBRZFY) || (defined CONFIG_BOARD_M200S_ZBRZFY_C) || (defined CONFIG_BOARD_M200S_ZBRZFY_D) || (defined CONFIG_BOARD_M200S_ZBRZFY_A)
#define HS_CAMERA_PREVIEW_WIDTH  (1280)
#define HS_CAMERA_PREVIEW_HEIGHT (720)
#define HS_CAMERA_PREVIEW_WIDTH_H  (0x05)
#define HS_CAMERA_PREVIEW_WIDTH_L  (0x00)
#define HS_CAMERA_PREVIEW_HEIGHT_H  (0x02)
#define HS_CAMERA_PREVIEW_HEIGHT_L  (0xd0)
#define HS_CAMERA_CAPTURERAW_WIDTH_H  (0x0a)
#define HS_CAMERA_CAPTURERAW_WIDTH_L  (0x80)
#define HS_CAMERA_CAPTURERAW_HEIGHT_H  (0x05)
#define HS_CAMERA_CAPTURERAW_HEIGHT_L  (0xf0)
#define HS_CAMERA_PROCESSRAW_WIDTH_H  (0x0a)
#define HS_CAMERA_PROCESSRAW_WIDTH_L  (0x80)
#define HS_CAMERA_PROCESSRAW_HEIGHT_H  (0x05)
#define HS_CAMERA_PROCESSRAW_HEIGHT_L  (0xf0)
#else
#define HS_CAMERA_PREVIEW_WIDTH  (1280)
#define HS_CAMERA_PREVIEW_HEIGHT (720)
#define HS_CAMERA_PREVIEW_WIDTH_H  (0x05)
#define HS_CAMERA_PREVIEW_WIDTH_L  (0x00)
#define HS_CAMERA_PREVIEW_HEIGHT_H  (0x02)
#define HS_CAMERA_PREVIEW_HEIGHT_L  (0xd0)
#define HS_CAMERA_CAPTURERAW_WIDTH_H  (0x0c)
#define HS_CAMERA_CAPTURERAW_WIDTH_L  (0xc0)
#define HS_CAMERA_CAPTURERAW_HEIGHT_H  (0x09)
#define HS_CAMERA_CAPTURERAW_HEIGHT_L  (0x90)
#define HS_CAMERA_PROCESSRAW_WIDTH_H  (0x0c)
#define HS_CAMERA_PROCESSRAW_WIDTH_L  (0xc0)
#define HS_CAMERA_PROCESSRAW_HEIGHT_H  (0x09)
#define HS_CAMERA_PROCESSRAW_HEIGHT_L  (0x90)
#endif
#endif

struct hs_isp_sensor_setting{
	unsigned short chip_id;
	struct isp_reg_t* hs_isp_setting;
};

static struct isp_reg_t hs_isp_mipi_regs_init[] = {
	// add new US settings
	{0x60100, 0x01},//Software reset
	{0x6301b, 0xf0},//isp clock enable
	{0x63025, 0x40},//clock divider
	{0x6301a, 0xf0},
	{0x63041, 0xf8},//dma clock
};


static struct isp_reg_t hs_isp_preview[] = {
	{0x1f000, 0x00},
#ifdef HSTP_CAMERA_RAW_8
	{0x1f001, 0x48},//raw 8
#else
	{0x1f001, 0x49},
#endif
	/* {0x1f002, HS_CAMERA_PREVIEW_WIDTH_H}, */
	/* {0x1f003, HS_CAMERA_PREVIEW_WIDTH_L}, */
	/* {0x1f004, HS_CAMERA_PREVIEW_HEIGHT_H}, */
	/* {0x1f005, HS_CAMERA_PREVIEW_HEIGHT_L}, */
	{0x1f006, 0x00},
	{0x1f007, 0x00},
	/* {0x1f008, HS_CAMERA_PREVIEW_WIDTH_H}, */
	/* {0x1f009, HS_CAMERA_PREVIEW_WIDTH_L}, */
	/* {0x1f00a, HS_CAMERA_PREVIEW_HEIGHT_H}, */
	/* {0x1f00b, HS_CAMERA_PREVIEW_HEIGHT_L}, */
	{0x1f00c, 0x00},
	{0x1f00d, 0x00},
	{0x1f00e, 0x00},
	{0x1f00f, 0x00},
	{0x1f022, 0x00},
	{0x1f023, 0x05},
	/* {0x1f024, HS_CAMERA_PREVIEW_WIDTH_H}, */
	/* {0x1f025, HS_CAMERA_PREVIEW_WIDTH_L}, */
	/* {0x1f026, HS_CAMERA_PREVIEW_HEIGHT_H}, */
	/* {0x1f027, HS_CAMERA_PREVIEW_HEIGHT_L}, */
	/* {0x1f028, HS_CAMERA_PREVIEW_WIDTH_H}, */
	/* {0x1f029, HS_CAMERA_PREVIEW_WIDTH_L}, */
	/* {0x1f02a, HS_CAMERA_PREVIEW_WIDTH_H}, */
	/* {0x1f02b, HS_CAMERA_PREVIEW_WIDTH_L}, */
#if defined(CONFIG_BOARD_CRUISE) && defined(CONFIG_VIDEO_OV8858)
	{0x1f070, 0x01},
	{0x1f071, 0x5e},
#else
	{0x1f070, 0x01},
	{0x1f071, 0x00},
#endif
	/* {0x1f072, 0x06}, */
	/* {0x1f073, 0x02}, */
	{0x1f074, 0x00},
	{0x1f075, 0x02},
	{0x1f076, 0x00},
	{0x1f077, 0xff},
	{0x1f078, 0x00},
	{0x1f079, 0x10},
	{0x1f07a, 0x06},
	{0x1f07b, 0x12},
	{0x1f084, 0x01},
	{0x1f085, 0x00},

	/* REG 0~11 */
	{0x63905, 0x80},
	{0x63906, 0x00},
	{0x63907, 0x01},

	{OVISP_REG_END, 0x00},    
};

static struct isp_reg_t hs_isp_aecagc_init[] = {

	/* aec awb manual setting */
	{0x1e030, 0x00},
	{0x1e031, 0x00},
	{0x1e032, 0x17},
	{0x1e033, 0x30},
	{0x1e034, 0x00},
	{0x1e035, 0x10},
	{0x1c082, 0x00},
	{0x1c083, 0xd1},
	{0x1c084, 0x00},
	{0x1c085, 0x80},
	{0x1c086, 0x00},
	{0x1c087, 0xe2},

	{OVISP_REG_END, 0x00},    
};
static struct isp_reg_t hs_isp_capture_raw[] = {
	{0x1f000, 0x00},
#ifdef HSTP_CAMERA_RAW_8
	{0x1f001, 0x08},//raw 8
#else
	{0x1f001, 0x09},
#endif
	/* {0x1f002, HS_CAMERA_CAPTURERAW_WIDTH_H}, */
	/* {0x1f003, HS_CAMERA_CAPTURERAW_WIDTH_L}, */
	/* {0x1f004, HS_CAMERA_CAPTURERAW_HEIGHT_H}, */
	/* {0x1f005, HS_CAMERA_CAPTURERAW_HEIGHT_L}, */
	{0x1f006, 0x00},
	{0x1f007, 0x00},
	/* {0x1f008, HS_CAMERA_CAPTURERAW_WIDTH_H}, */
	/* {0x1f009, HS_CAMERA_CAPTURERAW_WIDTH_L}, */
	/* {0x1f00a, HS_CAMERA_CAPTURERAW_HEIGHT_H}, */
	/* {0x1f00b, HS_CAMERA_CAPTURERAW_HEIGHT_L}, */
	{0x1f00c, 0x00},
	{0x1f00d, 0x00},
	{0x1f00e, 0x00},
	{0x1f00f, 0x00},
	{0x1f022, 0x00},
#ifdef HSTP_CAMERA_RAW_8
	{0x1f023, 0x00},//raw 8
#else
	{0x1f023, 0x01},
#endif
	/* {0x1f024, HS_CAMERA_CAPTURERAW_WIDTH_H}, */
	/* {0x1f025, HS_CAMERA_CAPTURERAW_WIDTH_L}, */
	/* {0x1f026, HS_CAMERA_CAPTURERAW_HEIGHT_H}, */
	/* {0x1f027, HS_CAMERA_CAPTURERAW_HEIGHT_L}, */
	/* {0x1f028, HS_CAMERA_CAPTURERAW_WIDTH_H}, */
	/* {0x1f029, HS_CAMERA_CAPTURERAW_WIDTH_L}, */
	/* {0x1f02a, HS_CAMERA_CAPTURERAW_WIDTH_H}, */
	/* {0x1f02b, HS_CAMERA_CAPTURERAW_WIDTH_L}, */
#if defined(CONFIG_BOARD_CRUISE) && defined(CONFIG_VIDEO_OV8858)
	{0x1f070, 0x00},
	{0x1f071, 0xbb},
#else
	{0x1f070, 0x01},
	{0x1f071, 0x00},
#endif
	{0x1f072, 0x0a},
	{0x1f073, 0xf0},
	{0x1f074, 0x00},
	{0x1f075, 0x10},
	{0x1f076, 0x00},
	{0x1f077, 0xff},
	{0x1f078, 0x00},
	{0x1f079, 0x10},
	{0x1f07a, 0x0b},
	{0x1f07b, 0x00},

	{0x1f084, 0x01},
	{0x1f085, 0x00},
	/* output addr */
	{0x1f0a0, 0x0e},
	{0x1f0a1, 0x00},
	{0x1f0a2, 0x00},
	{0x1f0a3, 0x00},
	/* REG 0~11 */
	{0x63907, 0x01},
	{0x63905, 0x00},
	{OVISP_REG_END, 0x00},    
};
static struct isp_reg_t hs_isp_process_raw[] = {
	{0x1f000, 0x00},
#ifdef HSTP_CAMERA_RAW_8
	{0x1f001, 0x60},//raw 8
#else
	{0x1f001, 0x61},
#endif
	{0x1f002, HS_CAMERA_CAPTURERAW_WIDTH_H},
	{0x1f003, HS_CAMERA_CAPTURERAW_WIDTH_L},
	{0x1f004, HS_CAMERA_CAPTURERAW_HEIGHT_H},
	{0x1f005, HS_CAMERA_CAPTURERAW_HEIGHT_L},
	{0x1f006, 0x00},
	{0x1f007, 0x00},
	{0x1f008, HS_CAMERA_CAPTURERAW_WIDTH_H},
	{0x1f009, HS_CAMERA_CAPTURERAW_WIDTH_L},
	{0x1f00a, HS_CAMERA_CAPTURERAW_HEIGHT_H},
	{0x1f00b, HS_CAMERA_CAPTURERAW_HEIGHT_L},
	{0x1f00c, 0x00},
	{0x1f00d, 0x00},
	{0x1f00e, 0x00},
	{0x1f00f, 0x00},
	{0x1f022, 0x00},
	{0x1f023, 0x05},
	{0x1f024, HS_CAMERA_PROCESSRAW_WIDTH_H},
	{0x1f025, HS_CAMERA_PROCESSRAW_WIDTH_L},
	{0x1f026, HS_CAMERA_PROCESSRAW_HEIGHT_H},
	{0x1f027, HS_CAMERA_PROCESSRAW_HEIGHT_L},
	{0x1f028, HS_CAMERA_PROCESSRAW_WIDTH_H},
	{0x1f029, HS_CAMERA_PROCESSRAW_WIDTH_L},
	{0x1f02a, 0x06},
	{0x1f02b, 0x60},
	/* {0x1f070, 0x00}, */
	/* {0x1f071, 0x00}, */
	/* {0x1f072, 0x00}, */
	/* {0x1f073, 0x00}, */
	/* {0x1f074, 0x00}, */
	/* {0x1f075, 0x00}, */
	/* {0x1f076, 0x00}, */
	/* {0x1f077, 0x00}, */
	/* {0x1f078, 0x00}, */
	/* {0x1f079, 0x00}, */
	/* {0x1f07a, 0x00}, */
	/* {0x1f07b, 0x00}, */
	{0x1f084, 0x01},
	{0x1f085, 0x00},

	/* output addr */
	{0x1f0a0, 0x0f},
	{0x1f0a1, 0x00},
	{0x1f0a2, 0x00},
	{0x1f0a3, 0x00},
#if (defined CONFIG_BOARD_M200S_ZBRZFY) || (defined CONFIG_BOARD_M200S_ZBRZFY_C) || (defined CONFIG_BOARD_M200S_ZBRZFY_D) || (defined CONFIG_BOARD_M200S_ZBRZFY_A)
	{0x1f0a4, 0x0f},
	{0x1f0a5, 0x3e},
	{0x1f0a6, 0x58},
	{0x1f0a7, 0x00},
#else
	{0x1f0a4, 0x0f},
	{0x1f0a5, 0x79},
	{0x1f0a6, 0xec},
	{0x1f0a7, 0x00},
#endif

	/* input addr */
	{0x1f0c4, 0x0e},
	{0x1f0c5, 0x00},
	{0x1f0c6, 0x00},
	{0x1f0c7, 0x00},
	/* REG 0~11 */
	/* {COMMAND_REG2, 0x01}, */
	/* {COMMAND_REG1, 0x89}, */
	{OVISP_REG_END, 0x00},    
};

#endif/*__JZ_HS_ISP_H__*/
