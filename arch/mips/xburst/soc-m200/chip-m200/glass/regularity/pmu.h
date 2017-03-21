#ifndef __PMU_H__
#define __PMU_H__
#ifdef CONFIG_REGULATOR_RICOH619

#define PMU_I2C_BUSNUM  1

/* ****************************PMU DC/LDO NAME******************************* */
#define DC1_NAME "cpu_core"
#define DC1_SLP_NAME "cpu_core_slp"
#define DC2_NAME "cpu_vmema"
#define DC3_NAME "cpu_mem12"
#define DC4_NAME "cpu_vddio"
#define DC5_NAME "gps"
#define LDO1_NAME "cpu_avdd"
#define LDO2_NAME "emmc_vcc"
#define LDO3_NAME "cpu_dvdd"
#define LDO4_NAME "touchpanel"
#define LDO5_NAME "cpu_2.5v"
#define LDO6_NAME "v33"
#define LDO7_NAME "lcd"
#define LDO8_NAME "vcc_sensor3v3"
#define LDO9_NAME "vcc_sensor1v8"
#define LDO10_NAME "wifi_vddio_1v8"
#define LDORTC1_NAME "rtc_1v8"
#define LDORTC2_NAME "rtc_1v1"
/* ****************************PMU DC/LDO NAME END*************************** */

/* ****************************PMU DC/LDO DEFAULT V************************** */
#define DC1_INIT_UV     1100
#define DC2_INIT_UV     1200
#define DC3_INIT_UV     1200
#ifdef CONFIG_JZ_EPD_V12
#define DC4_INIT_UV     3300
#else
#define DC4_INIT_UV     1800
#endif
#define DC5_INIT_UV     1800
#define LDO1_INIT_UV    2800
#define LDO2_INIT_UV    3300
#define LDO3_INIT_UV    1800
#define LDO4_INIT_UV    3300
#define LDO5_INIT_UV    2500
#define LDO6_INIT_UV    3300
#define LDO7_INIT_UV    1800
#define LDO8_INIT_UV    3300
#define LDO9_INIT_UV    1800
#define LDO10_INIT_UV   1800
#define LDORTC1_INIT_UV 1800
#define LDORTC2_INIT_UV 1100
/* ****************************PMU DC/LDO DEFAULT V END********************** */

/* ****************************PMU DC/LDO ALWAYS ON************************** */
#define DC1_ALWAYS_ON     1
#define DC2_ALWAYS_ON     1
#define DC3_ALWAYS_ON     0
#define DC4_ALWAYS_ON     1
#define DC5_ALWAYS_ON     0
#define LDO1_ALWAYS_ON    1
#define LDO2_ALWAYS_ON    1
#define LDO3_ALWAYS_ON    0
#define LDO4_ALWAYS_ON    0
#define LDO5_ALWAYS_ON    1
#define LDO6_ALWAYS_ON    1
#define LDO7_ALWAYS_ON    0
#define LDO8_ALWAYS_ON    0
#define LDO9_ALWAYS_ON    0
#define LDO10_ALWAYS_ON   0
#define LDORTC1_ALWAYS_ON 1
#define LDORTC2_ALWAYS_ON 1
/* ****************************PMU DC/LDO ALWAYS ON END********************** */

/* ****************************PMU DC/LDO BOOT ON**************************** */
#define DC1_BOOT_ON     1
#define DC2_BOOT_ON     1
#define DC3_BOOT_ON     0
#define DC4_BOOT_ON     1
#define DC5_BOOT_ON     0
#define LDO1_BOOT_ON    1
#define LDO2_BOOT_ON    1
#define LDO3_BOOT_ON    1
#define LDO4_BOOT_ON    0
#define LDO5_BOOT_ON    1
#define LDO6_BOOT_ON    1
#define LDO7_BOOT_ON    1
#define LDO8_BOOT_ON    0
#define LDO9_BOOT_ON    0
#define LDO10_BOOT_ON   0
#define LDORTC1_BOOT_ON 1
#define LDORTC2_BOOT_ON 1
/* ****************************PMU DC/LDO BOOT ON END************************ */

/* ****************************PMU DC/LDO INIT ENABLE************************ */
#define DC1_INIT_ENABLE     DC1_BOOT_ON
#define DC2_INIT_ENABLE     DC2_BOOT_ON
#define DC3_INIT_ENABLE     DC3_BOOT_ON
#define DC4_INIT_ENABLE     DC4_BOOT_ON
#define DC5_INIT_ENABLE     DC5_BOOT_ON
#define LDO1_INIT_ENABLE    LDO1_BOOT_ON
#define LDO2_INIT_ENABLE    LDO2_BOOT_ON
#define LDO3_INIT_ENABLE    LDO3_BOOT_ON
#define LDO4_INIT_ENABLE    LDO4_BOOT_ON
#define LDO5_INIT_ENABLE    LDO5_BOOT_ON
#define LDO6_INIT_ENABLE    LDO6_BOOT_ON
#define LDO7_INIT_ENABLE    LDO7_BOOT_ON
#define LDO8_INIT_ENABLE    LDO8_BOOT_ON
#define LDO9_INIT_ENABLE    LDO9_BOOT_ON
#define LDO10_INIT_ENABLE   LDO10_BOOT_ON
#define LDORTC1_INIT_ENABLE LDORTC1_BOOT_ON
#define LDORTC2_INIT_ENABLE LDORTC2_BOOT_ON
/* ****************************PMU DC/LDO INIT ENABLE END******************** */

/* ****************************BATTERY INIT PARAMETER************************ */
#define CH_VFCHG	 0x03	/* VFCHG	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
#define	CH_VRCHG 	 0x01	/* VRCHG	= 0 - 4 (3.85v, 3.90v, 3.95v, 4.00v, 4.10v) */
#define CH_VBATOVSET 	 0x0	/* VBATOVSET	= 0 or 1 (0 : 4.38v(up)/3.95v(down) 1: 4.53v(up)/4.10v(down)) */
#define CH_ICHG	  	 0x09	/* ICHG		= 0 - 0x1D (100mA - 3000mA) */
#define CH_ILIM_ADP 	 0x0e	/* ILIM_ADP	= 0 - 0x1D (100mA - 3000mA) */
#define CH_ILIM_USB 	 0x0e	/* ILIM_USB	= 0 - 0x1D (100mA - 3000mA) */
#define CH_ICCHG 	 0x01	/* ICCHG	= 0 - 3 (50mA 100mA 150mA 200mA) */
#define FG_TARGET_VSYS   3000	/* This value is the target one to DSOC=0% */
#define FG_TARGET_IBAT   100    /* This value is the target one to DSOC=0% */
#define FG_POFF_VBAT 	 0	/* setting value of 0 per Vbat */
#define FG_RSENSE_VAL	 100	/* setting value of R Sense */
#define JT_EN  		 0	/* JEITA Enable	  = 0 or 1 (1:enable, 0:disable) */
#define JT_HW_SW 	 1	/* JEITA HW or SW = 0 or 1 (1:HardWare, 0:SoftWare) */
#define JT_TEMP_H 	 50	/* degree C */
#define	JT_TEMP_L 	 12	/* degree C */
#define JT_VFCHG_H 	 0x03	/* VFCHG High  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
#define	JT_VFCHG_L 	 0	/* VFCHG High  	= 0 - 4 (4.05v, 4.10v, 4.15v, 4.20v, 4.35v) */
#define	JT_ICHG_H 	 0x09	/* ICHG Hi   	= 0 - 0x1D (100mA - 3000mA) */
#define JT_ICHG_L 	 0x01	/* ICHG Low   	= 0 - 0x1D (100mA - 3000mA) */
/* ****************************BATTERY INIT PARAMETER END******************** */
#endif	/* CONFIG_REGULATOR_RICOH619 */
#endif
