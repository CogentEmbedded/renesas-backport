#ifndef __ASM_VIN_H__
#define __ASM_VIN_H__

#include <media/soc_camera.h>

#define VIN_INPUT_UNDEFINED		(0) /* input undefined */
#define VIN_INPUT_ITUR_BT656_8BIT	(1) /* use ITU-R BT.656 8bit */
#define VIN_INPUT_ITUR_BT709_24BIT	(2) /* use ITU-R BT.709 24bit */

#define VIN_FLAG_USE_8BIT_BUS	(1 << 0) /* use  8bit bus width */
#define VIN_FLAG_USE_16BIT_BUS	(1 << 1) /* use 16bit bus width */
#define VIN_FLAG_HSYNC_LOW	(1 << 2) /* default High if possible */
#define VIN_FLAG_VSYNC_LOW	(1 << 3) /* default High if possible */

struct vin_info {
	unsigned long input;
	unsigned long flags;
};

#endif /* __ASM_VIN_H__ */
