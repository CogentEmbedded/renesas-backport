#ifndef __ASM_R8A7790_H__
#define __ASM_R8A7790_H__

#include <asm/mach/time.h>

struct rcar_du_platform_data;

void r8a7790_add_standard_devices(void);
void r8a7790_add_du_device(struct rcar_du_platform_data *pdata);
void r8a7790_clock_init(void);
void r8a7790_pinmux_init(void);

extern struct sys_timer r8a7790_timer;

#define MD(nr) BIT(nr)
u32 r8a7790_read_mode_pins(void);
void r8a7790_init_early(void);
extern struct smp_operations r8a7790_smp_ops;

#endif /* __ASM_R8A7790_H__ */
