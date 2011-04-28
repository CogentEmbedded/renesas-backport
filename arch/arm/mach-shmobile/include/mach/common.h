#ifndef __ARCH_MACH_COMMON_H
#define __ARCH_MACH_COMMON_H

extern struct sys_timer shmobile_timer;
extern void shmobile_setup_console(void);
struct clk;

extern int clk_init(void);
extern struct platform_suspend_ops shmobile_suspend_ops;
struct cpuidle_device;
extern void (*shmobile_cpuidle_modes[])(void);
extern void (*shmobile_cpuidle_setup)(struct cpuidle_device *dev);

extern void sh7367_init_irq(void);
extern void sh7367_add_early_devices(void);
extern void sh7367_add_standard_devices(void);
extern void sh7367_clock_init(void);
extern void sh7367_pinmux_init(void);

extern void sh7377_init_irq(void);
extern void sh7377_add_early_devices(void);
extern void sh7377_add_standard_devices(void);
extern void sh7377_pinmux_init(void);

extern void sh7372_init_irq(void);
extern void sh7372_add_early_devices(void);
extern void sh7372_add_standard_devices(void);
extern void sh7372_clock_init(void);
extern void sh7372_pinmux_init(void);
extern void sh7372_pm_init(void);
extern void sh7372_cpu_suspend(void);
extern void sh7372_cpu_resume(void);
extern struct clk sh7372_extal1_clk;
extern struct clk sh7372_extal2_clk;

#endif /* __ARCH_MACH_COMMON_H */
