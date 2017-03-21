#ifndef GP2AP050_LIB_H
#define GP2AP050_LIB_H

// Gesture Direction
#define GESTURE_DIR_RIGHT 			 0x8
#define GESTURE_DIR_LEFT 			 0x4
#define GESTURE_DIR_TOP 			 0x2
#define GESTURE_DIR_BOTTOM 			 0x1

// Gesture Zoom
#define GESTURE_ZOOM_LEVEL1			 3
#define GESTURE_ZOOM_LEVEL2			 5
#define GESTURE_ZOOM_LEVEL3			 6
#define GESTURE_ZOOM_LEVEL4			 7
#define GESTURE_ZOOM_LEVEL5			 9

// no gesture
#define GESTURE_NONE				 0

// for android environment
typedef int int16;
typedef char int1;
typedef char int8;


/* struct gs_params{ */
/* 	//// GS_PS_mode //// */
/* 	int gs_ps_mode; */
	
/* 	//// GS Wakeup/Shutdown flag//// */
/* 	int1 gs_enabled; */
/* 	//// Clear PROX & Interrupt FLAG registers for ONOFF Func//// */
/* 	int1 clear_int; */

/* 	//// Temporal parameters for Direction Judgement //// */
/* 	int max_x1, min_x1, max_y1, min_y1; */
/* 	int max_x2, min_x2, max_y2, min_y2, diff_max_x, diff_max_y; */

/* 	unsigned char x_plus, x_minus, y_plus, y_minus; */
/* 	unsigned char gs_state; */

/* 	int speed_counts; */
	
/* 	//// Thresholds depending on the performance //// */
/* 	signed int ignore_diff_th; */
/* 	unsigned int ignore_z_th; */

/* 	int ratio_th; */
	
/* 	//// Parameters for active offset cancelation //// */
/* 	int1 active_osc_on; */
/* 	int allowable_variation; */
/* 	int acquisition_num; */
/* 	int16 max_aoc_counts; */
/* 	int16 min_aoc_counts; */
	
/* 	//// Parameters for Zoom function //// */
/* 	int1 zoomFuncOn; */
/* 	int to_zoom_th; */
/* 	int out_zoom_th; */
/* 	int1 zoomModeNow; */
/* 	int zoom_mode_th; */
/* 	int16 zoom_z_th[5]; */
	
/* 	//// Saturation notification //// */
/* 	int saturated_data[4]; */

/* }; */



/* // ____________________________________________________________ */
/* // public */
/* #ifdef __cplusplus */
/* extern "C" { */
/* #endif */
/* void initGP2AP050Lib(void); */
/* void setGP2AP050Params(struct gs_params *p_gs); */
/* struct gs_params * getCurrentGP2AP050Params(struct gs_params *dst); */
/* struct gs_params * getCurrentGP2AP050ParamsDirect(); */
/* int getGP2AP050Gesture(int* raw_data); */
/* #ifdef __cplusplus */
/* } */
/* #endif */

#endif//GP2AP050_LIB_H
