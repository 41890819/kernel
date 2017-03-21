/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __JZ4775_ICDC_H__
#define __JZ4775_ICDC_H__

/* JZ internal register space */
enum {
	JZ_ICDC_SR		= 0x00,
	JZ_ICDC_AICR_DAC	= 0x01,
	JZ_ICDC_AICR_ADC	= 0x02,
	JZ_ICDC_CR_LO		= 0x03,
	JZ_ICDC_CR_HP		= 0x04,

	JZ_ICDC_MISSING_REG1,

	JZ_ICDC_CR_DAC		= 0x06,
	JZ_ICDC_CR_MIC		= 0x07,
	JZ_ICDC_CR_LI		= 0x08,
	JZ_ICDC_CR_ADC		= 0x09,
	JZ_ICDC_CR_MIX		= 0x0a,
	JZ_ICDC_CR_VIC		= 0x0b,
	JZ_ICDC_CCR		= 0x0c,
	JZ_ICDC_FCR_DAC		= 0x0d,
	JZ_ICDC_FCR_ADC		= 0x0e,
	JZ_ICDC_ICR		= 0x0f,
	JZ_ICDC_IMR		= 0x10,
	JZ_ICDC_IFR		= 0x11,
	JZ_ICDC_GCR_HPL		= 0x12,
	JZ_ICDC_GCR_HPR		= 0x13,
	JZ_ICDC_GCR_LIBYL	= 0x14,
	JZ_ICDC_GCR_LIBYR	= 0x15,
	JZ_ICDC_GCR_DACL	= 0x16,
	JZ_ICDC_GCR_DACR	= 0x17,
	JZ_ICDC_GCR_MIC1	= 0x18,
	JZ_ICDC_GCR_MIC2	= 0x19,
	JZ_ICDC_GCR_ADCL	= 0x1a,
	JZ_ICDC_GCR_ADCR	= 0x1b,

	JZ_ICDC_MISSING_REG2,

	JZ_ICDC_GCR_MIXADC	= 0x1d,
	JZ_ICDC_GCR_MIXDAC	= 0x1e,
	JZ_ICDC_AGC1		= 0x1f,
	JZ_ICDC_AGC2		= 0x20,
	JZ_ICDC_AGC3		= 0x21,
	JZ_ICDC_AGC4		= 0x22,
	JZ_ICDC_AGC5		= 0x23,
	JZ_ICDC_MAX_REGNUM,

	/* virtual registers */
	JZ_ICDC_LHPSEL  = 0x24,
	JZ_ICDC_RHPSEL  = 0x25,

	JZ_ICDC_LLOSEL = 0x26,
	JZ_ICDC_RLOSEL = 0x27,

	JZ_ICDC_LINSEL	= 0x28,
	JZ_ICDC_RINSEL	= 0x29,

	JZ_ICDC_MAX_NUM
};

#define ICDC_REG_IS_MISSING(r) 	(( (r) == JZ_ICDC_MISSING_REG1) || ( (r) == JZ_ICDC_MISSING_REG2))

extern struct snd_soc_dai jz_icdc_dai;
extern struct snd_soc_codec_device jz_icdc_soc_codec_dev;

#define DLC_IFR_SCLR    (1 << 6)
#define DLC_IFR_JACK    (1 << 5)
#define DLC_IFR_SCMC2   (1 << 4)
#define DLC_IFR_RUP     (1 << 3)
#define DLC_IFR_RDO     (1 << 2)
#define DLC_IFR_GUP     (1 << 1)
#define DLC_IFR_GDO     (1 << 0)

#define IFR_ALL_FLAG    (DLC_IFR_SCLR | DLC_IFR_JACK | DLC_IFR_SCMC2 | DLC_IFR_RUP | DLC_IFR_RDO | DLC_IFR_GUP | DLC_IFR_GDO)

#define DLC_IMR_SCLR    (1 << 6)
#define DLC_IMR_JACK    (1 << 5)
#define DLC_IMR_SCMC2   (1 << 4)
#define DLC_IMR_RUP     (1 << 3)
#define DLC_IMR_RDO     (1 << 2)
#define DLC_IMR_GUP     (1 << 1)
#define DLC_IMR_GDO     (1 << 0)

#define ICR_ALL_MASK                                                    \
	        (DLC_IMR_SCLR | DLC_IMR_JACK | DLC_IMR_SCMC2 | DLC_IMR_RUP | DLC_IMR_RDO | DLC_IMR_GUP | DLC_IMR_GDO)

#define ICR_COMMON_MASK (ICR_ALL_MASK & (~DLC_IMR_SCLR) & (~DLC_IMR_SCMC2))

#define DLV_DEBUG_SEM(x,y...) //printk(x,##y);

#define DLV_LOCK()                                           \
	do{                                                  \
		if(g_dlv_sem)                                \
		if(down_interruptible(g_dlv_sem))            \
		return;		                             \
		DLV_DEBUG_SEM("dlvsemlock lock\n");          \
	}while(0)

#define DLV_UNLOCK()                                         \
	do{                                                  \
		if(g_dlv_sem)                                \
			up(g_dlv_sem);                               \
		DLV_DEBUG_SEM("dlvsemlock unlock\n");        \
	}while(0)

#define DLV_LOCKINIT()                                      \
	do{                                                 \
		if(g_dlv_sem == NULL)                       \
		g_dlv_sem = (struct semaphore *)vmalloc(sizeof(struct semaphore)); \
		if(g_dlv_sem)                               \
			sema_init(g_dlv_sem, 0);               \
		DLV_DEBUG_SEM("dlvsemlock init\n");         \
	}while(0)

#define DLV_LOCKDEINIT()                                     \
	do{                                                  \
		if(g_dlv_sem)                                \
			vfree(g_dlv_sem);                            \
		g_dlv_sem = NULL;                            \
		DLV_DEBUG_SEM("dlvsemlock deinit\n");        \
	}while(0)

#endif	/* __JZ4775_ICDC_H__ */
