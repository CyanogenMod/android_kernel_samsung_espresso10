/*
 * ION Initialization for OMAP4.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ion.h>
#include <linux/memblock.h>
#include <linux/omap_ion.h>
#include <linux/platform_device.h>
#include <plat/common.h>

#include <mach/omap4_ion.h>

#include <linux/kconfig.h>

#ifdef CONFIG_ION_CMA
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#endif

#include "../mm/mm.h"		/* arm_lowmem_limit */

/*
 * Carveouts from higher end of RAM
 *   - SMC
 *   - ION 1D
 *   - Ducati heap
 *   - Tiler 2D secure
 *   - Tiler non-secure
 */

static bool system_512m;
static bool system_2gb;

static phys_addr_t omap4_smc_addr;
static phys_addr_t omap4_ion_heap_secure_input_addr;
static phys_addr_t omap4_ion_heap_secure_output_wfdhdcp_addr;
static phys_addr_t omap4_ducati_heap_addr;
static phys_addr_t omap4_ion_heap_tiler_mem_addr;
static phys_addr_t omap4_ion_heap_nonsec_tiler_mem_addr;

static size_t omap4_smc_size;
static size_t omap4_ion_heap_secure_input_size;
static size_t omap4_ion_heap_secure_output_wfdhdcp_size;
static size_t omap4_ducati_heap_size;
static size_t omap4_ion_heap_tiler_mem_size;
static size_t omap4_ion_heap_nonsec_tiler_mem_size;

static struct ion_platform_heap omap4_ion_heaps[] = {
	{
		.type = ION_HEAP_TYPE_CARVEOUT,
		.id = OMAP_ION_HEAP_SECURE_INPUT,
		.name = "secure_input",
	},
	{
		.type = ION_HEAP_TYPE_CARVEOUT,
		.id = OMAP_ION_HEAP_SECURE_OUTPUT_WFDHDCP,
		.name = "secure_output_wfdhdcp",
	},
	{
		.type = OMAP_ION_HEAP_TYPE_TILER,
#ifdef CONFIG_ION_CMA
		.id = OMAP_ION_HEAP_TILER_CMA,
#else
		.id = OMAP_ION_HEAP_TILER,
#endif
		.name = "tiler",
	},
	{
		.type = OMAP_ION_HEAP_TYPE_TILER,
		.id = OMAP_ION_HEAP_NONSECURE_TILER,
		.name = "nonsecure_tiler",
	},
	{
		.type = ION_HEAP_TYPE_SYSTEM,
		.id = OMAP_ION_HEAP_SYSTEM,
		.name = "system",
	},
	{
		.type = OMAP_ION_HEAP_TYPE_TILER_RESERVATION,
		.id = OMAP_ION_HEAP_TILER_RESERVATION,
		.name = "tiler_reservation",
	},
};

static struct ion_platform_data omap4_ion_data = {
	.nr = ARRAY_SIZE(omap4_ion_heaps),
	.heaps = omap4_ion_heaps,
};

static struct omap_ion_platform_data omap4_ion_pdata = {
	.ion = &omap4_ion_data,
};

#ifdef CONFIG_ION_CMA
static u64 omap4_dmamask = DMA_BIT_MASK(32);
#endif

static struct platform_device omap4_ion_device = {
	.name = "ion-omap4",
	.id = -1,
	.dev = {
#ifdef CONFIG_ION_CMA
		.dma_mask = &omap4_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
#endif
		.platform_data = &omap4_ion_data,
	},
};

struct omap_ion_platform_data *get_omap_ion_platform_data(void)
{
	return &omap4_ion_pdata;
}

void __init omap4_register_ion(void)
{
	platform_device_register(&omap4_ion_device);
}

phys_addr_t __init omap4_get_tiler2d_base(phys_addr_t prebase, size_t size)
{
	phys_addr_t tiler2d_base;

	if (IS_ENABLED(CONFIG_ION_CMA)) {
		tiler2d_base = omap4_ion_pdata.tiler2d_secure_base;
		WARN_ON((tiler2d_base + size) >= arm_lowmem_limit);
	} else
		tiler2d_base = prebase - size;

	return tiler2d_base;
}

void __init omap_ion_init(void)
{
	int i;
	int ret;

	system_512m = (omap_total_ram_size() == SZ_512M);
	system_2gb = (omap_total_ram_size() > SZ_1G);

	/* carveout sizes */
	omap4_smc_size = (SZ_1M * 3);

	if (system_512m) {
		omap4_ion_heap_secure_input_size = 0;
		omap4_ion_heap_secure_output_wfdhdcp_size = 0;
		omap4_ducati_heap_size = (SZ_1M * 83);
		omap4_ion_heap_nonsec_tiler_mem_size = 0;
		omap4_ion_heap_tiler_mem_size = 0;
	} else {
		omap4_ion_heap_secure_input_size = omap4_ion_pdata.tiler1d_size;
		omap4_ion_heap_secure_output_wfdhdcp_size =
				omap4_ion_pdata.secure_output_wfdhdcp_size;
		omap4_ducati_heap_size = omap4_ion_pdata.ducati_heap_size;
		omap4_ion_heap_nonsec_tiler_mem_size =
				omap4_ion_pdata.nonsecure_tiler2d_size;
		omap4_ion_heap_tiler_mem_size = omap4_ion_pdata.tiler2d_size;
	}

	/* carveout addresses */
	omap4_smc_addr = PLAT_PHYS_OFFSET + omap_total_ram_size() -
				omap4_smc_size;
	if (system_2gb)
		omap4_smc_addr = PLAT_PHYS_OFFSET + SZ_1G - omap4_smc_size;
	omap4_ion_heap_secure_input_addr = omap4_smc_addr -
				omap4_ion_heap_secure_input_size;
	omap4_ion_heap_secure_output_wfdhdcp_addr =
				omap4_ion_heap_secure_input_addr -
				omap4_ion_heap_secure_output_wfdhdcp_size;
	omap4_ducati_heap_addr = omap4_ion_heap_secure_output_wfdhdcp_addr -
				omap4_ducati_heap_size;
	omap4_ion_heap_tiler_mem_addr = omap4_get_tiler2d_base(
					omap4_ducati_heap_addr,
					omap4_ion_heap_tiler_mem_size);
	omap4_ion_heap_nonsec_tiler_mem_addr = omap4_ion_heap_tiler_mem_addr -
				omap4_ion_heap_nonsec_tiler_mem_size;

	pr_info("omap4_total_ram_size = 0x%x\n" \
				"omap4_smc_size = 0x%x\n"  \
				"omap4_ion_heap_secure_input_size = 0x%x\n"  \
				"omap4_ion_heap_secure_output_wfdhdcp_size = 0x%x\n"  \
				"omap4_ducati_heap_size = 0x%x\n"  \
				"omap4_ion_heap_tiler_mem_size = 0x%x\n"  \
				"omap4_ion_heap_nonsec_tiler_mem_size  = 0x%x\n",
				omap_total_ram_size(),
				omap4_smc_size,
				omap4_ion_heap_secure_input_size,
				omap4_ion_heap_secure_output_wfdhdcp_size,
				omap4_ducati_heap_size,
				omap4_ion_heap_tiler_mem_size,
				omap4_ion_heap_nonsec_tiler_mem_size);

	pr_info(" omap4_smc_addr = 0x%x\n"  \
				"omap4_ion_heap_secure_input_addr = 0x%x\n"  \
				"omap4_ion_heap_secure_output_wfdhdcp_addr = 0x%x\n"  \
				"omap4_ducati_heap_addr = 0x%x\n"  \
				"omap4_ion_heap_tiler_mem_addr = 0x%x\n"  \
				"omap4_ion_heap_nonsec_tiler_mem_addr  = 0x%x\n",
				omap4_smc_addr,
				omap4_ion_heap_secure_input_addr,
				omap4_ion_heap_secure_output_wfdhdcp_addr,
				omap4_ducati_heap_addr,
				omap4_ion_heap_tiler_mem_addr,
				omap4_ion_heap_nonsec_tiler_mem_addr);

	for (i = 0; i < omap4_ion_data.nr; i++) {
		struct ion_platform_heap *h = &omap4_ion_data.heaps[i];

		switch (h->id) {
		case OMAP_ION_HEAP_SECURE_INPUT:
			h->base = omap4_ion_heap_secure_input_addr;
			h->size = omap4_ion_heap_secure_input_size;
			break;
		case OMAP_ION_HEAP_SECURE_OUTPUT_WFDHDCP:
			h->base = omap4_ion_heap_secure_output_wfdhdcp_addr;
			h->size = omap4_ion_heap_secure_output_wfdhdcp_size;
			break;
		case OMAP_ION_HEAP_NONSECURE_TILER:
			h->base = omap4_ion_heap_nonsec_tiler_mem_addr;
			h->size = omap4_ion_heap_nonsec_tiler_mem_size;
			break;
#ifdef CONFIG_ION_CMA
		case OMAP_ION_HEAP_TILER_CMA:
#else
		case OMAP_ION_HEAP_TILER:
#endif
			h->base = omap4_ion_heap_tiler_mem_addr;
			h->size = omap4_ion_heap_tiler_mem_size;
			break;
		default:
			break;
		}
		pr_info("%s: %s id=%u [%lx-%lx] size=%x\n",
					__func__, h->name, h->id,
					h->base, h->base + h->size, h->size);
	}

	for (i = 0; i < omap4_ion_data.nr; i++) {
		if (omap4_ion_data.heaps[i].type == ION_HEAP_TYPE_CARVEOUT ||
		    omap4_ion_data.heaps[i].type == OMAP_ION_HEAP_TYPE_TILER) {
			if (!omap4_ion_data.heaps[i].size)
				continue;
#ifdef CONFIG_ION_CMA
			if (omap4_ion_data.heaps[i].id ==
					OMAP_ION_HEAP_TILER_CMA) {
				ret = dma_declare_contiguous(
					&omap4_ion_device.dev,
					omap4_ion_data.heaps[i].size,
					omap4_ion_data.heaps[i].base, 0);
				if (unlikely(ret))
					pr_err("dma declare %x@%lx failed\n",
						omap4_ion_data.heaps[i].size,
						omap4_ion_data.heaps[i].base);
				continue;
			}
#endif

			ret = memblock_remove(omap4_ion_data.heaps[i].base,
					      omap4_ion_data.heaps[i].size);
			if (unlikely(ret))
				pr_err("memblock remove of %x@%lx failed\n",
				       omap4_ion_data.heaps[i].size,
				       omap4_ion_data.heaps[i].base);
		}
	}
}

phys_addr_t omap_smc_addr(void)
{
	return omap4_smc_addr;
}

phys_addr_t omap_ion_heap_secure_input_addr(void)
{
	return omap4_ion_heap_secure_input_addr;
}

phys_addr_t omap_ion_heap_secure_output_wfdhdcp_addr(void)
{
	return omap4_ion_heap_secure_output_wfdhdcp_addr;
}

phys_addr_t omap_ducati_heap_addr(void)
{
	return omap4_ducati_heap_addr;
}

phys_addr_t omap_ion_heap_tiler_mem_addr(void)
{
	return omap4_ion_heap_tiler_mem_addr;
}

phys_addr_t omap_ion_heap_nonsec_tiler_mem_addr(void)
{
	return omap4_ion_heap_nonsec_tiler_mem_addr;
}

size_t omap_smc_size(void)
{
	return omap4_smc_size;
}

size_t omap_ion_heap_secure_input_size(void)
{
	return omap4_ion_heap_secure_input_size;
}

size_t omap_ion_heap_secure_output_wfdhdcp_size(void)
{
	return omap4_ion_heap_secure_output_wfdhdcp_size;
}

size_t omap_ducati_heap_size(void)
{
	return omap4_ducati_heap_size;
}

size_t omap_ion_heap_tiler_mem_size(void)
{
	return omap4_ion_heap_tiler_mem_size;
}

size_t omap_ion_heap_nonsec_tiler_mem_size(void)
{
	return omap4_ion_heap_nonsec_tiler_mem_size;
}
