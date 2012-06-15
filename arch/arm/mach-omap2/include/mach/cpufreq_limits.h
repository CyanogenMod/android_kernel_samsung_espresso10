

#ifndef OMAP_ARCH_PM_COMMON_H
#define OMAP_ARCH_PM_COMMON_H

enum cpufreq_lock_ID {
	DVFS_LOCK_ID_G2D,	/* G2D */
	DVFS_LOCK_ID_TV,	/* TV */
	DVFS_LOCK_ID_MFC,	/* MFC */
	DVFS_LOCK_ID_USB,	/* USB */
	DVFS_LOCK_ID_USB_IF,	/* USB_IF */
	DVFS_LOCK_ID_CAM,	/* CAM */
	DVFS_LOCK_ID_PM,	/* PM */
	DVFS_LOCK_ID_USER,	/* USER */
	DVFS_LOCK_ID_TMU,	/* TMU */
	DVFS_LOCK_ID_LPA,	/* LPA */
	DVFS_LOCK_ID_TSP,	/* TSP */
	DVFS_LOCK_ID_PEN,	/* E-PEN */
	DVFS_LOCK_ID_G3D,	/* G3D */
	DVFS_LOCK_ID_IR_LED,/* IR_LED */
	DVFS_LOCK_ID_LCD,	/* LCD */
	DVFS_LOCK_ID_VIB,	/* VIBRATOR */
	DVFS_LOCK_ID_END,
};

extern int omap_cpufreq_max_limit(unsigned int nId, unsigned long req_freq);
extern int omap_cpufreq_min_limit(unsigned int nId, unsigned long req_freq);
extern void omap_cpufreq_min_limit_free(unsigned int nId);
extern void omap_cpufreq_max_limit_free(unsigned int nId);

#define CONFIG_DVFS_LIMIT 1
#endif

