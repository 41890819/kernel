/*
 * drivers/power/ricoh618-battery.c
 *
 * Charger driver for RICOH RN5T618 power management chip.
 *
 * Copyright (C) 2012-2013 RICOH COMPANY,LTD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/mfd/ricoh618.h>
#include <linux/mfd/pmu-common.h>
#include <linux/power/ricoh618_battery.h>
//#include <linux/power/ricoh618_battery_init.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/time.h>

//define for function
//if FG function is enabled, please define below item.
#define FUEL_GAGE_FUNCTION_ENABLE
#define ENABLE_LOW_BATTERY_DETECTION
#define ENABLE_FACTORY_MODE
#define ENABLE_FG_KEEP_ON_MODE
/* #define ENABLE_INTERRUPT_IN_SLEEP */

/* define for current limit. unit is mA */
/*#define RICOH618_MAX_CHARGE_CURRENT	0
#define RICOH618_MAX_ADP_CURRENT	0
#define RICOH618_MAX_USB_CURRENT	0
#define RICOH618_CHARGE_COMPLETION_CURRENT	200	 mA Range 50~200
							 * (Step 50)
#define RICOH618_FULL_CHARGING_VOLTAGE		0	 mv can set 4050,
							 * 4100, 4150, 4200,
							 * 4350(default 4100)
#define	RICOH618_RE_CHARGING_VOLTAGE		0	 mv can set 3850,
							 * 3900, 3950, 4000,
							 * 4100(default 3900)
*/

/* FG setting */
//#define CUTOFF_VOL              3600   /* mV "0" means cutoff
//                            * voltage = original
//                            * OCV table value */
#define RICOH618_REL1_SEL_VALUE         64  /* mv Range 0~240 * (Step 16) */
#define RICOH618_REL2_SEL_VALUE			0

enum int_type {
	SYS_INT  = 0x01,
	DCDC_INT = 0x02,
	ADC_INT  = 0x08,
	GPIO_INT = 0x10,
	CHG_INT	 = 0x40,
};

#ifdef FUEL_GAGE_FUNCTION_ENABLE
/* define for FG delayed time */
#define RICOH618_MONITOR_START_TIME		15
#define RICOH618_FG_RESET_TIME			6
#define RICOH618_FG_STABLE_TIME			120
#define RICOH618_DISPLAY_UPDATE_TIME		15	//60
//#define RICOH618_SOCA_DISP_UPDATE_TIME		60
#define RICOH618_LOW_VOL_DOWN_TIME		10
#define RICOH618_CHARGE_MONITOR_TIME		20
#define RICOH618_CHARGE_RESUME_TIME		1
#define RICOH618_CHARGE_CALC_TIME		1
#define RICOH618_JEITA_UPDATE_TIME		60

/* define for FG parameter */
#define RICOH618_MAX_RESET_SOC_DIFF		5
#define RICOH618_GET_CHARGE_NUM			10
#define RICOH618_UPDATE_COUNT_DISP		4
#define RICOH618_UPDATE_COUNT_FULL		4
#define RICOH618_CHECK_COUNT_MONI		4
#define RICOH618_CHARGE_UPDATE_TIME		3
#define RE_CAP_GO_DOWN				10	/* 40 */
#define RICOH618_ENTER_LOW_VOL			70
#define RICOH618_TAH_SEL2			5
#define RICOH618_TAL_SEL2			6
#define RICOH618_LOAD_C				1200

/* define for FG status */
enum {
	RICOH618_SOCA_START,
	RICOH618_SOCA_UNSTABLE,
	RICOH618_SOCA_FG_RESET,
	RICOH618_SOCA_DISP,
	RICOH618_SOCA_STABLE,
	RICOH618_SOCA_ZERO,
	RICOH618_SOCA_FULL,
	RICOH618_SOCA_LOW_VOL,
};
#endif

#ifdef ENABLE_LOW_BATTERY_DETECTION
#define LOW_BATTERY_DETECTION_TIME		10
#endif

struct ricoh618_soca_info {
	int Rbat;
	int n_cap;
	int ocv_table[11];
	int soc;		/* Latest FG SOC value */
	int displayed_soc;
	int suspend_soc;
	int status;		/* SOCA status 0: Not initial; 5: Finished */
	int stable_count;
	int chg_status;		/* chg_status */
	int soc_delta;		/* soc delta for status3(DISP) */
	int cc_delta;
	int last_soc;
	int ready_fg;
	int reset_count;
	int reset_soc[3];
	int reset_flg_90;
	int reset_flg_95;
	int f_chg_margin;
	int dischg_state;
	int Vbat[RICOH618_GET_CHARGE_NUM];
	int Vsys[RICOH618_GET_CHARGE_NUM];
	int Ibat[RICOH618_GET_CHARGE_NUM];
	int Vbat_ave;
	int Vsys_ave;
	int Ibat_ave;
	int chg_count;
	int update_count;
	/* for LOW VOL state */
	int target_use_cap;
	int hurry_up_flg;
	int re_cap_old;
	int cutoff_ocv;
	int Rsys;
	int target_vsys;
	int target_ibat;
	int jt_limit;

};


struct ricoh618_battery_info {
	struct device      *dev;
	struct power_supply	battery;
	struct power_supply	ac;
	struct power_supply	usb;
	struct delayed_work	monitor_work;
	struct delayed_work	displayed_work;
	struct delayed_work	charge_stable_work;

	struct delayed_work	changed_work;
	struct workqueue_struct * monitor_wqueue;

#ifdef ENABLE_LOW_BATTERY_DETECTION
	struct delayed_work	low_battery_work;
#endif
	struct work_struct	irq_work;	/* for Charging & VADP/VUSB */
//	struct work_struct	usb_irq_work;	/* for VUSB */
	struct delayed_work	charge_monitor_work;
	struct delayed_work	get_charge_work;

	struct delayed_work	jeita_work;


	struct workqueue_struct *workqueue;	/* for Charging & VUSB/VADP */
#ifdef ENABLE_FACTORY_MODE
	struct delayed_work	factory_mode_work;
	struct workqueue_struct *factory_mode_wqueue;
#endif


	struct mutex		lock;
	unsigned long       monitor_time;
	int		irq_ac;
	int		irq_usb;
	int		adc_vdd_mv;
	int		multiple;
	int		alarm_vol_mv;
	int		status;
	int		min_voltage;
	int		max_voltage;
	int		cur_voltage;
	int		capacity;
	int		battery_temp;
	int		time_to_empty;
	int		time_to_full;
	int		chg_ctr;
	int		chg_stat1;
	unsigned	present:1;
	u16		delay;
	struct		ricoh618_soca_info *soca;
	int		first_pwon;
	bool		entry_factory_mode;
	int		ch_vfchg;
	int		ch_vrchg;
	int		ch_vbatovset;
	int		ch_ichg;
	int		ch_ilim_adp;
	int		ch_ilim_usb;
	int		ch_icchg;
	int		fg_target_vsys;
	int		fg_target_ibat;
	int		jt_en;
	int		jt_hw_sw;
	int		jt_temp_h;
	int		jt_temp_l;
	int		jt_vfchg_h;
	int		jt_vfchg_l;
	int		jt_ichg_h;
	int		jt_ichg_l;

	int 		num;
};

int charger_irq;
int g_soc;
int g_fg_on_mode;
/*This is for full state*/
int g_full_flag;

int hyang_test = 0;
uint8_t *battery_init_para;

static int BatteryTableFlageDef=0;
static int BatteryTypeDef=0;
static int Battery_Type(void)
{
	return BatteryTypeDef;
}

static int Battery_Table(void)
{
	return BatteryTableFlageDef;
}

static void ricoh618_battery_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info = container_of(work,
	struct ricoh618_battery_info, monitor_work.work);

	power_supply_changed(&info->battery);
	queue_delayed_work(info->monitor_wqueue, &info->monitor_work,
			   info->monitor_time);
}

#ifdef ENABLE_FACTORY_MODE
static int ricoh618_factory_mode(struct ricoh618_battery_info *info);
#endif

#ifdef FUEL_GAGE_FUNCTION_ENABLE
static int measure_vbatt_FG(struct ricoh618_battery_info *info, int *data);
static int measure_Ibatt_FG(struct ricoh618_battery_info *info, int *data);
static int calc_capacity(struct ricoh618_battery_info *info);
static int get_OCV_init_Data(struct ricoh618_battery_info *info, int index);
static int get_OCV_voltage(struct ricoh618_battery_info *info, int index);
static int get_check_fuel_gauge_reg(struct ricoh618_battery_info *info,
					 int Reg_h, int Reg_l, int enable_bit);
static int calc_capacity_in_period(struct ricoh618_battery_info *info,
					 int *cc_cap, bool *is_charging);
static int get_charge_priority(struct ricoh618_battery_info *info, bool *data);
static int set_charge_priority(struct ricoh618_battery_info *info, bool *data);
static int get_power_supply_status(struct ricoh618_battery_info *info);
static int measure_vsys_ADC(struct ricoh618_battery_info *info, int *data);
static int Calc_Linear_Interpolation(int x0, int y0, int x1, int y1, int y);
static int get_battery_temp(struct ricoh618_battery_info *info);
static int check_jeita_status(struct ricoh618_battery_info *info, bool *is_jeita_updated);

static int calc_ocv(struct ricoh618_battery_info *info)
{
	int Vbat = 0;
	int Ibat = 0;
	int ret;
	int ocv;

	ret = measure_vbatt_FG(info, &Vbat);
	ret = measure_Ibatt_FG(info, &Ibat);

	ocv = Vbat - Ibat * info->soca->Rbat;

	return ocv;
}

static int check_charge_status_2(struct ricoh618_battery_info *info, int displayed_soc_temp)
{
	if (displayed_soc_temp < 0)
			displayed_soc_temp = 0;

	get_power_supply_status(info);

	if (POWER_SUPPLY_STATUS_FULL == info->soca->chg_status) {
		g_full_flag = 1;
		info->soca->displayed_soc = 100*100;
	}
	if (info->soca->Ibat_ave >= 0) {
		if (g_full_flag == 1) {
			info->soca->displayed_soc = 100*100;
		} else {
			if (info->soca->displayed_soc/100 < 99) {
				info->soca->displayed_soc = displayed_soc_temp;
			} else {
				info->soca->displayed_soc = 99 * 100;
			}
		}
	}
	if (info->soca->Ibat_ave < 0) {
		if (g_full_flag == 1) {
			if ((calc_ocv(info) < (get_OCV_voltage(info, 10) - info->soca->f_chg_margin) )
			&& (info->soca->soc/100 <= 98) ) {
				g_full_flag = 0;
				info->soca->displayed_soc = 100*100;
			} else {
				info->soca->displayed_soc = 100*100;
			}
		} else {
			g_full_flag = 0;
			info->soca->displayed_soc = displayed_soc_temp;
		}
	}

	return info->soca->displayed_soc;
}

/**
* Calculate Capacity in a period
* - read CC_SUM & FA_CAP from Coulom Counter
* -  and calculate Capacity.
* @cc_cap: capacity in a period, unit 0.01%
* @is_charging: Flag of charging current direction
*               TRUE : charging (plus)
*               FALSE: discharging (minus)
**/
static int calc_capacity_in_period(struct ricoh618_battery_info *info,
					 int *cc_cap, bool *is_charging)
{
	int err;
	uint8_t cc_sum_reg[4];
	uint8_t cc_clr[4] = {0, 0, 0, 0};
	uint8_t fa_cap_reg[2];
	uint16_t fa_cap;
	uint32_t cc_sum;
	int cc_stop_flag;
	uint8_t status;
	uint8_t charge_state;
	int Ocv;


	*is_charging = true;	/* currrent state initialize -> charging */

	if (info->entry_factory_mode)
		return 0;

	/* get  power supply status */
	err = ricoh618_read(info->dev->parent, CHGSTATE_REG, &status);
	if (err < 0)
		goto out;
	charge_state = (status & 0x1F);
	Ocv = calc_ocv(info);
	if (charge_state == CHG_STATE_CHG_COMPLETE) {
		/* Check CHG status is complete or not */
		cc_stop_flag = 0;
	} else if (calc_capacity(info) == 100) {
		/* Check HW soc is 100 or not */
		cc_stop_flag = 0;
	} else if (Ocv > get_OCV_voltage(info, 9)) {		/*BUGs? ocv/1000 < getocv ROSS 20131012 */
		/* Check VBAT is high level or not */
		cc_stop_flag = 0;
	} else {
		cc_stop_flag = 1;
	}

	if (cc_stop_flag == 1)
	{
		/* Disable Charging/Completion Interrupt */
		err = ricoh618_set_bits(info->dev->parent,
						RICOH618_INT_MSK_CHGSTS1, 0x01);
		if (err < 0)
			goto out;

		/* disable charging */
		err = ricoh618_clr_bits(info->dev->parent, RICOH618_CHG_CTL1, 0x03);
		if (err < 0)
			goto out;
	}

	/* CC_pause enter */
	err = ricoh618_write(info->dev->parent, CC_CTRL_REG, 0x01);
	if (err < 0)
		goto out;
	/* Read CC_SUM */
	err = ricoh618_bulk_reads(info->dev->parent,
					CC_SUMREG3_REG, 4, cc_sum_reg);
	if (err < 0)
		goto out;

	/* CC_SUM <- 0 */
	err = ricoh618_bulk_writes(info->dev->parent,
					CC_SUMREG3_REG, 4, cc_clr);
	if (err < 0)
		goto out;

	/* CC_pause exist */
	err = ricoh618_write(info->dev->parent, CC_CTRL_REG, 0);
	if (err < 0)
		goto out;
	if (cc_stop_flag == 1)
	{

		/* Enable charging */
		err = ricoh618_set_bits(info->dev->parent, RICOH618_CHG_CTL1, 0x03);
		if (err < 0)
			goto out;

		udelay(1000);

		/* Clear Charging Interrupt status */
		err = ricoh618_clr_bits(info->dev->parent,
					RICOH618_INT_IR_CHGSTS1, 0x01);
		if (err < 0)
			goto out;

		/* ricoh618_read(info->dev->parent, RICOH618_INT_IR_CHGSTS1, &val);
		RICOH_FG_DBG("INT_IR_CHGSTS1 = 0x%x\n",val); */

		/* Enable Charging Interrupt */
		err = ricoh618_clr_bits(info->dev->parent,
						RICOH618_INT_MSK_CHGSTS1, 0x01);
		if (err < 0)
			goto out;
	}
	/* Read FA_CAP */
	err = ricoh618_bulk_reads(info->dev->parent,
				 FA_CAP_H_REG, 2, fa_cap_reg);
	if (err < 0)
		goto out;

	/* fa_cap = *(uint16_t*)fa_cap_reg & 0x7fff; */
	fa_cap = (fa_cap_reg[0] << 8 | fa_cap_reg[1]) & 0x7fff;

	/* calculation  two's complement of CC_SUM */
	cc_sum = cc_sum_reg[0] << 24 | cc_sum_reg[1] << 16 |
				cc_sum_reg[2] << 8 | cc_sum_reg[3];

	/* cc_sum = *(uint32_t*)cc_sum_reg; */
	if (cc_sum & 0x80000000) {
		cc_sum = (cc_sum^0xffffffff)+0x01;
		*is_charging = false;		/* discharge */
	}

	*cc_cap = cc_sum*25/9/fa_cap;	/* CC_SUM/3600/FA_CAP */

	return 0;
out:
	dev_err(info->dev, "Error !!-----\n");
	return err;
}
/**
* Calculate target using capacity
**/
static int get_target_use_cap(struct ricoh618_battery_info *info)
{
	int i;
	int ocv_table[11];
	int temp;
	/*int Target_Cutoff_Vol;*/
	int Ocv_ZeroPer_now;
	int Ibat_now;
	int fa_cap,use_cap;
	int FA_CAP_now;
	int start_per = 0;
	int RE_CAP_now;
	int CC_OnePer_step;
	int Ibat_min;

	/* get const value */
	Ibat_min = -1 * info->soca->target_ibat;
	if (info->soca->Ibat_ave > Ibat_min) /* I bat is minus */
	{
		Ibat_now = Ibat_min;
	} else {
		Ibat_now = info->soca->Ibat_ave;
	}
	fa_cap = get_check_fuel_gauge_reg(info, FA_CAP_H_REG, FA_CAP_L_REG,
								0x7fff);
	use_cap = fa_cap - info->soca->re_cap_old;

	Ocv_ZeroPer_now = info->soca->target_vsys*1000 - Ibat_now * info->soca->Rsys;						//Ross modified to debug!!! 1000 deleted

	printk("PMU: -------  Rsys= %d: cutoff_ocv= %d: Ocv_ZeroPer_now= %d =======\n",
	       info->soca->Rsys, info->soca->cutoff_ocv, Ocv_ZeroPer_now);

	/* get FA_CAP_now */
	/* Check Start % */
	for (i = 0; i <= 10; i = i+1) {
		temp = (battery_init_para[i*2]<<8)
			 | (battery_init_para[i*2+1]);
		/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
		temp = ((temp * 5000 * 10 / 4095) + 5) / 10;												//This was wrong !!###50000->>5000
		ocv_table[i] = temp;
		printk("PMU : %s : ocv_table %d is %d v\n",__func__, i, ocv_table[i]);
	}

	for (i = 1; i < 11; i++) {
		printk("PMU : %s : ocv_table %d is %d v Ocv_ZerPernow is %d\n",__func__, i, ocv_table[i],(Ocv_ZeroPer_now / 1000));  //ROSS MODIFIED 100->>1000
		if (ocv_table[i] >= Ocv_ZeroPer_now / 1000) {												//ROSS MODIFIED 100->>1000
			/* unit is 0.001% */
			start_per = Calc_Linear_Interpolation(
				(i-1)*1000, ocv_table[i-1], i*1000,
				 ocv_table[i], (Ocv_ZeroPer_now / 1000));											//ROSS MODIFIED 100->>1000
			i = 11;				//QUIT DERECTLY
		}
	}

	FA_CAP_now = fa_cap * ((10000 - start_per) / 100 ) / 100;

	/*printk("PMU: -------  Target_Cutoff_Vol= %d: Ocv_ZeroPer_now= %d: start_per= %d =======\n",
		   Target_Cutoff_Vol, Ocv_ZeroPer_now, start_per);*/

	/* get RE_CAP_now */
	RE_CAP_now = FA_CAP_now - use_cap ;
	printk("the target_vysy is %d n",info->soca->target_vsys);
	if (RE_CAP_now < RE_CAP_GO_DOWN || info->soca->Vsys_ave < info->soca->target_vsys) {		//ROSS MODIFIED FOR BUG *1000 DELETED.
		info->soca->hurry_up_flg = 1;
	} else {
		info->soca->hurry_up_flg = 0;
		/* get CC_OnePer_step */
		if (info->soca->displayed_soc > 0) { /* avoid divide-by-0 */
			CC_OnePer_step = RE_CAP_now / (info->soca->displayed_soc / 100);
		} else {
			CC_OnePer_step = 0;
		}
		/* get info->soca->target_use_cap */
		info->soca->target_use_cap = use_cap + CC_OnePer_step;
	}

	printk("PMU: ------- FA_CAP_now= %d: RE_CAP_now= %d: target_use_cap= %d: hurry_up_flg= %d -------\n",
	       FA_CAP_now, RE_CAP_now, info->soca->target_use_cap, info->soca->hurry_up_flg);

	return 0;
}

static void ricoh618_displayed_work(struct work_struct *work)
{
	int err;
	uint8_t val;
	int soc_round;
	int last_soc_round;
	int last_disp_round;
	int displayed_soc_temp;
	int cc_cap = 0;
	bool is_charging = true;
	int i;
	int re_cap,fa_cap,use_cap;
	bool is_jeita_updated;
	int temp_ocv;

	struct ricoh618_battery_info *info = container_of(work,
	struct ricoh618_battery_info, displayed_work.work);

	printk("====> %s   %d \n", __func__, __LINE__);
#ifdef ENABLE_FACTORY_MODE
	queue_delayed_work(info->factory_mode_wqueue, &info->factory_mode_work,
			 msecs_to_jiffies(1));
#endif
	if (info->entry_factory_mode) {
		printk("====> %s   %d  factory_mode:%d\n", __func__, __LINE__,info->entry_factory_mode);
		info->soca->status = RICOH618_SOCA_STABLE;
		info->soca->displayed_soc = -EINVAL;
		info->soca->ready_fg = 0;
		return;
	}

	mutex_lock(&info->lock);

	is_jeita_updated = false;

	printk("----> %s   %d \n", __func__, __LINE__);
	if ((RICOH618_SOCA_START == info->soca->status)
		 || (RICOH618_SOCA_STABLE == info->soca->status))
		info->soca->ready_fg = 1;

	/* judge Full state or Moni Vsys state */
	if ((RICOH618_SOCA_DISP == info->soca->status)
		 || (RICOH618_SOCA_STABLE == info->soca->status)) {
		/* caluc 95% ocv */
		temp_ocv = get_OCV_voltage(info, 10) -
				(get_OCV_voltage(info, 10) - get_OCV_voltage(info, 9))/2;

		if ((POWER_SUPPLY_STATUS_FULL == info->soca->chg_status)
			&& (calc_ocv(info) > temp_ocv)) {
			info->soca->status = RICOH618_SOCA_FULL;
			info->soca->update_count = 0;
		}
		if(g_full_flag == 1){
			info->soca->status = RICOH618_SOCA_FULL;
			info->soca->update_count = 0;
		}
		/* check Full state or not*/
		if (info->soca->Ibat_ave >= -20) {
			if ((calc_ocv(info)
				> (get_OCV_voltage(info, 10) - info->soca->f_chg_margin))
				|| (info->soca->displayed_soc/100 >= 99)) {
				g_full_flag = 0;
				info->soca->status = RICOH618_SOCA_FULL;
				info->soca->update_count = 0;
			} else if ((calc_ocv(info) > (get_OCV_voltage(info, 9)))   //##################ROSS MODIFIED ,90->>9
				&& (info->soca->Ibat_ave < 300)) {
				g_full_flag = 0;
				info->soca->status = RICOH618_SOCA_FULL;
				info->soca->update_count = 0;
			}
		} else { /* dis-charging */
			if (info->soca->displayed_soc/100 < RICOH618_ENTER_LOW_VOL) {
				info->soca->target_use_cap = 0;
				info->soca->status = RICOH618_SOCA_LOW_VOL;
			}
		}
	}

	if (RICOH618_SOCA_STABLE == info->soca->status) {
		info->soca->soc = calc_capacity(info) * 100;
		soc_round = info->soca->soc / 100;
		last_soc_round = info->soca->last_soc / 100;

		info->soca->soc_delta = soc_round - last_soc_round;

		//get charge status
		if (info->soca->chg_status == POWER_SUPPLY_STATUS_CHARGING) {
			if (soc_round >= 90) {
				if(soc_round < 95) {
					if (info->soca->reset_flg_90 == 0) {

						err = ricoh618_write(info->dev->parent,
						 FG_CTRL_REG, 0x51);
						if (err< 0)
							dev_err(info->dev, "Error in writing the control register\n");

						info->soca->ready_fg = 0;

						for (i = 0; i < 3; i = i+1) {
							info->soca->reset_soc[i] = 0;
						}
						info->soca->stable_count = 0;

						info->soca->status = RICOH618_SOCA_FG_RESET;
						/* Delay for addition Reset Time (6s) */
						info->soca->stable_count = 1;
						queue_delayed_work(info->monitor_wqueue,
								&info->charge_stable_work,
								RICOH618_FG_RESET_TIME*HZ);

						info->soca->reset_flg_90 = 1;
						goto end_flow;
					}
				} else if (soc_round < 100) {
					if (info->soca->reset_flg_95 == 0) {
						err = ricoh618_write(info->dev->parent,
						 FG_CTRL_REG, 0x51);
						if (err < 0)
							dev_err(info->dev, "Error in writing the control register\n");

						info->soca->ready_fg = 0;

						for (i = 0; i < 3; i = i+1) {
							info->soca->reset_soc[i] = 0;
						}

						info->soca->stable_count = 0;
						info->soca->status = RICOH618_SOCA_FG_RESET;
						info->soca->stable_count = 1;
						queue_delayed_work(info->monitor_wqueue,
								&info->charge_stable_work,
								RICOH618_FG_RESET_TIME*HZ);

						info->soca->reset_flg_95 = 1;
						goto end_flow;
					}
				}
			} else {
				info->soca->reset_flg_90 = 0;
				info->soca->reset_flg_95 = 0;
			}
		}

		if (info->soca->soc_delta >= -1 && info->soca->soc_delta <= 1) {
			info->soca->displayed_soc = info->soca->soc;
		} else {
			info->soca->status = RICOH618_SOCA_DISP;
		}
		info->soca->last_soc = info->soca->soc;
		info->soca->soc_delta = 0;
		info->soca->update_count = 0;
	} else if (RICOH618_SOCA_FULL == info->soca->status) {
		err = check_jeita_status(info, &is_jeita_updated);
		if (err < 0) {
			dev_err(info->dev, "Error in updating JEITA %d\n", err);
			goto end_flow;
		}
		info->soca->soc = calc_capacity(info) * 100;
		if (POWER_SUPPLY_STATUS_FULL == info->soca->chg_status) {
			if (0 == info->soca->jt_limit) {
				g_full_flag = 1;
				info->soca->displayed_soc = 100*100;
				info->soca->update_count = 0;
			} else {
				info->soca->update_count = 0;
			}
		}
		if (info->soca->Ibat_ave >= -20) { /* charging */
			if (0 == info->soca->jt_limit) {
				if (g_full_flag == 1) {
					info->soca->displayed_soc = 100*100;
					info->soca->update_count = 0;
				} else {
					if (info->soca->displayed_soc/100 < 99) {
						info->soca->update_count++;
						if (info->soca->update_count
						>= RICOH618_UPDATE_COUNT_FULL) {
							info->soca->displayed_soc = info->soca->displayed_soc + 100;
							info->soca->update_count = 0;
						}
					} else {
						info->soca->displayed_soc = 99 * 100;
						info->soca->update_count = 0;
					}
				}
			} else {
				info->soca->update_count = 0;
			}
		}
		if (info->soca->Ibat_ave < 0) {
			info->soca->update_count = 0;
			if (g_full_flag == 1) {
				if ((calc_ocv(info) < (get_OCV_voltage(info, 10) - info->soca->f_chg_margin))
					&& (info->soca->soc/100 <= 98)) {
					g_full_flag = 0;
					info->soca->displayed_soc = 100 * 100;
					info->soca->status = RICOH618_SOCA_DISP;
					info->soca->last_soc = info->soca->soc;
					info->soca->soc_delta = 0;
				} else {
					info->soca->displayed_soc = 100 * 100;
				}
			} else {
				g_full_flag = 0;
				info->soca->status = RICOH618_SOCA_DISP;
				info->soca->last_soc = info->soca->soc;
				info->soca->soc_delta = 0;
			}
		}
	} else if (RICOH618_SOCA_LOW_VOL == info->soca->status) {
		if(info->soca->Ibat_ave >= 0) {
			info->soca->soc = calc_capacity(info) * 100;
			info->soca->status = RICOH618_SOCA_DISP;
			info->soca->last_soc = info->soca->soc;
			info->soca->soc_delta = 0;
			info->soca->update_count = 0;
		} else {
			re_cap = get_check_fuel_gauge_reg(info, RE_CAP_H_REG, RE_CAP_L_REG,
								0x7fff);
			fa_cap = get_check_fuel_gauge_reg(info, FA_CAP_H_REG, FA_CAP_L_REG,
								0x7fff);
			use_cap = fa_cap - re_cap;

			if (info->soca->target_use_cap == 0)  {
				info->soca->re_cap_old = re_cap;
				get_target_use_cap(info);
			}

			if((use_cap >= info->soca->target_use_cap)
			|| (info->soca->hurry_up_flg == 1)) {
				info->soca->displayed_soc = info->soca->displayed_soc - 100;
				info->soca->displayed_soc = max(0, info->soca->displayed_soc);
				info->soca->re_cap_old = re_cap;
			}
			get_target_use_cap(info);
			info->soca->soc = calc_capacity(info) * 100;
		}
	}

	if (RICOH618_SOCA_DISP == info->soca->status) {

		info->soca->soc = calc_capacity(info) * 100;

		/* calculate capacity ,soc_delta is the capacity now,last_soc is the last time capacity */
		soc_round = info->soca->soc / 100;
		last_soc_round = info->soca->last_soc / 100;
		last_disp_round = (info->soca->displayed_soc + 50) / 100;

		info->soca->soc_delta =
			info->soca->soc_delta + (soc_round - last_soc_round);

		info->soca->last_soc = info->soca->soc;

		/* six case */
		if (last_disp_round == soc_round) {
			/* if SOC == DISPLAY move to stable */
			info->soca->displayed_soc = info->soca->soc ;
			info->soca->status = RICOH618_SOCA_STABLE;
		} else if (info->soca->Ibat_ave > 0) {
			if ((0 == info->soca->jt_limit) ||
			(POWER_SUPPLY_STATUS_FULL != info->soca->chg_status)) {
				/* Charge */
				if (last_disp_round < soc_round) {
					/* Case 1 : Charge, Display < SOC */
					info->soca->update_count++;
					if ((info->soca->update_count
						 >= RICOH618_UPDATE_COUNT_DISP)
						|| (info->soca->soc_delta == 1)) {
						info->soca->displayed_soc
							= (last_disp_round + 1)*100;
						info->soca->update_count = 0;
	 					info->soca->soc_delta = 0;
					}
					if (info->soca->displayed_soc/100
								 >= soc_round) {
						info->soca->displayed_soc
							= info->soca->soc ;
						info->soca->status
							= RICOH618_SOCA_STABLE;
					}
				} else if (last_disp_round > soc_round) {
					/* Case 2 : Charge, Display > SOC */
					info->soca->update_count = 0;
					if (info->soca->soc_delta >= 3) {
						info->soca->displayed_soc =
							(last_disp_round + 1)*100;
						info->soca->soc_delta -= 3;
					}
					if (info->soca->displayed_soc/100
								 <= soc_round) {
						info->soca->displayed_soc
							= info->soca->soc ;
						info->soca->status
						= RICOH618_SOCA_STABLE;
					}
				}
			} else {
				info->soca->update_count = 0;
				info->soca->soc_delta = 0;
			}
		} else {
			/* Dis-Charge */
			if (last_disp_round > soc_round) {
				/* Case 3 : Dis-Charge, Display > SOC */
				info->soca->update_count++;
				if ((info->soca->update_count
					 >= RICOH618_UPDATE_COUNT_DISP)
				|| (info->soca->soc_delta == -1)) {
					info->soca->displayed_soc
						= (last_disp_round - 1)*100;
					info->soca->update_count = 0;
					info->soca->soc_delta = 0;
				}
				if (info->soca->displayed_soc/100 <= soc_round) {
					info->soca->displayed_soc
						= info->soca->soc ;
					info->soca->status
						= RICOH618_SOCA_STABLE;
				}
			} else if (last_disp_round < soc_round) {
				/* dis Charge, Display < SOC */
				info->soca->update_count = 0;
				if (info->soca->soc_delta <= -3) {
					info->soca->displayed_soc
						= (last_disp_round - 1)*100;
					info->soca->soc_delta += 3;
				}
//				if (last_disp_round >= soc_round) {
				if (info->soca->displayed_soc/100 >= soc_round) {
					info->soca->displayed_soc
						= info->soca->soc ;
					info->soca->status
						= RICOH618_SOCA_STABLE;
				}
			}
		}
	} else if (RICOH618_SOCA_UNSTABLE == info->soca->status) {
		if (0 == info->soca->jt_limit) {
			check_charge_status_2(info, info->soca->displayed_soc);
		}
	} else if (RICOH618_SOCA_FG_RESET == info->soca->status) {
		/* No update */
	} else if (RICOH618_SOCA_START == info->soca->status) {
		err = check_jeita_status(info, &is_jeita_updated);
		is_jeita_updated = false;
		if (err < 0) {
			dev_err(info->dev, "Error in updating JEITA %d\n", err);
		}
		err = ricoh618_read(info->dev->parent, PSWR_REG, &val);

		val &= 0x7f;
		if (info->first_pwon) {
			info->soca->soc = calc_capacity(info) * 100;
			if ((info->soca->soc == 0) && (calc_ocv(info)
					< get_OCV_voltage(info, 0))) {
				info->soca->displayed_soc = 0;
				info->soca->status = RICOH618_SOCA_ZERO;
			} else {
				if (0 == info->soca->jt_limit) {
					check_charge_status_2(info, info->soca->soc);
				} else {
					info->soca->displayed_soc = info->soca->soc;
				}
				info->soca->status = RICOH618_SOCA_UNSTABLE;
			}
		} else if (g_fg_on_mode && (val == 0x7f)) {
			info->soca->soc = calc_capacity(info) * 100;
			if ((info->soca->soc == 0) && (calc_ocv(info)
					< get_OCV_voltage(info, 0))) {
				info->soca->displayed_soc = 0;
				info->soca->status = RICOH618_SOCA_ZERO;
			} else {
				if (0 == info->soca->jt_limit) {
					check_charge_status_2(info, info->soca->soc);
				} else {
					info->soca->displayed_soc = info->soca->soc;
				}
				info->soca->status = RICOH618_SOCA_STABLE;
			}
		} else {
			info->soca->soc = val * 100;
			if (err < 0) {
				dev_err(info->dev,
					 "Error in reading PSWR_REG %d\n", err);
				info->soca->soc
//					 = calc_capacity(info) * 100 + 50;
					 = calc_capacity(info) * 100;
			}

			err = calc_capacity_in_period(info, &cc_cap,
								 &is_charging);
			if (err < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");

			info->soca->cc_delta
				 = (is_charging == true) ? cc_cap : -cc_cap;
			if (calc_ocv(info) < get_OCV_voltage(info, 0)) {
				info->soca->displayed_soc = 0;
				info->soca->status = RICOH618_SOCA_ZERO;
			} else {
				displayed_soc_temp
				       = info->soca->soc + info->soca->cc_delta;
				if (displayed_soc_temp < 0)
					displayed_soc_temp = 0;
				displayed_soc_temp
					 = min(10000, displayed_soc_temp);
				displayed_soc_temp = max(0, displayed_soc_temp);
				if (0 == info->soca->jt_limit) {
					check_charge_status_2(info, displayed_soc_temp);
				} else {
					info->soca->displayed_soc = displayed_soc_temp;
				}
				info->soca->status = RICOH618_SOCA_UNSTABLE;
			}
			printk("----> %s   %d \n", __func__, __LINE__);
		}
	} else if (RICOH618_SOCA_ZERO == info->soca->status) {
		if (calc_ocv(info) > get_OCV_voltage(info, 0)) {
			err = ricoh618_write(info->dev->parent,
							 FG_CTRL_REG, 0x51);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");

			info->soca->stable_count = 0;
			info->soca->status = RICOH618_SOCA_FG_RESET;
			info->soca->stable_count = 1;
			queue_delayed_work(info->monitor_wqueue,
					&info->charge_stable_work,
					RICOH618_FG_RESET_TIME*HZ);
/*			if(info->soca->status == RICOH618_SOCA_STABLE)						//ross added to tested!
				{
					printk("##ROSS's LOG : we are in the stable .THE STATUS NOW IS %d ;\n",info->soca->status);
				}*/
			info->soca->ready_fg = 0;
		}
		info->soca->displayed_soc = 0;
	}

end_flow:
	if (g_fg_on_mode
		 && (info->soca->status == RICOH618_SOCA_STABLE)) {
		err = ricoh618_write(info->dev->parent, PSWR_REG, 0x7f);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");
		g_soc = 0x7F;
	} else {
		if (info->soca->displayed_soc < 0) {
			val = 0;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		err = ricoh618_write(info->dev->parent, PSWR_REG, val);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;
		} 															//ROSS modified for 20131015

		err = calc_capacity_in_period(info, &cc_cap, &is_charging);
		if (err < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");
	//}																//Ross lifted the } from this line to upper2031015

	printk("PMU: ------- STATUS= %d: IBAT= %d: VSYS= %d: VBAT= %d: DSOC= %d: RSOC= %d: -------\n",
	       info->soca->status, info->soca->Ibat_ave, info->soca->Vsys_ave, info->soca->Vbat_ave,
		info->soca->displayed_soc, info->soca->soc);

//	printk("PMU AGE*STATUS * %d*IBAT*%d*VSYS*%d*VBAT*%d*DSOC*%d*RSOC*%d*-------\n",
//	      info->soca->status, info->soca->Ibat_ave, info->soca->Vsys_ave, info->soca->Vbat_ave,
//		info->soca->displayed_soc, info->soca->soc);

#ifdef DISABLE_CHARGER_TIMER
	uint8_t val2;
	/* clear charger timer */
	if ( info->soca->chg_status == POWER_SUPPLY_STATUS_CHARGING ) {
		err = ricoh618_read(info->dev->parent, TIMSET_REG, &val);
		if (err < 0)
			dev_err(info->dev,
			"Error in read TIMSET_REG%d\n", err);
		/* to check bit 0-1 */
		val2 = val & 0x03;

		if (val2 == 0x02){
			/* set rapid timer 240 -> 300 */
			err = ricoh618_set_bits(info->dev->parent, TIMSET_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
			}
		} else {
			/* set rapid timer 300 -> 240 */
			err = ricoh618_clr_bits(info->dev->parent, TIMSET_REG, 0x01);
			err = ricoh618_set_bits(info->dev->parent, TIMSET_REG, 0x02);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
			}
		}
	}
#endif



	if (0 == info->soca->ready_fg)
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH618_FG_RESET_TIME * HZ);
	else if (RICOH618_SOCA_DISP == info->soca->status)
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH618_DISPLAY_UPDATE_TIME * HZ);
	else if (info->soca->hurry_up_flg == 1)
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH618_LOW_VOL_DOWN_TIME * HZ);
	else
		queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
					 RICOH618_DISPLAY_UPDATE_TIME * HZ);

	mutex_unlock(&info->lock);

	if(true == is_jeita_updated)
		power_supply_changed(&info->battery);

	return;
}

static void ricoh618_stable_charge_countdown_work(struct work_struct *work)
{
	int ret;
	int max = 0;
	int min = 100;
	int i;
	struct ricoh618_battery_info *info = container_of(work,
		struct ricoh618_battery_info, charge_stable_work.work);

	if (info->entry_factory_mode)
		return;

	mutex_lock(&info->lock);
	if (RICOH618_SOCA_FG_RESET == info->soca->status)
		info->soca->ready_fg = 1;

	if (2 <= info->soca->stable_count) {
		if (3 == info->soca->stable_count
			&& RICOH618_SOCA_FG_RESET == info->soca->status) {
			ret = ricoh618_write(info->dev->parent,
							 FG_CTRL_REG, 0x51);
			if (ret < 0)
				dev_err(info->dev, "Error in writing the control register\n");
			info->soca->ready_fg = 0;
		}
		info->soca->stable_count = info->soca->stable_count - 1;
		queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH618_FG_STABLE_TIME * HZ / 10);
	} else if (0 >= info->soca->stable_count) {
		/* Finished queue, ignore */
	} else if (1 == info->soca->stable_count) {
		if (RICOH618_SOCA_UNSTABLE == info->soca->status) {
			/* Judge if FG need reset or Not */
			info->soca->soc = calc_capacity(info) * 100;
			if (info->chg_ctr != 0) {
				queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH618_FG_STABLE_TIME * HZ / 10);
				mutex_unlock(&info->lock);
				return;
			}
			/* Do reset setting */
			ret = ricoh618_write(info->dev->parent,
						 FG_CTRL_REG, 0x51);
			if (ret < 0)
				dev_err(info->dev, "Error in writing the control register\n");

			info->soca->ready_fg = 0;
			info->soca->status = RICOH618_SOCA_FG_RESET;

			/* Delay for addition Reset Time (6s) */
			queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH618_FG_RESET_TIME*HZ);
		} else if (RICOH618_SOCA_FG_RESET == info->soca->status) {
			info->soca->reset_soc[2] = info->soca->reset_soc[1];
			info->soca->reset_soc[1] = info->soca->reset_soc[0];
			info->soca->reset_soc[0] = calc_capacity(info) * 100;
			info->soca->reset_count++;

			if (info->soca->reset_count > 10) {
				/* Reset finished; */
				info->soca->soc = info->soca->reset_soc[0];
				info->soca->stable_count = 0;
				goto adjust;
			}

			for (i = 0; i < 3; i++) {
				if (max < info->soca->reset_soc[i]/100)
					max = info->soca->reset_soc[i]/100;
				if (min > info->soca->reset_soc[i]/100)
					min = info->soca->reset_soc[i]/100;
			}

			if ((info->soca->reset_count > 3) && ((max - min)
					< RICOH618_MAX_RESET_SOC_DIFF)) {
				/* Reset finished; */
				info->soca->soc = info->soca->reset_soc[0];
				info->soca->stable_count = 0;
				goto adjust;
			} else {
				/* Do reset setting */
				ret = ricoh618_write(info->dev->parent,
							 FG_CTRL_REG, 0x51);
				if (ret < 0)
					dev_err(info->dev, "Error in writing the control register\n");

				info->soca->ready_fg = 0;

				/* Delay for addition Reset Time (6s) */
				queue_delayed_work(info->monitor_wqueue,
						 &info->charge_stable_work,
						 RICOH618_FG_RESET_TIME*HZ);
			}
		/* Finished queue From now, select FG as result; */
		} else if (RICOH618_SOCA_START == info->soca->status) {
			/* Normal condition */
		} else { /* other state ZERO/DISP/STABLE */
			info->soca->stable_count = 0;
		}

		mutex_unlock(&info->lock);
		return;

adjust:
		info->soca->last_soc = info->soca->soc;
		info->soca->status = RICOH618_SOCA_DISP;
		info->soca->soc_delta = 0;
		info->soca->update_count = 0;

	}
	mutex_unlock(&info->lock);
	return;
}

static void ricoh618_charge_monitor_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info = container_of(work,
		struct ricoh618_battery_info, charge_monitor_work.work);

	get_power_supply_status(info);

	if (POWER_SUPPLY_STATUS_DISCHARGING == info->soca->chg_status
		|| POWER_SUPPLY_STATUS_NOT_CHARGING == info->soca->chg_status) {
		switch (info->soca->dischg_state) {
		case	0:
			info->soca->dischg_state = 1;
			break;
		case	1:
			info->soca->dischg_state = 2;
			break;

		case	2:
		default:
			break;
		}
	} else {
		info->soca->dischg_state = 0;
	}

	queue_delayed_work(info->monitor_wqueue, &info->charge_monitor_work,
					 RICOH618_CHARGE_MONITOR_TIME * HZ);

	return;
}

static void ricoh618_get_charge_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info = container_of(work,
		struct ricoh618_battery_info, get_charge_work.work);

	int Vbat_temp, Vsys_temp, Ibat_temp;
	int Vbat_sort[RICOH618_GET_CHARGE_NUM];
	int Vsys_sort[RICOH618_GET_CHARGE_NUM];
	int Ibat_sort[RICOH618_GET_CHARGE_NUM];
	int i, j;
	int ret;

	mutex_lock(&info->lock);

	for (i = RICOH618_GET_CHARGE_NUM-1; i > 0; i--) {
		if (0 == info->soca->chg_count) {
			info->soca->Vbat[i] = 0;
			info->soca->Vsys[i] = 0;
			info->soca->Ibat[i] = 0;
		} else {
			info->soca->Vbat[i] = info->soca->Vbat[i-1];
			info->soca->Vsys[i] = info->soca->Vsys[i-1];
			info->soca->Ibat[i] = info->soca->Ibat[i-1];
		}
	}

	ret = measure_vbatt_FG(info, &info->soca->Vbat[0]);
	ret = measure_vsys_ADC(info, &info->soca->Vsys[0]);
	ret = measure_Ibatt_FG(info, &info->soca->Ibat[0]);
														/*EDIT 20131011*/
	if ((RICOH618_SOCA_FG_RESET == info->soca->status)||(info->soca->Vbat[0]<500))
	{
		info->soca->Vbat[0] = info->soca->Vbat_ave;
		info->soca->Vsys[0] = info->soca->Vsys_ave;
		info->soca->Ibat[0] = info->soca->Ibat_ave;
	}

	info->soca->chg_count++;

	if (RICOH618_GET_CHARGE_NUM != info->soca->chg_count) {
		queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
					 RICOH618_CHARGE_CALC_TIME * HZ);
		mutex_unlock(&info->lock);
		return ;
	}

	for (i = 0; i < RICOH618_GET_CHARGE_NUM; i++) {
		Vbat_sort[i] = info->soca->Vbat[i];
		Vsys_sort[i] = info->soca->Vsys[i];
		Ibat_sort[i] = info->soca->Ibat[i];
	}

	Vbat_temp = 0;
	Vsys_temp = 0;
	Ibat_temp = 0;
	for (i = 0; i < RICOH618_GET_CHARGE_NUM - 1; i++) {
		for (j = RICOH618_GET_CHARGE_NUM - 1; j > i; j--) {
			if (Vbat_sort[j - 1] > Vbat_sort[j]) {
				Vbat_temp = Vbat_sort[j];
				Vbat_sort[j] = Vbat_sort[j - 1];
				Vbat_sort[j - 1] = Vbat_temp;
			}
			if (Vsys_sort[j - 1] > Vsys_sort[j]) {
				Vsys_temp = Vsys_sort[j];
				Vsys_sort[j] = Vsys_sort[j - 1];
				Vsys_sort[j - 1] = Vsys_temp;
			}
			if (Ibat_sort[j - 1] > Ibat_sort[j]) {
				Ibat_temp = Ibat_sort[j];
				Ibat_sort[j] = Ibat_sort[j - 1];
				Ibat_sort[j - 1] = Ibat_temp;
			}
		}
	}

	Vbat_temp = 0;
	Vsys_temp = 0;
	Ibat_temp = 0;
	for (i = 3; i < RICOH618_GET_CHARGE_NUM-3; i++) {
		Vbat_temp = Vbat_temp + Vbat_sort[i];
		Vsys_temp = Vsys_temp + Vsys_sort[i];
		Ibat_temp = Ibat_temp + Ibat_sort[i];
	}
	Vbat_temp = Vbat_temp / (RICOH618_GET_CHARGE_NUM - 6);
	Vsys_temp = Vsys_temp / (RICOH618_GET_CHARGE_NUM - 6);
	Ibat_temp = Ibat_temp / (RICOH618_GET_CHARGE_NUM - 6);

	if (0 == info->soca->chg_count) {
		queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
				 RICOH618_CHARGE_UPDATE_TIME * HZ);
		mutex_unlock(&info->lock);
		return;
	} else {
		info->soca->Vbat_ave = Vbat_temp;
		info->soca->Vsys_ave = Vsys_temp;
		info->soca->Ibat_ave = Ibat_temp;
	}

	info->soca->chg_count = 0;
	queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
				 RICOH618_CHARGE_UPDATE_TIME * HZ);
	mutex_unlock(&info->lock);
	return;
}

/* Initial setting of FuelGauge SOCA function */
static int ricoh618_init_fgsoca(struct ricoh618_battery_info *info)
{
	int i;
	int err;

	for (i = 0; i <= 10; i = i+1) {
		info->soca->ocv_table[i] = get_OCV_voltage(info, i);
		printk(KERN_INFO "PMU: %s : * %d0%% voltage = %d uV\n",
				 __func__, i, info->soca->ocv_table[i]);
	}

	for (i = 0; i < 3; i = i+1)
		info->soca->reset_soc[i] = 0;
	info->soca->reset_count = 0;

	if (info->first_pwon) {
		err = ricoh618_write(info->dev->parent,
						 FG_CTRL_REG, 0x51);
		if (err < 0)
			dev_err(info->dev, "Error in writing the control register\n");
	}

	/* Rbat : Transfer */
	info->soca->Rbat = get_OCV_init_Data(info, 12) * 1000 / 512
							 * 5000 / 4095;
	info->soca->n_cap = get_OCV_init_Data(info, 11);

	info->soca->f_chg_margin = (get_OCV_voltage(info, 10) -
								get_OCV_voltage(info, 9)) / 10 * 3;

	info->soca->displayed_soc = 0;
	info->soca->suspend_soc = 0;
	info->soca->ready_fg = 0;
	info->soca->soc_delta = 0;
	info->soca->update_count = 0;
	info->soca->status = RICOH618_SOCA_START;
	/* stable count down 11->2, 1: reset; 0: Finished; */
	info->soca->stable_count = 11;
	info->soca->dischg_state = 0;
	info->soca->Vbat_ave = 0;
	info->soca->Vsys_ave = 0;
	info->soca->Ibat_ave = 0;
	info->soca->chg_count = 0;
	info->soca->target_use_cap = 0;
	info->soca->hurry_up_flg = 0;
	info->soca->re_cap_old = 0;
	info->soca->jt_limit = 0;

	for (i = 0; i < RICOH618_GET_CHARGE_NUM; i++) {
		info->soca->Vbat[i] = 0;
		info->soca->Vsys[i] = 0;
		info->soca->Ibat[i] = 0;
	}

	g_full_flag = 0;

#ifdef ENABLE_FG_KEEP_ON_MODE
	g_fg_on_mode = 1;
#else
	g_fg_on_mode = 0;
#endif

	/* Start first Display job */
	queue_delayed_work(info->monitor_wqueue, &info->displayed_work,
						   RICOH618_FG_RESET_TIME*HZ);

	/* Start first Waiting stable job */
	queue_delayed_work(info->monitor_wqueue, &info->charge_stable_work,
		   RICOH618_FG_STABLE_TIME*HZ/10);

	queue_delayed_work(info->monitor_wqueue, &info->charge_monitor_work,
					 RICOH618_CHARGE_MONITOR_TIME * HZ);

	queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
					 RICOH618_CHARGE_MONITOR_TIME * HZ);
	if (info->jt_en) {
		if (info->jt_hw_sw) {
			/* Enable JEITA function supported by H/W */
			err = ricoh618_set_bits(info->dev->parent, CHGCTL1_REG, 0x04);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
		} else {
		 	/* Disable JEITA function supported by H/W */
			err = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x04);
			if (err < 0)
				dev_err(info->dev, "Error in writing the control register\n");
			queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
						 	 RICOH618_FG_RESET_TIME * HZ);
		}
	} else {
		/* Disable JEITA function supported by H/W */
		err = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x04);
		if (err < 0)
			dev_err(info->dev, "Error in writing the control register\n");
	}


	printk(KERN_INFO "PMU: %s : * Rbat = %d mOhm   n_cap = %d mAH\n",
			 __func__, info->soca->Rbat, info->soca->n_cap);
	return 1;
}
#endif

static void ricoh618_changed_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info = container_of(work,
				struct ricoh618_battery_info, changed_work.work);

	power_supply_changed(&info->battery);

	return;
}

static int check_jeita_status(struct ricoh618_battery_info *info, bool *is_jeita_updated)
/*  JEITA Parameter settings
*
*          VCHG
*            |
* jt_vfchg_h~+~~~~~~~~~~~~~~~~~~~+
*            |                   |
* jt_vfchg_l-| - - - - - - - - - +~~~~~~~~~~+
*            |    Charge area    +          |
*  -------0--+-------------------+----------+--- Temp
*            !                   +
*          ICHG
*            |                   +
*  jt_ichg_h-+ - -+~~~~~~~~~~~~~~+~~~~~~~~~~+
*            +    |              +          |
*  jt_ichg_l-+~~~~+   Charge area           |
*            |    +              +          |
*         0--+----+--------------+----------+--- Temp
*            0   jt_temp_l      jt_temp_h   55
*/
{
	int temp;
	int err = 0;
	int vfchg;
	uint8_t chgiset_org;
	uint8_t batset2_org;
	uint8_t set_vchg_h, set_vchg_l;
	uint8_t set_ichg_h, set_ichg_l;

	*is_jeita_updated = false;

	/* No execute if JEITA disabled */
	if (!info->jt_en || info->jt_hw_sw)
		return 0;

	/* Check FG Reset */
	if (info->soca->ready_fg) {
		temp = get_battery_temp(info) / 10;
	} else {
		goto out;
	}

	/* Read BATSET2 */
	err = ricoh618_read(info->dev->parent, BATSET2_REG, &batset2_org);
	if (err < 0) {
		goto out;
	}
	vfchg = (batset2_org & 0x70) >> 4;
	batset2_org &= 0x8F;

	/* Read CHGISET */
	err = ricoh618_read(info->dev->parent, CHGISET_REG, &chgiset_org);
	if (err < 0) {
		dev_err(info->dev, "Error in readng the chrage setting register\n");
		goto out;
	}
	chgiset_org &= 0xC0;

	set_ichg_h = (uint8_t)(chgiset_org | info->jt_ichg_h);
	set_ichg_l = (uint8_t)(chgiset_org | info->jt_ichg_l);

	set_vchg_h = (uint8_t)((info->jt_vfchg_h << 4) | batset2_org);
	set_vchg_l = (uint8_t)((info->jt_vfchg_l << 4) | batset2_org);

	printk(KERN_INFO "PMU: %s *** Temperature: %d, vfchg: %d, SW status: %d, chg_status: %d ******\n",
		 __func__, temp, vfchg, info->soca->status, info->soca->chg_status);

	if (temp <= 0 || 55 <= temp) {
		/* 1st and 5th temperature ranges (~0, 55~) */
		printk(KERN_INFO "PMU: %s *** Temp(%d) is out of 0-55 ******\n", __func__, temp);
		err = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
		info->soca->jt_limit = 0;
		*is_jeita_updated = true;
	} else if (temp < info->jt_temp_l) {
		/* 2nd temperature range (0~12) */
		if (vfchg != info->jt_vfchg_h) {
			printk(KERN_INFO "PMU: %s *** 0<Temp<12, update to vfchg=%d ******\n",
									__func__, info->jt_vfchg_h);
			err = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
				goto out;
			}

			/* set VFCHG/VRCHG */
			err = ricoh618_write(info->dev->parent,
							 BATSET2_REG, set_vchg_h);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the battery setting register\n");
				goto out;
			}
			info->soca->jt_limit = 0;
			*is_jeita_updated = true;
		} else
			printk(KERN_INFO "PMU: %s *** 0<Temp<50, already set vfchg=%d, so no need to update ******\n",
					__func__, info->jt_vfchg_h);

		/* set ICHG */
		err = ricoh618_write(info->dev->parent, CHGISET_REG, set_ichg_l);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the battery setting register\n");
			goto out;
		}
		err = ricoh618_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
	} else if (temp < info->jt_temp_h) {
		/* 3rd temperature range (12~50) */
		if (vfchg != info->jt_vfchg_h) {
			printk(KERN_INFO "PMU: %s *** 12<Temp<50, update to vfchg==%d ******\n", __func__, info->jt_vfchg_h);

			err = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
				goto out;
			}
			/* set VFCHG/VRCHG */
			err = ricoh618_write(info->dev->parent,
							 BATSET2_REG, set_vchg_h);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the battery setting register\n");
				goto out;
			}
			info->soca->jt_limit = 0;
		} else
			printk(KERN_INFO "PMU: %s *** 12<Temp<50, already set vfchg==%d, so no need to update ******\n",
					__func__, info->jt_vfchg_h);

		/* set ICHG */
		err = ricoh618_write(info->dev->parent, CHGISET_REG, set_ichg_h);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the battery setting register\n");
			goto out;
		}
		err = ricoh618_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
	} else if (temp < 55) {
		/* 4th temperature range (50~55) */
		if (vfchg != info->jt_vfchg_l) {
			printk(KERN_INFO "PMU: %s *** 50<Temp<55, update to vfchg==%d ******\n", __func__, info->jt_vfchg_l);

			err = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x03);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the control register\n");
				goto out;
			}
			/* set VFCHG/VRCHG */
			err = ricoh618_write(info->dev->parent,
							 BATSET2_REG, set_vchg_l);
			if (err < 0) {
				dev_err(info->dev, "Error in writing the battery setting register\n");
				goto out;
			}
			info->soca->jt_limit = 1;
			*is_jeita_updated = true;
		} else
			printk(KERN_INFO "JEITA: %s *** 50<Temp<55, already set vfchg==%d, so no need to update ******\n",
					__func__, info->jt_vfchg_l);

		/* set ICHG */
		err = ricoh618_write(info->dev->parent, CHGISET_REG, set_ichg_h);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the battery setting register\n");
			goto out;
		}
		err = ricoh618_set_bits(info->dev->parent, CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto out;
		}
	}

	get_power_supply_status(info);
	printk(KERN_INFO "PMU: %s *** Hope updating value in this timing after checking jeita, chg_status: %d, is_jeita_updated: %d ******\n",
		 __func__, info->soca->chg_status, *is_jeita_updated);

	return 0;

out:
	printk(KERN_INFO "PMU: %s ERROR ******\n", __func__);
	return err;
}

static void ricoh618_jeita_work(struct work_struct *work)
{
	int ret;
	bool is_jeita_updated = false;
	struct ricoh618_battery_info *info = container_of(work,
		struct ricoh618_battery_info, jeita_work.work);

	mutex_lock(&info->lock);

	ret = check_jeita_status(info, &is_jeita_updated);
	if (0 == ret) {
		queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
					 RICOH618_JEITA_UPDATE_TIME * HZ);
	} else {
		printk(KERN_INFO "PMU: %s *** Call check_jeita_status() in jeita_work, err:%d ******\n",
							__func__, ret);
		queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
					 RICOH618_FG_RESET_TIME * HZ);
	}

	mutex_unlock(&info->lock);

	if(true == is_jeita_updated)
		power_supply_changed(&info->battery);

	return;
}

#ifdef ENABLE_FACTORY_MODE
/*------------------------------------------------------*/
/* Factory Mode						*/
/*    Check Battery exist or not			*/
/*    If not, disabled Rapid to Complete State change	*/
/*------------------------------------------------------*/
static int ricoh618_factory_mode(struct ricoh618_battery_info *info)
{
	int ret = 0;
	uint8_t val = 0;

	ret = ricoh618_read(info->dev->parent, RICOH618_INT_MON_CHGCTR, &val);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return ret;
	}
	if (!(val & 0x01)) /* No Adapter connected */
		return ret;

	/* Rapid to Complete State change disable */
	ret = ricoh618_write(info->dev->parent, RICOH618_CHG_CTL1, 0x63);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return ret;
	}

	/* Wait 1s for checking Charging State */
	queue_delayed_work(info->factory_mode_wqueue, &info->factory_mode_work,
			 msecs_to_jiffies(1));

	return ret;
}

static void check_charging_state_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info = container_of(work,
		struct ricoh618_battery_info, factory_mode_work.work);

	int ret = 0;
	uint8_t val = 0;
	int chargeCurrent = 0;

	ret = ricoh618_read(info->dev->parent, CHGSTATE_REG, &val);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
		return;
	}


	chargeCurrent = get_check_fuel_gauge_reg(info, CC_AVERAGE1_REG,
						 CC_AVERAGE0_REG, 0x3fff);
	if (chargeCurrent < 0) {
		dev_err(info->dev, "Error in reading the FG register\n");
		return;
	}

	/* Rapid State && Charge Current about 0mA*/
	if (((chargeCurrent > 0x3ffc && chargeCurrent < 0x3fff)
		|| chargeCurrent < 0x05) && val == 0x43) {
		printk("PMU:%s ==== No battery !! Enter Factory mode --- chargeCurrent:%d\n"
				, __func__, chargeCurrent);
		info->entry_factory_mode = true;
		/* clear FG_ACC bit */
		ret = ricoh618_clr_bits(info->dev->parent, RICOH618_FG_CTRL, 0x10);
		if (ret < 0)
			dev_err(info->dev, "Error in writing FG_CTRL\n");

		return;	/* Factory Mode */
	}
	printk("=======> chargeCurrent:0x%x \n\n",chargeCurrent);

	/* Return Normal Mode --> Rapid to Complete State change enable */
	/* disable the status change from Rapid Charge to Charge Complete */
	/*ret = ricoh618_write(info->dev->parent, RICOH618_CHG_CTL1, 0xa3);*/
	ret = ricoh618_clr_bits(info->dev->parent, CHGCTL1_REG, 0x40);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return;
	}
//	printk("PMU:%s ==== have battery !! --- chargeCurrent:%x \n\n", __func__, chargeCurrent);
	printk(KERN_INFO "PMU:%s --- Battery exist !! Return Normal mode ---0x%2x\n"
			, __func__, val);

	return;
}
#endif /* ENABLE_FACTORY_MODE */


static int Calc_Linear_Interpolation(int x0, int y0, int x1, int y1, int y)
{
	int	alpha;
	int x;

	alpha = (y - y0)*100 / (y1 - y0);

	x = ((100 - alpha) * x0 + alpha * x1) / 100;

	return x;
}
static int ricoh618_set_OCV_table(struct ricoh618_battery_info *info)
{
	int		ret = 0;
	int		ocv_table[11];
	int		i, j;
	int		available_cap;
	int		temp;
	int		start_par = 0;
	int		percent_step;
	int		OCV_percent_new[11];
	int		Rbat;
	int		Ibat_min;
	int		target_vbat;

	printk("----------- start set OCV  \n");
	info->soca->target_vsys = info->fg_target_vsys;
	info->soca->target_ibat = info->fg_target_ibat;

	/* get internal impedence */
	temp =  (battery_init_para[24]<<8) | (battery_init_para[25]);
	Rbat = temp * 1000 / 512 * 5000 / 4095;
	info->soca->Rsys = Rbat + 55;

	if ((info->soca->target_ibat == 0) || (info->soca->target_vsys == 0)) {	/* normal version */
		info->soca->target_ibat = RICOH618_LOAD_C;
		info->soca->target_vsys = (get_OCV_voltage(info, 0)  - RICOH618_LOAD_C * info->soca->Rsys)/1000 ;    //MODIFIED BY ROSS FOR THE  SAKE OF THE UNIT IS NOT COMPATIBLE
		if (info->soca->target_vsys < 3400) //Ross added !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
         info->soca->target_vsys = 3400;
	} else {	/*Slice cutoff voltage version. */
		/* get ocv table. this table is calculated by Apprication */
		for (i = 0; i <= 10; i = i+1) {
			temp = (battery_init_para[i*2]<<8)
				 | (battery_init_para[i*2+1]);
			/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
			temp = ((temp * 50000 * 10 / 4095) + 5) / 10;			//MODIFIED BY ROSS 5000->>4095
			ocv_table[i] = temp;
		}

//		/* get internal impedence */
//		temp =  (battery_init_para[24]<<8) | (battery_init_para[25]);
//		Rbat = temp * 1000 / 512 * 5000 / 4095;

		Ibat_min = -1 * info->soca->target_ibat;
//		info->soca->Rsys = Rbat + 55;
		info->soca->cutoff_ocv = info->soca->target_vsys - Ibat_min * info->soca->Rsys / 1000;	//ROSS REVIVID THIS TO SCALING

		printk("PMU: -------  Rbat= %d: Rsys= %d: cutoff_ocv= %d: =======\n",
			Rbat, info->soca->Rsys, info->soca->cutoff_ocv);


		/* Check Start % */
		for (i = 1; i < 11; i++) {
			if (ocv_table[i] >= info->soca->cutoff_ocv * 10) {
				/* unit is 0.001% */
				start_par = Calc_Linear_Interpolation(
					(i-1)*1000, ocv_table[i-1], i*1000,
					 ocv_table[i], (info->soca->cutoff_ocv * 10));
				i = 11;
			}
		}
		/* calc new ocv percent */
		percent_step = (10000 - start_par) / 10;

		for (i = 0; i < 11; i++) {
			OCV_percent_new[i]
				 = start_par + percent_step*(i - 0);
		}

		/* calc new ocv voltage */
		for (i = 0; i < 11; i++) {
			for (j = 1; j < 11; j++) {
				if (1000*j >= OCV_percent_new[i]) {
					temp = Calc_Linear_Interpolation(
						ocv_table[j-1], (j-1)*1000,
						 ocv_table[j] , j*1000,
						 OCV_percent_new[i]);

					temp = temp * 4095 / 50000;

					battery_init_para[i*2 + 1] = temp;
					battery_init_para[i*2] = temp >> 8;

					j = 11;
				}
			}
		}

		for (i = 0; i <= 10; i = i+1) {
			temp = (battery_init_para[i*2]<<8)
				 | (battery_init_para[i*2+1]);
			/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
			temp = ((temp * 5000 * 10 / 4095) + 5) / 10;				//WHY ALL THE DATA IS *4095/50000? ROSS
			ocv_table[i] = temp;
			printk("PMU : %s : ocv_table %d is %d v\n",__func__, i, ocv_table[i]);
		}

		/* calc available capacity */
		/* get avilable capacity */
		/* battery_init_para23-24 is designe capacity */
		available_cap = (battery_init_para[22]<<8)
					 | (battery_init_para[23]);

		available_cap = available_cap
			 * ((10000 - start_par) / 100) / 100 ;


		battery_init_para[23] =  available_cap;
		battery_init_para[22] =  available_cap >> 8;

	}
	ret = ricoh618_bulk_writes_bank1(info->dev->parent,
			 BAT_INIT_TOP_REG, 32, battery_init_para);
	for (i = 0; i <= 10; i = i+1) {
			temp = (battery_init_para[i*2]<<8)
				 | (battery_init_para[i*2+1]);
			/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
			temp = ((temp * 5000 * 10 / 4095) + 5) / 10;				//WHY ALL THE DATA IS *4095/50000? ROSS
			ocv_table[i] = temp;
			printk("PMU : %s : wr ocv_table %d is %d v\n",__func__, i, ocv_table[i]);
		}
	uint8_t val;
	ret = ricoh618_read_bank1(info->dev->parent,0xD5,&val);
    printk("PMU : wr Resister is %x v\n",val);

	if (ret < 0) {
		dev_err(info->dev, "batterry initialize error\n");
		return ret;
	}
	printk("----------- start set OCV successful !\n");

	return 1;
}

//Initial setting of battery
static int ricoh618_init_battery(struct ricoh618_battery_info *info)
{
	int ret = 0;
	//Need to implement initial setting of batery and error
	////////////////////////////////
#ifdef FUEL_GAGE_FUNCTION_ENABLE
	uint8_t val = 0;
	uint8_t val2;
	/* set kanwa state */
	if (RICOH618_REL1_SEL_VALUE > 240)
		val = 0x0F;
	else
		val = RICOH618_REL1_SEL_VALUE / 16 ;

	/* set relaxation state */
	if (RICOH618_REL2_SEL_VALUE > 120)
		val2 = 0x0F;
	else
		val2 = RICOH618_REL2_SEL_VALUE / 8 ;

	val =  val + (val2 << 4);


	ret = ricoh618_write_bank1(info->dev->parent, BAT_REL_SEL_REG, val);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the OCV Tabler\n");
		return ret;
	}

	ret = ricoh618_write_bank1(info->dev->parent, BAT_TA_SEL_REG, 0x00);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing BAT_TA_SEL_REG\n");
		return ret;
	}
	/* for debug
	ret = ricoh618_read(info->dev->parent, BAT_TA_SEL_REG, &val);
	printk("PMU : %s : TEMP!!! BAT TA SEL is 0x%x\n",__func__,val);
	*************/

	ret = ricoh618_read(info->dev->parent, FG_CTRL_REG, &val);
	if(ret<0){
		dev_err(info->dev, "Error in reading the control register\n");
		return ret;
	}
	val = (val & 0x10) >> 4;//check FG_ACC DATA
	info->first_pwon = (val == 0) ? 1 : 0;

	ret = ricoh618_set_OCV_table(info);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the OCV Tabler\n");
		return ret;
	}

	ret = ricoh618_write(info->dev->parent, FG_CTRL_REG, 0x11);
	if(ret<0){
		dev_err(info->dev, "Error in writing the control register\n");
		return ret;
	}

#endif

      //////////////////////////////
	ret = ricoh618_write(info->dev->parent, VINDAC_REG, 0x01);
	if (ret < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
		return ret;
	}

	if(info->alarm_vol_mv < 2700 || info->alarm_vol_mv > 3400){
		dev_err(info->dev, "alarm_vol_mv is out of range!\n");
		return -1;
	}

	return ret;
}

/* Initial setting of charger */
static int ricoh618_init_charger(struct ricoh618_battery_info *info)
{
	int err;
	uint8_t val;
	uint8_t val2;
	uint8_t val3;
	int charge_status;

	info->chg_ctr = 0;
	info->chg_stat1 = 0;

	err = ricoh618_set_bits(info->dev->parent, RICOH618_PWR_FUNC, 0x20);
	if (err < 0) {
		dev_err(info->dev, "Error in writing the PWR FUNC register\n");
		goto free_device;
	}
	charge_status = get_power_supply_status(info);

	if (charge_status != POWER_SUPPLY_STATUS_FULL)
	{
		/* Disable charging */
		err = ricoh618_clr_bits(info->dev->parent,CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto free_device;
		}
	}

	/* REGISET1:(0xB6) setting */
	if ((info->ch_ilim_adp != 0xFF) || (info->ch_ilim_adp <= 0x1D)) {
		val = info->ch_ilim_adp;

		err = ricoh618_write(info->dev->parent, REGISET1_REG,val);
		if (err < 0) {
			dev_err(info->dev, "Error in writing REGISET1_REG %d\n",
										 err);
			goto free_device;
		}
	}

		/* REGISET2:(0xB7) setting */
	err = ricoh618_read(info->dev->parent, REGISET2_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read REGISET2_REG %d\n", err);
		goto free_device;
	}

	if ((info->ch_ilim_usb != 0xFF) || (info->ch_ilim_usb <= 0x1D)) {
		val2 = info->ch_ilim_usb;
	} else {/* Keep OTP value */
		val2 = (val & 0x1F);
	}

		/* keep bit 5-7 */
	val &= 0xE0;

	val = val + val2;

	err = ricoh618_write(info->dev->parent, REGISET2_REG,val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing REGISET2_REG %d\n",
									 err);
		goto free_device;
	}

	err = ricoh618_read(info->dev->parent, CHGISET_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read CHGISET_REG %d\n", err);
		goto free_device;
	}

		/* Define Current settings value for charging (bit 4~0)*/
	if ((info->ch_ichg != 0xFF) || (info->ch_ichg <= 0x1D)) {
		val2 = info->ch_ichg;
	} else { /* Keep OTP value */
		val2 = (val & 0x1F);
	}

		/* Define Current settings at the charge completion (bit 7~6)*/
	if ((info->ch_icchg != 0xFF) || (info->ch_icchg <= 0x03)) {
		val3 = info->ch_icchg << 6;
	} else { /* Keep OTP value */
		val3 = (val & 0xC);
	}

	val = val2 + val3;

	err = ricoh618_write(info->dev->parent, CHGISET_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing CHGISET_REG %d\n",
									 err);
		goto free_device;
	}

		//debug messeage
	err = ricoh618_read(info->dev->parent, CHGISET_REG,&val);
	printk("PMU : %s : after CHGISET_REG (0x%x) is 0x%x info->ch_ichg is 0x%x info->ch_icchg is 0x%x\n",__func__,CHGISET_REG,val,info->ch_ichg,info->ch_icchg);

		//debug messeage
	err = ricoh618_read(info->dev->parent, BATSET1_REG,&val);
	printk("PMU : %s : before BATSET1_REG (0x%x) is 0x%x info->ch_vbatovset is 0x%x\n",__func__,BATSET1_REG,val,info->ch_vbatovset);

	/* BATSET1_REG(0xBA) setting */
	err = ricoh618_read(info->dev->parent, BATSET1_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read BATSET1 register %d\n", err);
		goto free_device;
	}

		/* Define Battery overvoltage  (bit 4)*/
	if ((info->ch_vbatovset != 0xFF) || (info->ch_vbatovset <= 0x1)) {
		val2 = info->ch_vbatovset;
		val2 = val2 << 4;
	} else { /* Keep OTP value */
		val2 = (val & 0x10);
	}

		/* keep bit 0-3 and bit 5-7 */
	val = (val & 0xEF);

	val = val + val2;

	err = ricoh618_write(info->dev->parent, BATSET1_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing BAT1_REG %d\n",
									 err);
		goto free_device;
	}
		//debug messeage
	err = ricoh618_read(info->dev->parent, BATSET1_REG,&val);
	printk("PMU : %s : after BATSET1_REG (0x%x) is 0x%x info->ch_vbatovset is 0x%x\n",__func__,BATSET1_REG,val,info->ch_vbatovset);

		//debug messeage
	err = ricoh618_read(info->dev->parent, BATSET2_REG,&val);
	printk("PMU : %s : before BATSET2_REG (0x%x) is 0x%x info->ch_vrchg is 0x%x info->ch_vfchg is 0x%x \n",__func__,BATSET2_REG,val,info->ch_vrchg,info->ch_vfchg);


	/* BATSET2_REG(0xBB) setting */
	err = ricoh618_read(info->dev->parent, BATSET2_REG, &val);
	if (err < 0) {
		dev_err(info->dev,
	 	"Error in read BATSET2 register %d\n", err);
		goto free_device;
	}

		/* Define Re-charging voltage (bit 2~0)*/
	if ((info->ch_vrchg != 0xFF) || (info->ch_vrchg <= 0x04)) {
		val2 = info->ch_vrchg;
	} else { /* Keep OTP value */
		val2 = (val & 0x07);
	}

		/* Define FULL charging voltage (bit 6~4)*/
	if ((info->ch_vfchg != 0xFF) || (info->ch_vfchg <= 0x04)) {
		val3 = info->ch_vfchg;
		val3 = val3 << 4;
	} else {	/* Keep OTP value */
		val3 = (val & 0x70);
	}

		/* keep bit 3 and bit 7 */
	val = (val & 0x88);

	val = val + val2 + val3;

	err = ricoh618_write(info->dev->parent, BATSET2_REG, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing RICOH618_RE_CHARGE_VOLTAGE %d\n",
									 err);
		goto free_device;
	}


	/* Set rising edge setting ([1:0]=01b)for INT in charging */
	/*  and rising edge setting ([3:2]=01b)for charge completion */
	err = ricoh618_read(info->dev->parent, RICOH618_CHG_STAT_DETMOD1, &val);
	if (err < 0) {
		dev_err(info->dev, "Error in reading CHG_STAT_DETMOD1 %d\n",
								 err);
		goto free_device;
	}
	val &= 0xf0;
	val |= 0x05;
	err = ricoh618_write(info->dev->parent, RICOH618_CHG_STAT_DETMOD1, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing CHG_STAT_DETMOD1 %d\n",
								 err);
		goto free_device;
	}

	/* Unmask In charging/charge completion */
	err = ricoh618_write(info->dev->parent, RICOH618_INT_MSK_CHGSTS1, 0xfc);
	if (err < 0) {
		dev_err(info->dev, "Error in writing INT_MSK_CHGSTS1 %d\n",
								 err);
		goto free_device;
	}

	/* Set both edge for VUSB([3:2]=11b)/VADP([1:0]=11b) detect */
	err = ricoh618_read(info->dev->parent, RICOH618_CHG_CTRL_DETMOD1, &val);
	if (err < 0) {
		dev_err(info->dev, "Error in reading CHG_CTRL_DETMOD1 %d\n",
								 err);
		goto free_device;
	}
	val &= 0xf0;
	val |= 0x0f;
	err = ricoh618_write(info->dev->parent, RICOH618_CHG_CTRL_DETMOD1, val);
	if (err < 0) {
		dev_err(info->dev, "Error in writing CHG_CTRL_DETMOD1 %d\n",
								 err);
		goto free_device;
	}

	/* Unmask In VUSB/VADP completion */
	err = ricoh618_write(info->dev->parent, RICOH618_INT_MSK_CHGCTR, 0xfc);
	if (err < 0) {
		dev_err(info->dev, "Error in writing INT_MSK_CHGSTS1 %d\n",
								 err);
		goto free_device;
	}

	if (charge_status != POWER_SUPPLY_STATUS_FULL)
	{
		/* Enable charging */
		err = ricoh618_set_bits(info->dev->parent,CHGCTL1_REG, 0x03);
		if (err < 0) {
			dev_err(info->dev, "Error in writing the control register\n");
			goto free_device;
		}
	}


	/* Set ADRQ=00 to stop ADC */
	ricoh618_write(info->dev->parent, RICOH618_ADC_CNT3, 0x0);
	/* Enable VSYS threshold Low interrupt */
	ricoh618_write(info->dev->parent, RICOH618_INT_EN_ADC1, 0x10);
	/* Set ADC auto conversion interval 250ms */
	ricoh618_write(info->dev->parent, RICOH618_ADC_CNT2, 0x0);
	/* Enable VSYS pin conversion in auto-ADC */
	ricoh618_write(info->dev->parent, RICOH618_ADC_CNT1, 0x10);
	/* Set VSYS threshold low voltage = 3.50v */
	ricoh618_write(info->dev->parent, RICOH618_ADC_VSYS_THL, 0x77);
	/* Start auto-mode & average 4-time conversion mode for ADC */
	ricoh618_write(info->dev->parent, RICOH618_ADC_CNT3, 0x28);
	/* Enable master ADC INT */
	ricoh618_set_bits(info->dev->parent, RICOH618_INTC_INTEN, ADC_INT);

free_device:
	return err;
}


static int get_power_supply_status(struct ricoh618_battery_info *info)
{
	uint8_t status;
	uint8_t supply_state;
	uint8_t charge_state;
	int ret = 0;

	//get  power supply status
	ret = ricoh618_read(info->dev->parent, CHGSTATE_REG, &status);
	if(ret<0){
		dev_err(info->dev, "Error in reading the control register\n");
		return ret;
	}

	charge_state = (status & 0x1F);
	supply_state = (status & 0xC0);

	if (info->entry_factory_mode)
			return POWER_SUPPLY_STATUS_NOT_CHARGING;

	printk("-----> ricoh618 charge_state is 0x%x supply_state is 0x%x !!!!!!\n", charge_state, supply_state);
	if (supply_state == SUPPLY_STATE_BAT)
	{
		info->soca->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}else{
		switch (charge_state) {
		case	CHG_STATE_CHG_OFF:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_CHG_READY_VADP:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_CHG_TRICKLE:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_CHARGING;
				break;
		case	CHG_STATE_CHG_RAPID:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_CHARGING;
				break;
		case	CHG_STATE_CHG_COMPLETE:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_FULL;
				break;
		case	CHG_STATE_SUSPEND:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_VCHG_OVER_VOL:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_BAT_ERROR:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_NO_BAT:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_BAT_OVER_VOL:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_BAT_TEMP_ERR:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_DIE_ERR:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_DIE_SHUTDOWN:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_DISCHARGING;
				break;
		case	CHG_STATE_NO_BAT2:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		case	CHG_STATE_CHG_READY_VUSB:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_NOT_CHARGING;
				break;
		default:
				info->soca->chg_status
					= POWER_SUPPLY_STATUS_UNKNOWN;
				break;
		}
	}

	return	info->soca->chg_status;
}

static void charger_irq_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info
		 = container_of(work, struct ricoh618_battery_info, irq_work);
	int ret = 0;
	printk(KERN_INFO "PMU:%s In\n", __func__);

	power_supply_changed(&info->battery);

	mutex_lock(&info->lock);
	info->chg_ctr = 0;
	info->chg_stat1 = 0;

	/* Enable Interrupt for VADP */
//	ret = ricoh618_clr_bits(info->dev->parent,
//					 RICOH618_INT_MSK_CHGCTR, 0x01);
	ret = ricoh618_write(info->dev->parent, RICOH618_INT_MSK_CHGCTR, 0xfc);
	if (ret < 0)
		dev_err(info->dev,
			 "%s(): Error in enable charger mask INT %d\n",
			 __func__, ret);

	/* Enable Interrupt for Charging & complete */
	ret = ricoh618_write(info->dev->parent, RICOH618_INT_MSK_CHGSTS1, 0xfc);
	if (ret < 0)
		dev_err(info->dev,
			 "%s(): Error in enable charger mask INT %d\n",
			 __func__, ret);

	mutex_unlock(&info->lock);
	printk(KERN_INFO "PMU:%s Out\n", __func__);
}

#ifdef ENABLE_LOW_BATTERY_DETECTION
static void low_battery_irq_work(struct work_struct *work)
{
	struct ricoh618_battery_info *info = container_of(work,
		 struct ricoh618_battery_info, low_battery_work.work);

	int ret = 0;

	printk(KERN_INFO "PMU:%s In\n", __func__);

	power_supply_changed(&info->battery);

	/* Enable VADP threshold Low interrupt */
	ricoh618_write(info->dev->parent, RICOH618_INT_EN_ADC1, 0x10);
	if (ret < 0)
		dev_err(info->dev,
			 "%s(): Error in enable adc mask INT %d\n",
			 __func__, ret);
}
#endif

static irqreturn_t charger_in_isr(int irq, void *battery_info)
{
	struct ricoh618_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_stat1 |= 0x01;
	queue_work(info->workqueue, &info->irq_work);
	return IRQ_HANDLED;
}

static irqreturn_t charger_complete_isr(int irq, void *battery_info)
{
	struct ricoh618_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_stat1 |= 0x02;
	queue_work(info->workqueue, &info->irq_work);

	return IRQ_HANDLED;
}

/*static irqreturn_t charger_usb_isr(int irq, void *battery_info)
{
	struct ricoh618_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_ctr |= 0x02;
	queue_work(info->workqueue, &info->usb_irq_work);

	if (RICOH618_SOCA_UNSTABLE == info->soca->status
		|| RICOH618_SOCA_FG_RESET == info->soca->status)
		info->soca->stable_count = 11;

	return IRQ_HANDLED;
}

static irqreturn_t charger_adp_isr(int irq, void *battery_info)
{
	struct ricoh618_battery_info *info = battery_info;
	printk(KERN_INFO "PMU:%s\n", __func__);

	info->chg_ctr |= 0x01;
	queue_work(info->workqueue, &info->irq_work);

	if (RICOH618_SOCA_UNSTABLE == info->soca->status
		|| RICOH618_SOCA_FG_RESET == info->soca->status)
		info->soca->stable_count = 11;

	return IRQ_HANDLED;
}*/


#ifdef ENABLE_LOW_BATTERY_DETECTION
/*************************************************************/
/* for Detecting Low Battery                                 */
/*************************************************************/

static irqreturn_t adc_vsysl_isr(int irq, void *battery_info)
{

	struct ricoh618_battery_info *info = battery_info;

	printk(KERN_INFO "PMU:%s\n", __func__);

	queue_delayed_work(info->monitor_wqueue, &info->low_battery_work,
					LOW_BATTERY_DETECTION_TIME*HZ);

	return IRQ_HANDLED;
}
#endif


/*
 * Get Charger Priority
 * - get higher-priority between VADP and VUSB
 * @ data: higher-priority is stored
 *         true : VUSB
 *         false: VADP
 */
static int get_charge_priority(struct ricoh618_battery_info *info, bool *data)
{
	int ret = 0;
	uint8_t val = 0;

	ret = ricoh618_read(info->dev->parent, CHGCTL1_REG, &val);
	val = val >> 7;
	*data = (bool)val;

	return ret;
}

/*
 * Set Charger Priority
 * - set higher-priority between VADP and VUSB
 * - data: higher-priority is stored
 *         true : VUSB
 *         false: VADP
 */
static int set_charge_priority(struct ricoh618_battery_info *info, bool *data)
{
	int ret = 0;
	uint8_t val = 0;

	val = *data << 7;
	val &= 0x80;

	ret = ricoh618_set_bits(info->dev->parent, CHGCTL1_REG, val);
	return ret;
}


#ifdef	FUEL_GAGE_FUNCTION_ENABLE
static int get_check_fuel_gauge_reg(struct ricoh618_battery_info * info, int Reg_h, int Reg_l, int enable_bit)
{
	uint8_t get_data_h, get_data_l;
	int old_data, current_data;
	int i;
	int ret = 0;

	old_data = 0;

	for( i = 0; i < 5 ; i++)
	{
		ret = ricoh618_read(info->dev->parent, Reg_h, &get_data_h);
		if (ret < 0){
			dev_err(info->dev, "Error in reading the control register\n");
			return ret;
		}

		ret = ricoh618_read(info->dev->parent, Reg_l, &get_data_l);
		if (ret < 0){
			dev_err(info->dev, "Error in reading the control register\n");
			return ret;
		}

		current_data =((get_data_h & 0xff) << 8) | (get_data_l & 0xff);
		current_data = (current_data & enable_bit);

		if(current_data == old_data)
		{
			return current_data;
		}else{

			old_data = current_data;
		}
	}

	return current_data;
}

static int calc_capacity(struct ricoh618_battery_info *info)
{
	uint8_t capacity;
	int temp;
	int ret = 0;
	int nt;
	int temperature;

	temperature = get_battery_temp(info) / 10; //unit 0.1 degree -> 1 degree

	if (temperature >= 25) {
		//printk("PMU : %s : TEMP!!! temperature is %d Over 25\n",__func__,temperature);
		nt = 0;
	} else if (temperature >= 5) {
		nt = (25 - temperature) * RICOH618_TAH_SEL2 * 625 / 100;
	} else {
		nt = (625  + (5 - temperature) * RICOH618_TAL_SEL2 * 625 / 100);
	}

	//get remaining battery capacity from fuel gauge
	ret = ricoh618_read(info->dev->parent, SOC_REG, &capacity);
	if(ret<0){
		dev_err(info->dev, "Error in reading the control register\n");
		return ret;
	}

	temp = capacity * 100 * 100 / (10000 - nt);

	return temp;		//Unit is 1%
}

static int get_battery_temp(struct ricoh618_battery_info *info)
{
	uint8_t sign_bit;
	int ret = 0;

	ret = get_check_fuel_gauge_reg(info, TEMP_1_REG, TEMP_2_REG, 0x0fff);
	if(ret<0){
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	/* bit3 of 0xED(TEMP_1) is sign_bit */
	sign_bit = ((ret & 0x0800) >> 11);

	ret = ( ret & 0x07ff);


	if(sign_bit ==0){ //positive value part
		ret = ret * 625  / 1000;						//conversion unit // 1 unit is 0.0625 degree and retun unit should be 0.1 degree,
	}else//negative value part
	{
		ret = (~ret + 1) & 0x7ff;
		ret = -1 * ret * 625 /1000;
	}

	return ret;
}

static int get_time_to_empty(struct ricoh618_battery_info *info)
{
	int ret = 0;

	ret = get_check_fuel_gauge_reg(info, TT_EMPTY_H_REG, TT_EMPTY_L_REG, 0xffff);
	if(ret<0){
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	/*conversion unit*/
	/*1unit is 1miniute and return nnit should be 1 second*/
	ret = ret * 60;

	return ret;
}

static int get_time_to_full(struct ricoh618_battery_info *info)
{
	int ret = 0;

	ret = get_check_fuel_gauge_reg(info, TT_FULL_H_REG, TT_FULL_L_REG, 0xffff);
	if(ret<0){
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	ret = ret * 60;

	return  ret;
}

//battery voltage is get from Fuel gauge
static int measure_vbatt_FG(struct ricoh618_battery_info *info, int *data)
{
	int ret = 0;


	ret =  get_check_fuel_gauge_reg(info, VOLTAGE_1_REG, VOLTAGE_2_REG, 0x0fff);
	if(ret<0){
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	*data = ret;
	*data = *data * 5000 / 4095;		//conversion unit 1 Unit is 1.22mv (5000/4095 mv)
	*data = *data * 1000;			//return unit should be 1uV


	return ret;
}
static int measure_Ibatt_FG(struct ricoh618_battery_info *info, int *data)
{
	int ret = 0;

	ret =  get_check_fuel_gauge_reg(info, CC_AVERAGE1_REG,
						 CC_AVERAGE0_REG, 0x3fff);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the fuel gauge control register\n");
		return ret;
	}

	*data = (ret > 0x1fff) ? (ret - 0x4000) : ret;
	return ret;
}

static int get_OCV_init_Data(struct ricoh618_battery_info *info, int index)
{
	int ret = 0;
	ret =  (battery_init_para[index*2]<<8) | (battery_init_para[index*2+1]);
	return ret;
}

static int get_OCV_voltage(struct ricoh618_battery_info *info, int index)
{
	int ret = 0;
	ret =  get_OCV_init_Data(info, index);
	/* conversion unit 1 Unit is 1.22mv (5000/4095 mv) */
	ret = ret * 50000 / 4095;
	/* return unit should be 1uV */
	ret = ret * 100;
	return ret;
}


#else
//battery voltage is get from ADC
static int measure_vbatt_ADC(struct ricoh618_battery_info *info, int *data)
{
	int	i;
	uint8_t data_l =0, data_h = 0;
	int ret;


	ret = ricoh618_set_bits(info->dev->parent, INTEN_REG, 0x08); 		//ADC interrupt enable
	if(ret <0){
		dev_err(info->dev, "Error in setting the control register bit\n");
		goto err;
	}

	ret = ricoh618_set_bits(info->dev->parent, EN_ADCIR3_REG, 0x01); 	//enable interrupt request of single mode
	if(ret <0){
		dev_err(info->dev, "Error in setting the control register bit\n");
		goto err;
	}

	ret = ricoh618_write(info->dev->parent, ADCCNT3_REG, 0x10);		//single request
	if(ret <0){
		dev_err(info->dev, "Error in writing the control register\n");
		goto err;
	}

	for(i = 0; i < 5; i++){
		msleep(1);
		dev_info(info->dev, "ADC conversion times: %d\n", i);
		ret = ricoh618_read(info->dev->parent, EN_ADCIR3_REG, &data_h);	//read completed flag of  ADC
		if(ret <0){
			dev_err(info->dev, "Error in reading the control register\n");
			goto err;
		}

		if(data_h & 0x01){
			goto	done;
		}
	}

	dev_err(info->dev, "ADC conversion too long!\n");
	goto err;

done:
	ret = ricoh618_read(info->dev->parent, VBATDATAH_REG, &data_h);
	if(ret <0){
		dev_err(info->dev, "Error in reading the control register\n");
		goto err;
	}

	ret = ricoh618_read(info->dev->parent, VBATDATAL_REG, &data_l);
	if(ret <0){
		dev_err(info->dev, "Error in reading the control register\n");
		goto err;
	}

	*data = ((data_h & 0xff) << 4) | (data_l & 0x0f);
	*data = *data * 5000 / 4095;								//conversion unit 1 Unit is 1.22mv (5000/4095 mv)
	*data = *data * 1000;									//return unit should be 1uV


	return 0;

err:
//	mutex_unlock(&info->lock);
	return -1;
}

#endif

static int measure_vsys_ADC(struct ricoh618_battery_info *info, int *data)
{
	uint8_t data_l = 0, data_h = 0;
	int ret;

	ret = ricoh618_read(info->dev->parent, VSYSDATAH_REG, &data_h);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
	}

	ret = ricoh618_read(info->dev->parent, VSYSDATAL_REG, &data_l);
	if (ret < 0) {
		dev_err(info->dev, "Error in reading the control register\n");
	}

	*data = ((data_h & 0xff) << 4) | (data_l & 0x0f);
	*data = *data * 1000 * 3 * 5 / 2 / 4095;
	/* return unit should be 1uV */
	*data = *data * 1000;

	return 0;
}

static void ricoh618_external_power_changed(struct power_supply *psy)
{
	struct ricoh618_battery_info *info;

	info = container_of(psy, struct ricoh618_battery_info, battery);
	queue_delayed_work(info->monitor_wqueue,
			   &info->changed_work, HZ / 2);
	return;
}

static int ricoh618_batt_get_prop(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct ricoh618_battery_info *info = dev_get_drvdata(psy->dev->parent);
	int data = 0;
	int ret = 0;
	uint8_t status;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = ricoh618_read(info->dev->parent, CHGSTATE_REG, &status);
		if (ret < 0) {
			dev_err(info->dev, "Error in reading the control register\n");
			mutex_unlock(&info->lock);
			return ret;
		}
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (status & 0x40 ? 1 : 0);
		else if (psy->type == POWER_SUPPLY_TYPE_USB)
			val->intval = (status & 0x80 ? 1 : 0);
		break;
	 //this setting is same as battery driver of 584
	case POWER_SUPPLY_PROP_STATUS:
		ret = get_power_supply_status(info);
		val->intval = ret;
		info->status = ret;
		dev_info(info->dev, "Power Supply Status is %d\n", info->status);
		break;

	 //this setting is same as battery driver of 584
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->present;
		break;

	//current voltage is get from fuel gauge
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* return real vbatt Voltage */
#ifdef	FUEL_GAGE_FUNCTION_ENABLE
		if (info->soca->ready_fg)
			ret = measure_vbatt_FG(info, &data);
		else {
			//val->intval = -EINVAL;
			data = info->cur_voltage * 1000;
			break;
		}
#else
		ret = measure_vbatt_ADC(info, &data);
#endif
		val->intval = data;
		info->cur_voltage = data / 1000;		//convert unit uV -> mV
		dev_info(info->dev, "battery voltage is %d mV\n", info->cur_voltage);

		break;

#ifdef	FUEL_GAGE_FUNCTION_ENABLE
	//current battery capacity is get from fuel gauge
	case POWER_SUPPLY_PROP_CAPACITY:
		if (info->entry_factory_mode){
			val->intval = 100;
			info->capacity = 100;
			printk("----> come to this line: %d show battery capacity !\n", __LINE__);
		} else if (info->soca->displayed_soc <= 0) {
			val->intval = 0;
			info->capacity = 0;
			printk("----> come to this line: %d show battery capacity !\n", __LINE__);
		} else {
			val->intval = (info->soca->displayed_soc + 50)/100;
			info->capacity = (info->soca->displayed_soc + 50)/100;
			printk("----> come to this line: %d show info->soca->displayed_soc %d !\n",
					__LINE__, info->soca->displayed_soc);
		}
		dev_info(info->dev, "battery capacity is %d%%\n",
							info->capacity);

		break;

	//current temperature of buttery
	case POWER_SUPPLY_PROP_TEMP:
		if (info->soca->ready_fg) {
			ret = get_battery_temp(info);
			val->intval = ret;
			info->battery_temp = ret/10;
			dev_info(info->dev, "battery temperature is %d degree \n",  info->battery_temp );
		} else {
			val->intval = info->battery_temp * 10;
			dev_info(info->dev, "battery temperature is %d degree\n", info->battery_temp);
		}

		break;

	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		if (info->soca->ready_fg) {
			ret = get_time_to_empty(info);
			val->intval = ret;
			info->time_to_empty = ret/60;
			dev_info(info->dev,
				 "time of empty battery is %d minutes\n",
							 info->time_to_empty);
		} else {
			//val->intval = -EINVAL;
			val->intval = info->time_to_empty * 60;
			dev_info(info->dev, "time of empty battery is %d minutes\n", info->time_to_empty);
		}

		break;

	 case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		if (info->soca->ready_fg) {
			ret = get_time_to_full(info);
			val->intval = ret;
			info->time_to_full = ret/60;
			dev_info(info->dev,
				 "time of full battery is %d minutes\n",
							 info->time_to_full);
		} else {
			//val->intval = -EINVAL;
			val->intval = info->time_to_full * 60;
			dev_info(info->dev, "time of full battery is %d minutes\n", info->time_to_full);
		}

		break;
#endif
	 case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		ret = 0;
		break;

	default:
		return -ENODEV;
	}

	mutex_unlock(&info->lock);

	return ret;
}

static irqreturn_t ricoh618_ac_irq(int irq, void *dev_id)
{
	struct ricoh618_battery_info *info = dev_id;


	power_supply_changed(&info->ac);

	return IRQ_HANDLED;
}

static irqreturn_t ricoh618_usb_irq(int irq, void *dev_id)
{
	struct ricoh618_battery_info *info = dev_id;

	dev_dbg(info->dev,"----> %s \n", __func__);

	power_supply_changed(&info->usb);

	return IRQ_HANDLED;
}

static enum power_supply_property ricoh618_batt_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,

#ifdef	FUEL_GAGE_FUNCTION_ENABLE
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
#endif
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
};

static enum power_supply_property ricoh618_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

static __devinit int ricoh618_battery_probe(struct platform_device *pdev)
{
	struct ricoh618 *iodev = dev_get_drvdata(pdev->dev.parent);
	struct pmu_platform_data *pmu_pdata = dev_get_platdata(iodev->dev);
	struct ricoh618_battery_info *info;
	struct ricoh618_battery_platform_data *pdata;
	int type_n;
	int temp;


	int ret = 0;

	printk("-------->  %s: \n",__func__);

	info = kzalloc(sizeof(struct ricoh618_battery_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->soca = kzalloc(sizeof(struct ricoh618_soca_info), GFP_KERNEL);
		if (!info->soca)
			return -ENOMEM;


	info->dev = &pdev->dev;
	info->status = POWER_SUPPLY_STATUS_CHARGING;
	pdata = (struct ricoh618_battery_platform_data *)pmu_pdata->bat_private;
	battery_init_para = pdata->battery_init_data;
	info->monitor_time = pdata->monitor_time * HZ;
	info->alarm_vol_mv = pdata->alarm_vol_mv;

	type_n = Battery_Type();
	info->num = Battery_Table();
	temp = sizeof(battery_init_para)/(sizeof(uint8_t)*32);
	printk("%s temp=%d\n", __func__, temp);
	if(info->num >= (sizeof(battery_init_para)/(sizeof(uint8_t)*32)))
		info->num = 0;
	printk("%s type_n=%d\n", __func__, type_n);
	printk("%s info->num=%d\n", __func__, info->num);
	/* these valuse are set in platform */
	if (type_n == 0)
	{
		info->ch_vfchg = pdata->ch_vfchg;
		info->ch_vrchg = pdata->ch_vrchg;
		info->ch_vbatovset = pdata->ch_vbatovset;
		info->ch_ichg = pdata->ch_ichg;
		info->ch_ilim_adp = pdata->ch_ilim_adp;
		info->ch_ilim_usb = pdata->ch_ilim_usb;
		info->ch_icchg = pdata->ch_icchg;
		info->fg_target_vsys = pdata->fg_target_vsys;
		info->fg_target_ibat = pdata->fg_target_ibat;
		info->jt_en = pdata->jt_en;
		info->jt_hw_sw = pdata->jt_hw_sw;
		info->jt_temp_h = pdata->jt_temp_h;
		info->jt_temp_l = pdata->jt_temp_l;
		info->jt_vfchg_h = pdata->jt_vfchg_h;
		info->jt_vfchg_l = pdata->jt_vfchg_l;
		info->jt_ichg_h = pdata->jt_ichg_h;
		info->jt_ichg_l = pdata->jt_ichg_l;
	} else {
	}
	info->adc_vdd_mv = ADC_VDD_MV;	//2800;
	info->min_voltage = MIN_VOLTAGE;	//3600;
	info->max_voltage = MAX_VOLTAGE;	//4200;

	info->delay = 500;
	info->entry_factory_mode = false;
		printk("====> %s   %d  factory_mode:%d\n", __func__, __LINE__,info->entry_factory_mode);


	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);
	if (!((&pdev->dev)->p)) {
		printk("-----> platform_set_drvdata error  \n");
		goto out;
	}

	info->battery.name = "battery";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.properties = ricoh618_batt_props;
	info->battery.num_properties = ARRAY_SIZE(ricoh618_batt_props);
	info->battery.get_property = ricoh618_batt_get_prop;
	info->battery.set_property = NULL;
	info->battery.external_power_changed = ricoh618_external_power_changed;

	/* Disable Charger/ADC interrupt */
	ret = ricoh618_clr_bits(info->dev->parent, RICOH618_INTC_INTEN,
							 CHG_INT | ADC_INT);
	if (ret)
		goto out;

	ret = ricoh618_init_battery(info);
	if (ret){
		printk("  %s  line = %d! \n ", __func__, __LINE__);
		goto out;
	}

#ifdef ENABLE_FACTORY_MODE
	info->factory_mode_wqueue
		= create_singlethread_workqueue("ricoh618_factory_mode");
	INIT_DELAYED_WORK_DEFERRABLE(&info->factory_mode_work,
					 check_charging_state_work);

	ret = ricoh618_factory_mode(info);
	if (ret)
		goto out;

#endif

	ret = power_supply_register(&pdev->dev, &info->battery);
	if (ret){
		info->battery.dev->parent = &pdev->dev;
	}

#define DEF_CHARGER(PSY, NAME, TYPE)					\
	info->PSY.name = NAME;						\
	info->PSY.type = TYPE;						\
	info->PSY.supplied_to = supply_list;				\
	info->PSY.num_supplicants = ARRAY_SIZE(supply_list);		\
	info->PSY.properties = ricoh618_charger_props;			\
	info->PSY.num_properties = ARRAY_SIZE(ricoh618_charger_props);	\
	info->PSY.get_property = ricoh618_batt_get_prop
//	info->PSY.get_property = ricoh618_charger_get_prop

	DEF_CHARGER(usb, "usb", POWER_SUPPLY_TYPE_USB);
	DEF_CHARGER(ac, "ac", POWER_SUPPLY_TYPE_MAINS);
#undef DEF_POWER

	info->irq_usb = RICOH618_IRQ_FVUSBDETSINT + IRQ_RESERVED_BASE;
	info->irq_ac = RICOH618_IRQ_FVADPDETSINT + IRQ_RESERVED_BASE;

	info->monitor_wqueue = create_singlethread_workqueue("ricoh618_battery_monitor");
	info->workqueue = create_singlethread_workqueue("ricoh618_charger_in");
	INIT_WORK(&info->irq_work, charger_irq_work);

	INIT_DELAYED_WORK_DEFERRABLE(&info->monitor_work, ricoh618_battery_work);

	INIT_DELAYED_WORK_DEFERRABLE(&info->displayed_work,
					 ricoh618_displayed_work);
	INIT_DELAYED_WORK_DEFERRABLE(&info->charge_stable_work,
					 ricoh618_stable_charge_countdown_work);
	INIT_DELAYED_WORK_DEFERRABLE(&info->charge_monitor_work,
					 ricoh618_charge_monitor_work);
	INIT_DELAYED_WORK_DEFERRABLE(&info->get_charge_work,
					 ricoh618_get_charge_work);
	INIT_DELAYED_WORK_DEFERRABLE(&info->jeita_work, ricoh618_jeita_work);
	INIT_DELAYED_WORK(&info->changed_work, ricoh618_changed_work);


	power_supply_register(&pdev->dev, &info->usb);
	power_supply_register(&pdev->dev, &info->ac);

	ret = request_threaded_irq(RICOH618_IRQ_FONCHGINT + IRQ_RESERVED_BASE,
					NULL, charger_in_isr,IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"ricoh618_charger_in", info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get CHG_INT IRQ for chrager: %d\n",
									ret);
		goto out;
	}
	ret = request_threaded_irq(RICOH618_IRQ_FCHGCMPINT + IRQ_RESERVED_BASE,
						NULL, charger_complete_isr,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ricoh618_charger_comp",
								info);
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get CHG_COMP IRQ for chrager: %d\n",
									 ret);
		goto out;
	}

	ret = request_threaded_irq(info->irq_ac, NULL, ricoh618_ac_irq,
			  IRQF_TRIGGER_FALLING | IRQF_DISABLED, "ricoh618_ac_irq", info);
	if (ret) {
		dev_err(info->dev, "request ricoh618_ac_irq fail with error num = %d \n", ret);
		goto out;
	}
	ret = request_threaded_irq(info->irq_usb, NULL, ricoh618_usb_irq,
			  IRQF_TRIGGER_FALLING | IRQF_DISABLED, "ricoh618_usb_irq", info);
	if (ret) {
		dev_err(info->dev, "request ricoh618_usb_irq fail with error num = %d \n", ret);
		goto out;
	}

#ifdef ENABLE_LOW_BATTERY_DETECTION
	ret = request_threaded_irq(RICOH618_IRQ_VSYSLIR + IRQ_RESERVED_BASE,
					NULL, adc_vsysl_isr, IRQF_ONESHOT,
						"ricoh618_adc_vsysl", info);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Can't get ADC_VSYSL IRQ for chrager: %d\n", ret);
		goto out;
	}
	INIT_DELAYED_WORK_DEFERRABLE(&info->low_battery_work,
					 low_battery_irq_work);
#endif

	/* Charger init and IRQ setting */
	ret = ricoh618_init_charger(info);
	if (ret)
		goto out;

#ifdef	FUEL_GAGE_FUNCTION_ENABLE
	ret = ricoh618_init_fgsoca(info);
#endif
	queue_delayed_work(info->monitor_wqueue, &info->monitor_work,
			   info->monitor_time);

	/* Enable Charger interrupt */
//	ricoh618_set_bits(info->dev->parent, RICOH618_INTC_INTEN, CHG_INT);
	ricoh618_set_bits(info->dev->parent, RICOH618_INTC_INTEN, CHG_INT | ADC_INT);

	return 0;

out:
	kfree(info);
	return ret;
}

static int __devexit ricoh618_battery_remove(struct platform_device *pdev)
{
	struct ricoh618_battery_info *info = platform_get_drvdata(pdev);
	uint8_t val;
	int ret;
	int err;
	int cc_cap = 0;
	bool is_charging = true;

	if (g_fg_on_mode
		 && (info->soca->status == RICOH618_SOCA_STABLE)) {
		err = ricoh618_write(info->dev->parent, PSWR_REG, 0x7f);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");
		g_soc = 0x7f;
	} else {
		if (info->soca->displayed_soc < 0) {
			val = 0;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		ret = ricoh618_write(info->dev->parent, PSWR_REG, val);
		if (ret < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;

		ret = calc_capacity_in_period(info, &cc_cap, &is_charging);
		if (ret < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");
	}

	if (g_fg_on_mode == 0) {
		ret = ricoh618_clr_bits(info->dev->parent,
					 FG_CTRL_REG, 0x01);
		if (ret < 0)
			dev_err(info->dev, "Error in clr FG EN\n");
	}

	/* set rapid timer 300 min */
	err = ricoh618_set_bits(info->dev->parent, TIMSET_REG, 0x03);
	if (err < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
	}

	free_irq(IRQ_RESERVED_BASE + RICOH618_IRQ_FONCHGINT, &info);
	free_irq(IRQ_RESERVED_BASE + RICOH618_IRQ_FCHGCMPINT, &info);
	free_irq(IRQ_RESERVED_BASE + RICOH618_IRQ_FVUSBDETSINT, &info);
	free_irq(IRQ_RESERVED_BASE + RICOH618_IRQ_FVADPDETSINT, &info);
#ifdef ENABLE_LOW_BATTERY_DETECTION
	free_irq(IRQ_RESERVED_BASE + RICOH618_IRQ_VSYSLIR, &info);
#endif


	cancel_delayed_work(&info->monitor_work);
	cancel_delayed_work(&info->charge_stable_work);
	cancel_delayed_work(&info->charge_monitor_work);
	cancel_delayed_work(&info->get_charge_work);
	cancel_delayed_work(&info->displayed_work);
	cancel_delayed_work(&info->changed_work);
#ifdef ENABLE_LOW_BATTERY_DETECTION
	cancel_delayed_work(&info->low_battery_work);
#endif
#ifdef ENABLE_FACTORY_MODE
	cancel_delayed_work(&info->factory_mode_work);
#endif
	cancel_delayed_work(&info->jeita_work);
	cancel_work_sync(&info->irq_work);

	flush_workqueue(info->monitor_wqueue);
	flush_workqueue(info->workqueue);
#ifdef ENABLE_FACTORY_MODE
	flush_workqueue(info->factory_mode_wqueue);
#endif

	destroy_workqueue(info->monitor_wqueue);
	destroy_workqueue(info->workqueue);
#ifdef ENABLE_FACTORY_MODE
	destroy_workqueue(info->factory_mode_wqueue);
#endif

	power_supply_unregister(&info->battery);
	kfree(info);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int ricoh618_battery_suspend(struct device *dev)
{
	uint8_t val;
	int ret;
	int err;
	int cc_cap = 0;
	bool is_charging = true;
	struct ricoh618_battery_info *info = dev_get_drvdata(dev);

#ifdef ENABLE_MASKING_INTERRUPT_IN_SLEEP
	ricoh618_clr_bits(dev->parent, RICOH618_INTC_INTEN, CHG_INT);
#endif

	if (g_fg_on_mode
		 && (info->soca->status == RICOH618_SOCA_STABLE)) {
		err = ricoh618_write(info->dev->parent, PSWR_REG, 0x7f);
		if (err < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");
		 g_soc = 0x7F;
		info->soca->suspend_soc = (info->soca->displayed_soc + 50)/100;
	} else {
		if (info->soca->displayed_soc < 0) {
			val = 0;
		} else {
			val = (info->soca->displayed_soc + 50)/100;
			val &= 0x7f;
		}
		ret = ricoh618_write(info->dev->parent, PSWR_REG, val);
		if (ret < 0)
			dev_err(info->dev, "Error in writing PSWR_REG\n");

		g_soc = val;

		info->soca->suspend_soc = (info->soca->displayed_soc + 50)/100;
	}

	ret = calc_capacity_in_period(info, &cc_cap, &is_charging);
	if (ret < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");

	if (info->soca->status == RICOH618_SOCA_STABLE
		|| info->soca->status == RICOH618_SOCA_FULL)
		info->soca->status = RICOH618_SOCA_DISP;

	/* set rapid timer 300 min */
	err = ricoh618_set_bits(info->dev->parent, TIMSET_REG, 0x03);
	if (err < 0) {
		dev_err(info->dev, "Error in writing the control register\n");
	}

	flush_delayed_work(&info->monitor_work);
	flush_delayed_work(&info->displayed_work);
	flush_delayed_work(&info->charge_stable_work);
	flush_delayed_work(&info->charge_monitor_work);
	flush_delayed_work(&info->get_charge_work);
	flush_delayed_work(&info->changed_work);
#ifdef ENABLE_LOW_BATTERY_DETECTION
	flush_delayed_work(&info->low_battery_work);
#endif
#ifdef ENABLE_FACTORY_MODE
	flush_delayed_work(&info->factory_mode_work);
#endif

/*	flush_work(&info->irq_work); */

	return 0;
}

static int ricoh618_battery_resume(struct device *dev)
{
	struct ricoh618_battery_info *info = dev_get_drvdata(dev);
	uint8_t val;
	int ret;
	int displayed_soc_temp;
	int cc_cap = 0;
	bool is_charging = true;
	bool is_jeita_updated;
	int i;
	int delta_time;								//ADDED to solve the inpulse bug 20131015
#ifdef ENABLE_MASKING_INTERRUPT_IN_SLEEP
	ricoh618_set_bits(dev->parent, RICOH618_INTC_INTEN, CHG_INT);
#endif

	ret = check_jeita_status(info, &is_jeita_updated);
	if (ret < 0) {
		dev_err(info->dev, "Error in updating JEITA %d\n", ret);
	}

	if (info->entry_factory_mode) {
		info->soca->displayed_soc = -EINVAL;
	} else if (RICOH618_SOCA_ZERO == info->soca->status) {
		if (calc_ocv(info) > get_OCV_voltage(info, 0)) {
			ret = ricoh618_read(info->dev->parent, PSWR_REG, &val);
			val &= 0x7f;
			info->soca->soc = val * 100;
			if (ret < 0) {
				dev_err(info->dev,
					 "Error in reading PSWR_REG %d\n", ret);
				info->soca->soc
					 = calc_capacity(info) * 100;
			}

			ret = calc_capacity_in_period(info, &cc_cap,
								 &is_charging);
			if (ret < 0)
				dev_err(info->dev, "Read cc_sum Error !!-----\n");

			info->soca->cc_delta
				 = (is_charging == true) ? cc_cap : -cc_cap;

			displayed_soc_temp
				 = info->soca->soc + info->soca->cc_delta;
			if (displayed_soc_temp < 0)
				displayed_soc_temp = 0;

			displayed_soc_temp = min(10000, displayed_soc_temp);
			displayed_soc_temp = max(0, displayed_soc_temp);
			info->soca->displayed_soc = displayed_soc_temp;

			ret = ricoh618_write(info->dev->parent,
							 FG_CTRL_REG, 0x51);
			if (ret < 0)
				dev_err(info->dev, "Error in writing the control register\n");
			info->soca->ready_fg = 0;
			info->soca->status = RICOH618_SOCA_FG_RESET;

		} else
			info->soca->displayed_soc = 0;
	} else {
		info->soca->soc = info->soca->suspend_soc * 100;

		ret = calc_capacity_in_period(info, &cc_cap, &is_charging);
		if (ret < 0)
			dev_err(info->dev, "Read cc_sum Error !!-----\n");

		info->soca->cc_delta = (is_charging == true) ? cc_cap : -cc_cap;

		displayed_soc_temp = info->soca->soc + info->soca->cc_delta;
		if (displayed_soc_temp < 0)
				displayed_soc_temp = 0;

		displayed_soc_temp = min(10000, displayed_soc_temp);
		displayed_soc_temp = max(0, displayed_soc_temp);

		if (0 == info->soca->jt_limit) {
			check_charge_status_2(info, displayed_soc_temp);
		} else {
			info->soca->displayed_soc = displayed_soc_temp;
		}

		if (RICOH618_SOCA_DISP == info->soca->status) {
			info->soca->last_soc = calc_capacity(info) * 100;
			info->soca->soc_delta = 0;
		}
	}
	info->soca->update_count = 0;

	ret = measure_vbatt_FG(info, &info->soca->Vbat_ave);
	ret = measure_vsys_ADC(info, &info->soca->Vsys_ave);
	ret = measure_Ibatt_FG(info, &info->soca->Ibat_ave);

	power_supply_changed(&info->battery);
	queue_delayed_work(info->monitor_wqueue, &info->displayed_work, HZ);

	if (RICOH618_SOCA_UNSTABLE == info->soca->status) {
		info->soca->stable_count = 10;
		queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH618_FG_STABLE_TIME*HZ/10);
	} else if (RICOH618_SOCA_FG_RESET == info->soca->status) {
		info->soca->stable_count = 1;

		for (i = 0; i < 3; i = i+1)
			info->soca->reset_soc[i] = 0;
		info->soca->reset_count = 0;

		queue_delayed_work(info->monitor_wqueue,
					 &info->charge_stable_work,
					 RICOH618_FG_RESET_TIME*HZ);
	}


	queue_delayed_work(info->monitor_wqueue, &info->monitor_work,
					info->monitor_time);

	queue_delayed_work(info->monitor_wqueue, &info->charge_monitor_work,
					RICOH618_CHARGE_RESUME_TIME * HZ);

	info->soca->chg_count = 0;
	queue_delayed_work(info->monitor_wqueue, &info->get_charge_work,
					RICOH618_CHARGE_RESUME_TIME * HZ);
	if (info->jt_en) {
		if (!info->jt_hw_sw) {
			queue_delayed_work(info->monitor_wqueue, &info->jeita_work,
					RICOH618_JEITA_UPDATE_TIME * HZ);
		}
	}

	return 0;
}

static struct dev_pm_ops ricoh618_battery_pm_ops = {
	.suspend	= ricoh618_battery_suspend,
	.resume		= ricoh618_battery_resume,
};
#endif

static struct platform_driver ricoh618_battery_driver = {
	.driver	= {
				.name	= "ricoh618-battery",
				.owner	= THIS_MODULE,
#ifdef CONFIG_PM
				.pm	= &ricoh618_battery_pm_ops,
#endif
	},
	.probe	= ricoh618_battery_probe,
	.remove	= __devexit_p(ricoh618_battery_remove),
};

static int __init ricoh618_battery_init(void)
{
	return platform_driver_register(&ricoh618_battery_driver);
}
module_init(ricoh618_battery_init);

static void __exit ricoh618_battery_exit(void)
{
	platform_driver_unregister(&ricoh618_battery_driver);
}
module_exit(ricoh618_battery_exit);

MODULE_DESCRIPTION("RICOH618 Battery driver");
MODULE_ALIAS("platform:ricoh618-battery");
MODULE_LICENSE("GPL");
