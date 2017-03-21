#ifndef __DMIC_OPS_H_
#define __DMIC_OPS_H_

//#define DMIC_FIFO_THR	(48) /* 48 * 4Bytes, or 48 * 2 Bytes.*/
#define DMIC_FIFO_THR	(64) /* 48 * 4Bytes, or 48 * 2 Bytes.*/
//#define DMIC_FIFO_THR	(128)

enum dmic_status {
	WAITING_TRIGGER = 1,
	WAITING_DATA,
};

extern int dmic_current_state;
extern unsigned int cur_thr_value;
extern unsigned int wakeup_failed_times;

void dump_dmic_regs(void);


extern int dmic_config(void);
extern void dmic_init(void);
extern int dmic_enable(void);
extern int dmic_disable(void);

extern int dmic_handler(int);

extern void reconfig_thr_value();

extern int dmic_init_deep_sleep(void);

extern int dmic_init_record(void);

extern int dmic_init_mode(int mode);


extern int dmic_ioctl(int, unsigned long);

extern int cpu_should_sleep(void);

extern int dmic_disable_tri(void);
#endif




