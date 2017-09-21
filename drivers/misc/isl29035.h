#ifndef __ISL29035_H__
#define __ISL29035_H__

/* -----------------------------------------------------------
 *  | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
 * -----------------------------------------------------------
 *  |   OPERATION MODE   |   RESERVED  |  INT |   PERSIST   |             COMMAND-I         0x0
 * -----------------------------------------------------------
 *  |          RESERVED         |  RESOLUTION |    RANGE    |             COMMAND-II        0x1
 * -----------------------------------------------------------
 */

/* RANGE :     0   -->    1000
               1   -->    4000
               2   -->    8000
               3   -->    16000
 */

/* PERSIST :   0   -->    1
               1   -->    4
               2   -->    8
               3   -->    16
 */

/* RESOLUTION :     0   -->    65536        105 ms
                    1   -->    4096         6.5 ms
                    2   -->    256          0.41 ms          // can not be used
                    3   -->    16           0.0256 ms        // can not be used
 */

/* Light value is 16-bit data, register 0x2 store low 8-bit data 
   and register 0x3 store high 8-bit data

   Interrupt is trigged while light value cross a limit window.
   Low limit value is 16-bit data, register 0x4 store low 8-bit data
   and register 0x5 store high 8-bit data
   High limit value is 16-bit data, register 0x6 store low 8-bit data
   and register 0x7 store high 8-bit data
*/

enum {
	POWER_DOWM = 0,
	ALS_ONCE,
	IR_ONCE,
	ALS_CONTINUE = 5,
	IR_CONTINUE,
};

#define WORK_MODE           ALS_CONTINUE
#define PERSIST                  1
#define LUX_RANGE                1
#define ADC_RES                  1
#define TOTAL_LEVEL              20

#define light_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0666,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#endif
