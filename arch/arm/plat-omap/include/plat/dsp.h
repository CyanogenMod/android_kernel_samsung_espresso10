#ifndef __OMAP_DSP_H__
#define __OMAP_DSP_H__

#include <linux/types.h>

struct omap_dsp_platform_data {
	void (*dsp_set_min_opp) (u8 opp_id);
	u8 (*dsp_get_opp) (void);
	void (*cpu_set_freq) (unsigned long f);
	unsigned long (*cpu_get_freq) (void);
	unsigned long mpu_speed[6];

	/* functions to write and read PRCM registers */
	void (*dsp_prm_write)(u32, s16 , u16);
	u32 (*dsp_prm_read)(s16 , u16);
	u32 (*dsp_prm_rmw_bits)(u32, u32, s16, s16);
	void (*dsp_cm_write)(u32, s16 , u16);
	u32 (*dsp_cm_read)(s16 , u16);
	u32 (*dsp_cm_rmw_bits)(u32, u32, s16, s16);

	phys_addr_t phys_mempool_base;
	phys_addr_t phys_mempool_size;
};

#if defined(CONFIG_TIDSPBRIDGE) || defined(CONFIG_TIDSPBRIDGE_MODULE)
extern void omap_dsp_reserve_sdram_memblock(void);
phys_addr_t omap_dsp_get_mempool_size(void);
phys_addr_t omap_dsp_get_mempool_base(void);
#elif !defined(CONFIG_OMAP_REMOTE_PROC_DSP)
static inline void omap_dsp_reserve_sdram_memblock(void) { }
static inline phys_addr_t omap_dsp_get_mempool_size(void) { return 0; }
static inline phys_addr_t omap_dsp_get_mempool_base(void) { return 0; }
#endif

#endif
