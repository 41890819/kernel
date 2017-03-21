/* kernel/drivers/video/jz_ipu_v1_2/jz_ipu.c
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Input file for Ingenic IPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/suspend.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/earlysuspend.h>

#ifdef CONFIG_SOC_M200
#include <mach/jz_libdmmu.h>
#include <mach/libdmmu.h>
#endif

#include "jz_regs_v12.h"
#include "jz_ipu_v12.h"

#ifdef USE_FG0_ALPHA
extern int jzfb_set_fg0_alpha(int is_pixel_alpha, unsigned int alpha);
#endif

// #define TEST_IPU

#ifdef PHYS
#undef PHYS
#endif
#define PHYS(x) (x)

struct ipu_reg_struct jz_ipu_regs_name[] = {
	{"IPU_FM_CTRL", IPU_FM_CTRL},
	{"IPU_STATUS", IPU_STATUS},
	{"IPU_D_FMT", IPU_D_FMT},
	{"IPU_Y_ADDR", IPU_Y_ADDR},
	{"IPU_U_ADDR", IPU_U_ADDR},
	{"IPU_V_ADDR", IPU_V_ADDR},
	{"IPU_IN_FM_GS", IPU_IN_FM_GS},
	{"IPU_Y_STRIDE", IPU_Y_STRIDE},
	{"IPU_UV_STRIDE", IPU_UV_STRIDE},
	{"IPU_OUT_ADDR", IPU_OUT_ADDR},
	{"IPU_OUT_GS", IPU_OUT_GS},
	{"IPU_OUT_STRIDE", IPU_OUT_STRIDE},
	{"IPU_CSC_C0_COEF", IPU_CSC_C0_COEF},
	{"IPU_CSC_C1_COEF", IPU_CSC_C1_COEF},
	{"IPU_CSC_C2_COEF", IPU_CSC_C2_COEF},
	{"IPU_CSC_C3_COEF", IPU_CSC_C3_COEF},
	{"IPU_CSC_C4_COEF", IPU_CSC_C4_COEF},
	{"IPU_CSC_OFFSET_PARA", IPU_CSC_OFFSET_PARA},
	{"IPU_SRC_TLB_ADDR", IPU_SRC_TLB_ADDR},
	{"IPU_DEST_TLB_ADDR", IPU_DEST_TLB_ADDR},
	{"IPU_ADDR_CTRL", IPU_ADDR_CTRL},
	{"IPU_Y_ADDR_N", IPU_Y_ADDR_N},
	{"IPU_U_ADDR_N", IPU_U_ADDR_N},
	{"IPU_V_ADDR_N", IPU_V_ADDR_N},
	{"IPU_OUT_ADDR_N", IPU_OUT_ADDR_N},
	{"IPU_SRC_TLB_ADDR_N", IPU_SRC_TLB_ADDR_N},
	{"IPU_DEST_TLB_ADDR_N", IPU_DEST_TLB_ADDR_N},
	{"IPU_TRIG", IPU_TRIG},
	{"IPU_FM_XYOFT", IPU_FM_XYOFT},
	{"IPU_GLB_CTRL", IPU_GLB_CTRL},
	{"IPU_OSD_CTRL", IPU_OSD_CTRL},
	{"IPU_RSZ_COEF", IPU_RSZ_COEF},
	{"IPU_TLB_PARA", IPU_TLB_PARA},
	{"IPU_VADDR_STLB_ERR", IPU_VADDR_STLB_ERR},
	{"IPU_VADDR_DTLB_ERR", IPU_VADDR_DTLB_ERR}
};

// Debug info
static struct ipu_proc_info * get_ipu_procinfo(struct jz_ipu *ipu, struct file *filp);
static void dump_img(struct jz_ipu *ipu)
{
	struct ipu_img_param *img = NULL;
	struct ipu_proc_info *ipu_proc = NULL;

	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return;
	}

	ipu_proc = get_ipu_procinfo(ipu, ipu->cur_proc);
	if (ipu_proc == NULL) {
		return;
	}

	img = &ipu_proc->img;
	dev_info(ipu->dev, "ipu_cmd = %#x\n", img->ipu_cmd);
	dev_info(ipu->dev, "output_mode[%#x]\r\n", (unsigned int)img->output_mode);
	dev_info(ipu->dev, "in_width[%#x]\r\n", (unsigned int)img->in_width);
	dev_info(ipu->dev, "in_height[%#x]\r\n", (unsigned int)img->in_height);
	dev_info(ipu->dev, "in_bpp[%#x]\r\n", (unsigned int)img->in_bpp);
	dev_info(ipu->dev, "in_fmt[%#x]\n", (unsigned int)img->in_fmt);
	dev_info(ipu->dev, "out_fmt[%#x]\n", (unsigned int)img->out_fmt);
	dev_info(ipu->dev, "out_x[%#x]\n", (unsigned int)img->out_x);
	dev_info(ipu->dev, "out_y[%#x]\n", (unsigned int)img->out_y);
	dev_info(ipu->dev, "out_width[%#x]\r\n", (unsigned int)img->out_width);
	dev_info(ipu->dev, "out_height[%#x]\r\n", (unsigned int)img->out_height);
	dev_info(ipu->dev, "y_buf_v[%#x]\r\n", (unsigned int)img->y_buf_v);
	dev_info(ipu->dev, "u_buf_v[%#x]\r\n", (unsigned int)img->u_buf_v);
	dev_info(ipu->dev, "v_buf_v[%#x]\r\n", (unsigned int)img->v_buf_v);
	dev_info(ipu->dev, "y_buf_p[%#x]\r\n", (unsigned int)img->y_buf_p);
	dev_info(ipu->dev, "u_buf_p[%#x]\r\n", (unsigned int)img->u_buf_p);
	dev_info(ipu->dev, "v_buf_p[%#x]\r\n", (unsigned int)img->v_buf_p);
	dev_info(ipu->dev, "out_buf_v[%#x]\r\n", (unsigned int)img->out_buf_v);
	dev_info(ipu->dev, "out_buf_p[%#x]\r\n", (unsigned int)img->out_buf_p);
	dev_info(ipu->dev, "src_page_mapping[%#x]\r\n", (unsigned int)img->src_page_mapping);
	dev_info(ipu->dev, "dst_page_mapping[%#x]\r\n", (unsigned int)img->dst_page_mapping);
	dev_info(ipu->dev, "y_t_addr[%#x]\r\n", (unsigned int)img->y_t_addr);
	dev_info(ipu->dev, "u_t_addr[%#x]\r\n", (unsigned int)img->u_t_addr);
	dev_info(ipu->dev, "v_t_addr[%#x]\r\n", (unsigned int)img->v_t_addr);
	dev_info(ipu->dev, "out_t_addr[%#x]\r\n", (unsigned int)img->out_t_addr);
	dev_info(ipu->dev, "stride.y[%#x]\r\n", (unsigned int)img->stride.y);
	dev_info(ipu->dev, "stride.u[%#x]\r\n", (unsigned int)img->stride.u);
	dev_info(ipu->dev, "stride.v[%#x]\r\n", (unsigned int)img->stride.v);
	dev_info(ipu->dev, "Wdiff[%#x]\r\n", (unsigned int)img->Wdiff);
	dev_info(ipu->dev, "Hdiff[%#x]\r\n", (unsigned int)img->Hdiff);
	dev_info(ipu->dev, "zoom_mode[%#x]\r\n", (unsigned int)img->zoom_mode);
}

static int jz_dump_ipu_regs(struct jz_ipu *ipu)
{
	int i, total;
	struct ipu_proc_info *ipu_proc = NULL;

	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL!\n");
		return -1;
	}

	ipu_proc = get_ipu_procinfo(ipu, ipu->cur_proc);
	if (ipu_proc == NULL) {
		return -1;
	}

	total = sizeof(jz_ipu_regs_name) / sizeof(struct ipu_reg_struct);
	for (i = 0; i < total; i++) {
		dev_info(ipu->dev, "%-20s :  0x%08x\r\n",
			 jz_ipu_regs_name[i].name,
			 reg_read(ipu, jz_ipu_regs_name[i].addr));
	}

	return 1;
}

static int ipu_dump_regs(struct jz_ipu *ipu)
{
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	dev_info(ipu->dev, "ipu->base: %p\n", ipu->iomem);
	dump_img(ipu);
	jz_dump_ipu_regs(ipu);

	return 0;
}

// Bit Handler : used in jz_regs_v12.h macro definition
static void bit_set(struct jz_ipu *ipu, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = reg_read(ipu, offset);
	tmp |= bit;
	reg_write(ipu, offset, tmp);
}

static void bit_clr(struct jz_ipu *ipu, int offset, unsigned int bit)
{
	unsigned int tmp = 0;

	tmp = reg_read(ipu, offset);
	tmp &= ~(bit);
	reg_write(ipu, offset, tmp);
}

// IPU Proc Handler Functions
static int create_proc_info(struct jz_ipu *ipu, struct file *filp)
{
	struct ipu_proc_info *ipu_proc = NULL;

	ipu_proc = kzalloc(sizeof(struct ipu_proc_info), GFP_KERNEL);
	if (ipu_proc == NULL) {
		dev_err(ipu->dev, "kzalloc proc info failed !");
		return -1;
	}

	ipu_proc->pid = current->pid;
	ipu_proc->ipu_filp = filp;
	list_add_tail(&ipu_proc->list, &ipu->process_list);

	return 0;
}

static int destroy_proc_info(struct jz_ipu *ipu, struct file *filp)
{
	struct ipu_proc_info *ipu_proc = NULL;

	ipu_proc = get_ipu_procinfo(ipu, filp);
	if (ipu_proc == NULL) {
		dev_err(ipu->dev, "proc info to destroy not found !");
		return -1;
	}

	list_del_init(&ipu_proc->list);
	kfree(ipu_proc);

	return 0;
}

static struct ipu_proc_info * get_ipu_procinfo(struct jz_ipu *ipu, struct file *filp)
{
	struct ipu_proc_info *ipu_proc = NULL;

	list_for_each_entry(ipu_proc, &ipu->process_list, list) {
		if (ipu_proc != NULL && ipu_proc->ipu_filp == filp)
			return ipu_proc;
	}
	dev_err(ipu->dev, "[%d, %d] Related IPU process info not found !",
		current->pid, current->tgid);

	return NULL;
}

// HAL Interface : hardware/libhardware/include/hardware/hardware.h
static unsigned int hal_infmt_is_packaged(int hal_fmt)
{
	unsigned int is_packaged = 0;

	switch (hal_fmt) {
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
	case HAL_PIXEL_FORMAT_YCbCr_420_SP:
	case HAL_PIXEL_FORMAT_YCbCr_422_P:
	case HAL_PIXEL_FORMAT_YCbCr_420_P:
	case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
	case HAL_PIXEL_FORMAT_YCbCr_420_B:
	case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
		is_packaged = 0;
		break;
	case HAL_PIXEL_FORMAT_RGBA_5551:
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_RGB_888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_BGRX_8888:
	case HAL_PIXEL_FORMAT_RGB_565:
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
	case HAL_PIXEL_FORMAT_YCbCr_420_I:
	default:
		is_packaged = 1;
		break;
	}

	return is_packaged;
}

static unsigned int hal_to_ipu_infmt(int hal_fmt)
{
	unsigned int ipu_fmt = IN_FMT_YUV420;

	switch (hal_fmt) {
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
		ipu_fmt = IN_FMT_YUV422;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_420_SP:
		ipu_fmt = IN_FMT_YUV420;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_422_P:
		ipu_fmt = IN_FMT_YUV422;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
		ipu_fmt = IN_FMT_YUV422;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_420_I:
		ipu_fmt = IN_FMT_YUV420;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_420_B:
	case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
		ipu_fmt = IN_FMT_YUV420_B;
		break;
	case HAL_PIXEL_FORMAT_RGBA_5551:
		ipu_fmt = IN_FMT_RGB_555;
		break;
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_RGB_888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_BGRX_8888:
		ipu_fmt = IN_FMT_RGB_888;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
		ipu_fmt = IN_FMT_RGB_565;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_420_P:
	case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
	default:
		ipu_fmt = IN_FMT_YUV420;
		break;
	}

	return ipu_fmt;
}

static unsigned int hal_to_ipu_outfmt(int hal_fmt)
{
	unsigned int ipu_fmt = OUT_FMT_RGB888;

	switch (hal_fmt) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_RGB_888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_BGRX_8888:
		ipu_fmt = OUT_FMT_RGB888;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
		ipu_fmt = OUT_FMT_RGB565;
		break;
	case HAL_PIXEL_FORMAT_RGBA_5551:
		ipu_fmt = OUT_FMT_RGB555;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
		ipu_fmt = OUT_FMT_YUV422;
		break;
	}

	return ipu_fmt;
}

static unsigned int get_out_fmt_rgb_order(int hal_out_fmt)
{
	unsigned int order = RGB_OUT_OFT_RGB;

	switch (hal_out_fmt) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
		order = RGB_OUT_OFT_BGR;
		break;
	case HAL_PIXEL_FORMAT_RGB_888:
	case HAL_PIXEL_FORMAT_RGB_565:
	case HAL_PIXEL_FORMAT_BGRA_8888:
	case HAL_PIXEL_FORMAT_BGRX_8888:
	case HAL_PIXEL_FORMAT_RGBA_5551:
	default:
		order = RGB_OUT_OFT_RGB;
		break;
	}

	return order;
}


// IPU Handler Functions
static int set_gs_regs(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	struct ipu_img_param *img = NULL;
	unsigned int tmp;
	int Wdiff, Hdiff, outW, outH;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;

	outW = img->out_width;
	outH = img->out_height;
	Wdiff = img->Wdiff;
	Hdiff = img->Hdiff;

	switch (hal_to_ipu_infmt(img->in_fmt)) {
	case IN_FMT_YUV420:
	case IN_FMT_YUV420_B:
		Hdiff = (Hdiff + 1) & ~1;
		Wdiff = (Wdiff + 1) & ~1;
		break;
	case IN_FMT_YUV422:
		Wdiff = (Wdiff + 1) & ~1;
		break;
	case IN_FMT_YUV411:
	case IN_FMT_RGB_888:
		break;
	default:
		dev_err(ipu->dev, "Input data format isn't support\n");
		return -1;
	}

	switch (hal_to_ipu_outfmt(img->out_fmt)) {
	case OUT_FMT_RGB888:
		outW = outW << 2;
		break;
	case OUT_FMT_RGB555:
	case OUT_FMT_RGB565:
		outW = outW << 1;
		break;
	default:
		dev_err(ipu->dev, "Output data format isn't support\n");
		return -1;
	}

	tmp = IN_FM_W(ipu_proc->img.in_width - Wdiff) |
		IN_FM_H((ipu_proc->img.in_height - Hdiff) & ~0x1);
	reg_write(ipu, IPU_IN_FM_GS, tmp);
	tmp = OUT_FM_W(outW) | OUT_FM_H(outH);
	reg_write(ipu, IPU_OUT_GS, tmp);

	return 0;
}

static void config_osd_regs(struct jz_ipu *ipu, int is_pixel_alpha)
{
	unsigned int tmp;

	if (is_pixel_alpha)
		tmp = GLB_ALPHA(0xff) | MOD_OSD(0x3) | OSD_PM;
	else
		tmp = GLB_ALPHA(0xff) | MOD_OSD(0x1) | OSD_PM;
	reg_write(ipu, IPU_OSD_CTRL, tmp);
}

static int set_ipu_resize(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	struct ipu_img_param *img;
	unsigned int tmp;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;

	if (img->out_width != img->in_width)
		__enable_hrsz();
	else
		__disable_hrsz();

	if (img->out_height != img->in_height)
		__enable_vrsz();
	else
		__disable_vrsz();

	if (img->zoom_mode != ZOOM_MODE_BILINEAR) {
		__sel_zoom_mode();
	} else {
		__disable_zoom_mode();
	}

	tmp = VCOEF(img->v_coef) | HCOEF(img->h_coef);
	reg_write(ipu, IPU_RSZ_COEF, tmp);

	return 0;
}

static int set_xy_offset(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	unsigned int tmp;
	struct ipu_img_param *img = NULL;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;
	tmp = SCREEN_XOFT(img->out_x) | SCREEN_YOFT(img->out_y);
	reg_write(ipu, IPU_FM_XYOFT, tmp);

	return 0;
}

static int set_csc_cfg(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	struct ipu_img_param *img = NULL;
	unsigned int in_fmt, out_fmt;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;

	in_fmt = hal_to_ipu_infmt(img->in_fmt);
	out_fmt = hal_to_ipu_outfmt(img->out_fmt);

	// Set csc coefficient
	if ((in_fmt != IN_FMT_YUV444) && (out_fmt != OUT_FMT_YUV422)) {
		__enable_csc_mode();
		reg_write(ipu, IPU_CSC_C0_COEF, YUV_CSC_C0);
		reg_write(ipu, IPU_CSC_C1_COEF, YUV_CSC_C1);
		reg_write(ipu, IPU_CSC_C2_COEF, YUV_CSC_C2);
		reg_write(ipu, IPU_CSC_C3_COEF, YUV_CSC_C3);
		reg_write(ipu, IPU_CSC_C4_COEF, YUV_CSC_C4);
		reg_write(ipu, IPU_CSC_OFFSET_PARA, YUV_CSC_OFFSET_PARA);
	} else {
		__disable_csc_mode();
		reg_write(ipu, IPU_CSC_OFFSET_PARA, 0x0);
	}

	return 0;
}

static int set_data_format(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	struct ipu_img_param *img = NULL;
	unsigned int tmp;
	int in_fmt, out_fmt, out_rgb_order;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;

	in_fmt = hal_to_ipu_infmt(img->in_fmt);
	out_fmt = hal_to_ipu_outfmt(img->out_fmt);
	out_rgb_order = get_out_fmt_rgb_order(img->out_fmt);

	if (in_fmt == IN_FMT_YUV422) {
	// FIXME : we need export interface to choose YUV order for user
#if defined(CONFIG_LCD_ECX336AF)
		in_fmt |= IN_OFT_Y1UY0V;
#else
		in_fmt |= IN_OFT_Y1VY0U;
#endif
	}

	if (out_fmt == OUT_FMT_YUV422)
		out_fmt |= YUV_PKG_OUT_OFT_VY1UY0;

	tmp = in_fmt | out_fmt | out_rgb_order;
	reg_write(ipu, IPU_D_FMT, tmp);

	if (in_fmt == IN_FMT_YUV420_B) {
		__enable_blk_mode();
	}

	if (hal_infmt_is_packaged(img->in_fmt)) {
		__enable_pkg_mode();
	} else {
		__disable_pkg_mode();
	}

	return 0;
}

static int set_data_stride(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	struct ipu_img_param *img = NULL;
	unsigned int tmp;
	int in_fmt, out_fmt;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;

	in_fmt = hal_to_ipu_infmt(img->in_fmt);
	out_fmt = hal_to_ipu_outfmt(img->out_fmt);

	// Set input stride
	if (img->stride.y == 0) { // set default stride

		if (in_fmt == IN_FMT_YUV420_B) {
			reg_write(ipu, IPU_Y_STRIDE, img->in_width * 16);
		} else if (reg_read(ipu, IPU_FM_CTRL) & SPKG_SEL) {
			reg_write(ipu, IPU_Y_STRIDE, img->in_width * 2);
		} else {
			reg_write(ipu, IPU_Y_STRIDE, img->in_width);
		}

		switch (in_fmt) {
		case IN_FMT_YUV420:
		case IN_FMT_YUV422:
			tmp = U_STRIDE(img->in_width / 2) | V_STRIDE(img->in_width / 2);
			reg_write(ipu, IPU_UV_STRIDE, tmp);
			break;
		case IN_FMT_YUV420_B:
			tmp = U_STRIDE(img->in_width * 8) | V_STRIDE(img->in_width * 8);
			reg_write(ipu, IPU_UV_STRIDE, tmp);
			break;
		case IN_FMT_YUV444:
			tmp = U_STRIDE(img->in_width) | V_STRIDE(img->in_width);
			reg_write(ipu, IPU_UV_STRIDE, tmp);
			break;
		case IN_FMT_YUV411:
			tmp = U_STRIDE(img->in_width / 4) | V_STRIDE(img->in_width / 4);
			reg_write(ipu, IPU_UV_STRIDE, tmp);
			break;
		default:
			dev_err(ipu->dev, "Input data format isn't support\n");
			return -1;
		}
	} else {
		reg_write(ipu, IPU_Y_STRIDE, img->stride.y);
		tmp = U_STRIDE(img->stride.u) | V_STRIDE(img->stride.v);
		reg_write(ipu, IPU_UV_STRIDE, tmp);
	}


	// Set output stride
	if (img->stride.out == 0) { // set default stride
		switch (img->output_mode & IPU_OUTPUT_MODE_MASK) {
		case IPU_OUTPUT_TO_LCD_FG1:
			break;
		case IPU_OUTPUT_TO_LCD_FB0:
		case IPU_OUTPUT_TO_LCD_FB1:
			tmp = img->fb_w * img->in_bpp >> 3;
			reg_write(ipu, IPU_OUT_STRIDE, tmp);
			break;
		case IPU_OUTPUT_TO_FRAMEBUFFER:
		default:
			switch (out_fmt) {
			default:
			case OUT_FMT_RGB888:
				tmp = img->out_width << 2;
				break;
			case OUT_FMT_RGB555:
			case OUT_FMT_RGB565:
				tmp = img->out_width << 1;
				break;
			}
			reg_write(ipu, IPU_OUT_STRIDE, tmp);
			break;
		}
	} else {
		reg_write(ipu, IPU_OUT_STRIDE, img->stride.out);
	}

	return 0;
}

static void enable_ctrl_regs(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	unsigned int tmp = 0;

	if (ipu_proc->img.output_mode & IPU_OUTPUT_TO_LCD_FG1) {
		// No need to make destination TLB base and address ready
		tmp = 0xffffffd7;
	} else {
		tmp = 0xffffffff;
	}
	reg_write(ipu, IPU_ADDR_CTRL, tmp);
	tmp = reg_read(ipu, IPU_ADDR_CTRL);
}

static int jz_ipu_init(struct jz_ipu *ipu, struct ipu_proc_info *ipu_proc)
{
	struct ipu_img_param *img = NULL;
	int is_pixel_alpha = 0;
	int ret = 0;

	if (ipu == NULL || ipu_proc == NULL)
		return -1;

	img = &ipu_proc->img;

	// Enable IPU Mode In LCDC
	if (img->output_mode & IPU_OUTPUT_TO_LCD_FG1) {
		ipu->cur_output_mode = IPU_OUTPUT_TO_LCD_FG1;
		__enable_lcdc_mode();
	}

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		ipu->cur_output_mode = IPU_OUTPUT_BLOCK_MODE;
		__disable_lcdc_mode();
	}

	// Set output position on screen
	if (img->output_mode & IPU_OUTPUT_TO_LCD_FG1) {
		set_xy_offset(ipu, ipu_proc);
#ifdef USE_FG0_ALPHA
		is_pixel_alpha = jzfb_set_fg0_alpha(1, 0xFF);
#endif
	}

	ret = set_data_format(ipu, ipu_proc);
	if (ret != 0) {
		dev_err(ipu->dev, "set_data_format error : out!\n");
		return -1;
	}

	ret = set_gs_regs(ipu, ipu_proc);
	if (ret != 0) {
		dev_err(ipu->dev, "set_gs_regs error : out!\n");
		return -1;
	}

	ret = set_ipu_resize(ipu, ipu_proc);
	if (ret != 0) {
		dev_err(ipu->dev, "set_ipu_resize error : out!\n");
		return -1;
	}

	ret = set_csc_cfg(ipu, ipu_proc);
	if (ret != 0) {
		dev_err(ipu->dev, "set_csc_cfg error : out!\n");
		return -1;
	}

	ret = set_data_stride(ipu, ipu_proc);
	if (ret != 0) {
		dev_err(ipu->dev, "set_data_stride error : out!\n");
		return -1;
	}

	// Set relative TLB register
	if (img->stlb_base) {
		reg_write(ipu, IPU_SRC_TLB_ADDR, img->stlb_base);
	}

	if (img->dtlb_base) {
		reg_write(ipu, IPU_DEST_TLB_ADDR, img->dtlb_base);
	}

	if (img->stlb_base || img->dtlb_base)
		reg_write(ipu, IPU_TLB_PARA, (DMMU_PTE_CHECK_PAGE_VALID << 16 | DMMU_PTE_CHECK_PAGE_VALID));

	config_osd_regs(ipu, is_pixel_alpha);

	enable_ctrl_regs(ipu, ipu_proc);

	ipu->inited = 1;

	return 0;
}

static int ipu_init(struct jz_ipu *ipu, struct ipu_img_param *imgp, struct file *filp)
{
	struct ipu_proc_info *ipu_proc = NULL;

	if (ipu == NULL) {
		printk("%s : IPU is NULL!\n", __func__);
		return -1;
	}

	if (imgp == NULL) {
		printk("ipu_init : No parameters passed in.\n");
		return -1;
	}

	ipu_proc = get_ipu_procinfo(ipu, filp);
	if (ipu_proc == NULL) {
		return -1;
	}

	dev_dbg(ipu->dev, "ipu_init : %d process init now.\n", current->pid);

	ipu->cur_proc = filp;

	if (clk_is_enabled(ipu->clk) == 0)
		clk_enable(ipu->clk);

	__reset_ipu();

	memcpy(&ipu_proc->img, imgp, sizeof(struct ipu_img_param));

	return jz_ipu_init(ipu, ipu_proc);
}

static int ipu_start(struct jz_ipu *ipu)
{
	unsigned long irq_flags;
	struct ipu_img_param *img = NULL;
	struct ipu_proc_info *ipu_proc = NULL;
	int timeout = 0;

	if (ipu == NULL) {
		printk("%s : IPU is NULL!\n", __func__);
		return -1;
	}

	ipu_proc = get_ipu_procinfo(ipu, ipu->cur_proc);
	if (ipu_proc == NULL) {
		return -1;
	}

	mutex_lock(&ipu->run_lock);

	if (ipu->suspend_entered) {
		// IPU already suspend, do nothing here
		mutex_lock(&ipu->run_lock);
		return 0;
	}

	img = &ipu_proc->img;

	__clear_ipu_out_end();

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		/* Wait for current frame to finished */
		spin_lock_irqsave(&ipu->update_lock, irq_flags);
		ipu->frame_requested++;
		spin_unlock_irqrestore(&ipu->update_lock, irq_flags);
		__ipu_enable_irq();
	}

	// ipu_dump_regs(ipu);

	__start_ipu();

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		/* Wait for current frame to finished */
		if (ipu->frame_requested != ipu->frame_done) {
			timeout = wait_event_interruptible_timeout(ipu->frame_wq,
								   ipu->frame_done == ipu->frame_requested,
								   HZ);

			if (timeout <= 0) {
				dev_err(ipu->dev, "Wait ipu frame end timeout ...\n");
			}
		}
	}

	mutex_unlock(&ipu->run_lock);

	return 0;
}

static int ipu_setbuffer(struct jz_ipu *ipu, struct ipu_img_param *imgp, struct file *filp)
{
	unsigned int py_buf, pu_buf, pv_buf;
	unsigned int py_buf_v, pu_buf_v, pv_buf_v;
	unsigned int tmp;

	struct ipu_img_param *img = NULL;
	struct ipu_proc_info *ipu_proc = NULL;

	if (ipu == NULL) {
		printk("%s : IPU is NULL!\n", __func__);
		return -1;
	}

	ipu_proc = get_ipu_procinfo(ipu, filp);
	if (ipu_proc == NULL) {
		return -1;
	}

	mutex_lock(&ipu->run_lock);

	if (ipu->suspend_entered) {
		// IPU already suspend, do nothing here
		mutex_unlock(&ipu->run_lock);
		return 0;
	}

	// We need to init IPU in two situation :
	// 1. Last IPU user is different from this one
	// 2. IPU has not init
	if ((ipu->cur_proc != filp) || (ipu->inited == 0)) {
		ipu_init(ipu, imgp, filp);
	} else if (imgp != NULL) {
		// Update IPU parameters
		memcpy(&ipu_proc->img, imgp, sizeof(struct ipu_img_param));
	}

	img = &ipu_proc->img;

	dev_dbg(ipu->dev, "PAddr : 0x%08x 0x%08x 0x%08x. VAddr : 0x%08x 0x%08x 0x%08x.\n",
		(unsigned int)img->y_buf_p, (unsigned int)img->u_buf_p, (unsigned int)img->v_buf_p,
		(unsigned int)img->y_buf_v, (unsigned int)img->u_buf_v, (unsigned int)img->v_buf_v);
	dev_dbg(ipu->dev, "PAddr = 0x%08x, VAddr : = 0x%08x.\n",
		(unsigned int)img->out_buf_v, (unsigned int)img->out_buf_p);
	dev_dbg(ipu->dev, "spage_map : 0x%08x.  dpage_map = 0x%08x.",
		img->src_page_mapping, img->dst_page_mapping);

	// Set input buffer info
	py_buf = ((unsigned int)img->y_buf_p);
	pu_buf = ((unsigned int)img->u_buf_p);
	pv_buf = ((unsigned int)img->v_buf_p);

	py_buf_v = (unsigned int)img->y_buf_v;
	pu_buf_v = (unsigned int)img->u_buf_v;
	pv_buf_v = (unsigned int)img->v_buf_v;

	if (img->src_page_mapping == 0) {
		__disable_spage_map();
	} else if (py_buf_v == 0) {
		dev_warn(ipu->dev, "No mapping buffer, disable source page mapping !\n");
		img->src_page_mapping = 0;
		__disable_spage_map();
	} else {
		py_buf = py_buf_v;
		pu_buf = pu_buf_v;
		pv_buf = pv_buf_v;
		__enable_spage_map();
	}

	reg_write(ipu, IPU_Y_ADDR, py_buf);
	reg_write(ipu, IPU_U_ADDR, pu_buf);
	reg_write(ipu, IPU_V_ADDR, pv_buf);

	// Set output buffer info
	if (img->output_mode & IPU_OUTPUT_TO_LCD_FG1) {
		img->dst_page_mapping = 0;
		__disable_dpage_map();
	} else if (img->dst_page_mapping == 0) {
		__disable_dpage_map();

		tmp = PHYS((unsigned int)img->out_buf_p);
		if (tmp == 0) {
			dev_err(ipu->dev, "Out buffer (0x%08x) not found !\n", img->out_buf_p);
			mutex_unlock(&ipu->run_lock);
			return -1;
		}
		reg_write(ipu, IPU_OUT_ADDR, tmp);
	} else {
		tmp = PHYS((unsigned int)img->out_buf_v);
		if (tmp == 0) {
			dev_err(ipu->dev, " Out virtual buffer (%p) not found !\n", img->out_buf_v);
			img->dst_page_mapping = 0;
			__disable_dpage_map();

			tmp = PHYS((unsigned int)img->out_buf_p);
			if (tmp == 0) {
				dev_err(ipu->dev, "Out buffer (0x%08x) not found !\n", img->out_buf_p);
				mutex_unlock(&ipu->run_lock);
				return -1;
			}
			reg_write(ipu, IPU_OUT_ADDR, tmp);
		} else {
			reg_write(ipu, IPU_OUT_ADDR, tmp);
			__enable_dpage_map();
		}
	}

	set_data_stride(ipu, ipu_proc);

	// Set configure enable contorl, which indicate new ready configure
	tmp = reg_read(ipu, IPU_ADDR_CTRL);
	if (img->output_mode & IPU_OUTPUT_TO_LCD_FG1) {
		tmp &= ~D_RY;
		tmp |= (Y_RY | U_RY | V_RY | PTS_RY);
	} else {
		tmp |= (Y_RY | U_RY | V_RY | D_RY | PTS_RY | PTD_RY);
	}
	reg_write(ipu, IPU_ADDR_CTRL, tmp);

	// IPU no need to wait frame end, just start
	// If you require to wait, set block_mode and exec ipu_start
	if (img->output_mode & IPU_OUTPUT_TO_LCD_FG1)
		__start_ipu();

	mutex_unlock(&ipu->run_lock);

	return 0;
}

static int jz_ipu_stop(struct jz_ipu *ipu)
{
	unsigned long long clock_start;
	unsigned long long clock_now;

	if (ipu->cur_output_mode == IPU_OUTPUT_TO_LCD_FG1) {
		reg_write(ipu, IPU_TRIG, IPU_STOP_LCD);

#ifdef USE_FG0_ALPHA
		jzfb_set_fg0_alpha(0, 0xFF);
#endif
	} else {
		reg_write(ipu, IPU_TRIG, IPU_STOP);
	}

	clock_start = sched_clock();
	while (1) {
		if (reg_read(ipu, IPU_STATUS) & OUT_END)
			break;

		// max timeout 100ms
		clock_now = sched_clock();
		if ((clock_now - clock_start) > (30 * 1000000)) {
			dev_err(ipu->dev, "Wait ipu stop timeout ...\n");
			return -1;
		}
	}

	return 0;
}

static int ipu_stop(struct jz_ipu *ipu)
{
	if (ipu == NULL) {
		printk("%s : IPU is NULL!\n", __func__);
		return -1;
	}

	mutex_lock(&ipu->run_lock);
	if (ipu->suspend_entered) {
		// IPU already suspend, do nothing here
	} else {
		jz_ipu_stop(ipu);
	}
	mutex_unlock(&ipu->run_lock);

	return 0;
}

static int ipu_shut(struct jz_ipu *ipu)
{
	return 0;
}

static int ipu_set_bypass(struct jz_ipu *ipu)
{
	return 0;
}

static int ipu_clr_bypass(struct jz_ipu *ipu)
{
	return 0;
}

static int ipu_get_bypass_state(struct jz_ipu *ipu)
{
	return 0;
}

static int ipu_enable_clk(struct jz_ipu *ipu, unsigned int value)
{
	return 0;
}

static int ipu_to_buf(struct jz_ipu *ipu, unsigned int value)
{
	return 0;
}

static long ipu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ipu_img_param img;
	struct miscdevice *dev = file->private_data;
	struct jz_ipu *ipu = container_of(dev, struct jz_ipu, misc_dev);
	void __user *argp = (void __user *)arg;
	unsigned int value;

	if (_IOC_TYPE(cmd) != JZIPU_IOC_MAGIC) {
		dev_err(ipu->dev, "0x%08x is not IPU command !\n", cmd);
		return -EFAULT;
	}

	switch (cmd) {
	case IOCTL_IPU_START:
		ret = ipu_start(ipu);
		break;
	case IOCTL_IPU_INIT:
		if (copy_from_user(&img, argp, sizeof(struct ipu_img_param))) {
			ret = -EFAULT;
		} else {
			mutex_lock(&ipu->run_lock);
			if (ipu->suspend_entered) {
				// IPU already suspend, do nothing here
				ret = 0;
			} else {
				ret = ipu_init(ipu, &img, file);
			}
			mutex_unlock(&ipu->run_lock);
		}
		break;
	case IOCTL_IPU_SET_BUFF:
		if (copy_from_user(&img, argp, sizeof(struct ipu_img_param))) {
			ret = -EFAULT;
		} else {
			ret = ipu_setbuffer(ipu, &img, file);
		}
		break;
	case IOCTL_IPU_SHUT:
		ret = ipu_shut(ipu);
		break;
	case IOCTL_IPU_STOP:
		ret = ipu_stop(ipu);
		break;

	case IOCTL_IPU_DUMP_REGS:
		ret = ipu_dump_regs(ipu);
		break;
	case IOCTL_IPU_SET_BYPASS:
		ret = ipu_set_bypass(ipu);
		break;
	case IOCTL_IPU_CLR_BYPASS:
		ret = ipu_clr_bypass(ipu);
		break;
	case IOCTL_IPU_GET_BYPASS_STATE:
		ret = ipu_get_bypass_state(ipu);
		if (copy_to_user(argp, &ret, sizeof(int))) {
			dev_err(ipu->dev, "IOCTL_IPU_GET_BYPASS_STATE copy_to_user error !\n");
			ret = -EFAULT;
		}
		break;
	case IOCTL_IPU_ENABLE_CLK:
		if (copy_from_user(&value, argp, sizeof(int))) {
			ret = -EFAULT;
		} else {
			ret = ipu_enable_clk(ipu, value);
		}
		break;
	case IOCTL_IPU_TO_BUF:
		if (copy_from_user(&value, argp, sizeof(int))) {
			ret = -EFAULT;
		} else {
			ret = ipu_to_buf(ipu, value);
		}
		break;
	case IOCTL_IPU_GET_PBUFF:
#ifdef TEST_IPU
		{
			unsigned int pbuff = ipu->ipumem_p;
			if (copy_to_user(argp, &pbuff, sizeof(unsigned int))) {
				dev_err(ipu->dev, "IOCTL_IPU_GET_PBUFF copy_to_user error !\n");
				ret = -EFAULT;
			}
		}
#endif
		break;
#ifdef CONFIG_SOC_M200
	case IOCTL_IPU_DMMU_MAP:
		{
			struct ipu_dmmu_map_info di;
			if (copy_from_user(&di, (void *)arg, sizeof(di))) {
				ret = -EFAULT;
			} else {
				ret = dmmu_map(di.addr, di.len);
			}
			break;
		}
		break;
	case IOCTL_IPU_DMMU_UNMAP:
		{
			struct ipu_dmmu_map_info di;
			if (copy_from_user(&di, (void *)arg, sizeof(di))) {
				ret = -EFAULT;
			} else {
				ret = dmmu_unmap(di.addr, di.len);
			}
			break;
		}
		break;
	case IOCTL_IPU_DMMU_UNMAP_ALL:
		dmmu_unmap_all();
		break;
#endif
	default:
		dev_err(ipu->dev, "Invalid IPU command: 0x%08x\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ipu_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice *dev = filp->private_data;
	struct jz_ipu *ipu = container_of(dev, struct jz_ipu, misc_dev);

	mutex_lock(&ipu->lock);
	dev_dbg(ipu->dev, "[ ipu_open ] pid=%d, tgid=%d filp=%p\n",
		current->pid, current->tgid, filp);

	ret = create_proc_info(ipu, filp);

	mutex_unlock(&ipu->lock);

	return ret;
}

static int ipu_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct jz_ipu *ipu = container_of(dev, struct jz_ipu, misc_dev);

	mutex_lock(&ipu->lock);
	dev_dbg(ipu->dev, "[ ipu_release ] pid=%d, tgid=%d filp=%p\n",
		current->pid, current->tgid, filp);

	destroy_proc_info(ipu, filp);

	if (list_empty(&ipu->process_list)) {
		ipu_stop(ipu);
		clk_disable(ipu->clk);
		ipu->inited = 0;
	}

	mutex_unlock(&ipu->lock);

	return 0;
}

static struct file_operations ipu_ops = {
	.owner = THIS_MODULE,
	.open = ipu_open,
	.release = ipu_release,
	.unlocked_ioctl = ipu_ioctl,
};

static irqreturn_t ipu_irq_handler(int irq, void *data)
{

	struct jz_ipu *ipu = (struct jz_ipu *)data;
	unsigned long irq_flags = 0;
	unsigned int dummy_read;

	dummy_read = reg_read(ipu, IPU_STATUS);	/* avoid irq looping or disable_irq */
	__ipu_disable_irq();

	if (dummy_read & 0x100)
		dev_err(ipu->dev, "stlb err addr=%x\n", reg_read(ipu, IPU_VADDR_STLB_ERR));

	if (dummy_read & 0x200)
		dev_err(ipu->dev, "dtlb err addr=%x\n", reg_read(ipu, IPU_VADDR_DTLB_ERR));

	if (dummy_read & 0x4)
		dev_err(ipu->dev, "size error\n");

	if (ipu->cur_output_mode == IPU_OUTPUT_BLOCK_MODE) {
		spin_lock_irqsave(&ipu->update_lock, irq_flags);
		ipu->frame_done = ipu->frame_requested;
		spin_unlock_irqrestore(&ipu->update_lock, irq_flags);
		wake_up(&ipu->frame_wq);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ipu_early_suspend(struct early_suspend *h)
{
	struct jz_ipu *ipu = container_of(h, struct jz_ipu, early_suspend);

	mutex_lock(&ipu->run_lock);
	ipu->suspend_entered = 1;
	jz_ipu_stop(ipu);
	clk_disable(ipu->clk);
	ipu->inited = 0; // IPU need init
	mutex_unlock(&ipu->run_lock);
}

static void ipu_late_resume(struct early_suspend *h)
{
	struct jz_ipu *ipu = container_of(h, struct jz_ipu, early_suspend);

	mutex_lock(&ipu->run_lock);
	ipu->suspend_entered = 0;
	mutex_unlock(&ipu->run_lock);
}
#endif

static ssize_t dump_mem(struct device *dev, struct device_attribute *attr, char *buf)
{
	show_mem(SHOW_MEM_FILTER_NODES);

	return 0;
}

static ssize_t dump_ipu(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jz_ipu *ipu = dev_get_drvdata(dev);

	jz_dump_ipu_regs(ipu);

	return 0;
}

static struct device_attribute ipu_sysfs_attrs[] = {
	__ATTR(dump_mem, S_IRUGO | S_IWUSR, dump_mem, NULL),
	__ATTR(dump_ipu, S_IRUGO | S_IWUSR, dump_ipu, NULL),
};

static int ipu_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct jz_ipu *ipu;

	ipu = (struct jz_ipu *)kzalloc(sizeof(struct jz_ipu), GFP_KERNEL);
	if (ipu == NULL) {
		dev_err(&pdev->dev, "alloc ipu mem_region failed!\n");
		return -ENOMEM;
	}

	sprintf(ipu->name, "ipu");
	ipu->inited = 0;
	ipu->suspend_entered = 0;
	ipu->misc_dev.minor = MISC_DYNAMIC_MINOR;
	ipu->misc_dev.name = ipu->name;
	ipu->misc_dev.fops = &ipu_ops;
	ipu->dev = &pdev->dev;

	ipu->clk = clk_get(ipu->dev, ipu->name);
	if (IS_ERR(ipu->clk)) {
		ret = dev_err(&pdev->dev, "ipu clk get failed!\n");
		goto err_get_clk;
	}

	ipu->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (ipu->res == NULL) {
		dev_err(&pdev->dev, "failed to get dev resources: %d\n", ret);
		goto err_get_res;
	}
	ipu->res = request_mem_region(ipu->res->start,
				      ipu->res->end - ipu->res->start + 1,
				      pdev->name);
	if (ipu->res == NULL) {
		dev_err(&pdev->dev, "failed to request regs memory region\n");
		goto err_get_res;
	}

	ipu->iomem = ioremap(ipu->res->start, resource_size(ipu->res));
	if (ipu->iomem == NULL) {
		dev_err(&pdev->dev, "failed to remap regs memory region: %d\n",	ret);
		goto err_ioremap;
	}

	ipu->irq = platform_get_irq(pdev, 0);
	if (request_irq(ipu->irq, ipu_irq_handler, IRQF_SHARED, ipu->name, ipu)) {
		dev_err(&pdev->dev, "request irq failed\n");
		goto err_req_irq;
	}

	dev_set_drvdata(&pdev->dev, ipu);

	ret = misc_register(&ipu->misc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "register misc device failed!\n");
		goto err_set_drvdata;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ipu->early_suspend.suspend = ipu_early_suspend;
	ipu->early_suspend.resume = ipu_late_resume;
	ipu->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 15;
	register_early_suspend(&ipu->early_suspend);
#endif

	mutex_init(&ipu->lock);
	mutex_init(&ipu->run_lock);

	spin_lock_init(&ipu->update_lock);
	init_waitqueue_head(&ipu->frame_wq);
	INIT_LIST_HEAD(&ipu->process_list);

	for (i = 0; i < ARRAY_SIZE(ipu_sysfs_attrs); i++) {
		ret = device_create_file(&pdev->dev, &ipu_sysfs_attrs[i]);
		if (ret)
			break;
	}

#ifdef TEST_IPU
	ipu->ipumem_v = kmalloc((1920 * 1080 * 4 * 2 + PAGE_SIZE), GFP_KERNEL);
	if (ipu->ipumem_v) {
		ipu->ipumem_v = (void *)((unsigned long)(ipu->ipumem_v + (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1)));
		ipu->ipumem_p = (unsigned long) virt_to_phys(ipu->ipumem_v);
		memset(ipu->ipumem_v, 0x00, size);
	} else {
		dev_err(ipu->dev, "IPU kmalloc is error\n");
		ipu->ipumem_v = NULL;
		ipu->ipumem_p = NULL;
	}
#endif

	return 0;

 err_set_drvdata:
	free_irq(ipu->irq, ipu);
 err_req_irq:
	iounmap(ipu->iomem);
 err_ioremap:
	release_mem_region(ipu->res->start, ipu->res->end - ipu->res->start + 1);
 err_get_res:
	clk_put(ipu->clk);
 err_get_clk:
	kfree(ipu);

	return -EINVAL;
}

static int ipu_remove(struct platform_device *pdev)
{
	int i = 0;
	struct jz_ipu *ipu = dev_get_drvdata(&pdev->dev);

	for (i = 0; i < ARRAY_SIZE(ipu_sysfs_attrs); i++) {
		device_remove_file(&pdev->dev, &ipu_sysfs_attrs[i]);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ipu->early_suspend);
#endif

	free_irq(ipu->irq, ipu);
	iounmap(ipu->iomem);
	release_mem_region(ipu->res->start, ipu->res->end - ipu->res->start + 1);
	clk_put(ipu->clk);
	kfree(ipu);

	return 0;
}

static struct platform_driver jz_ipu_driver = {
	.probe = ipu_probe,
	.remove = ipu_remove,
	.driver = {
		.name = "jz-ipu",
	},
};

static int __init ipu_setup(void)
{
	return platform_driver_register(&jz_ipu_driver);
}

static void __exit ipu_cleanup(void)
{
	platform_driver_unregister(&jz_ipu_driver);
}

module_init(ipu_setup);
module_exit(ipu_cleanup);

MODULE_DESCRIPTION("JZ IPU driver");
MODULE_AUTHOR("Derrick Kznan <Derrick.kznan@ingenic.cn>");
MODULE_LICENSE("GPL");
