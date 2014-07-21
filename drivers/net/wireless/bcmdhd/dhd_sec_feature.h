/*
 * Header file describing the internal (inter-module) DHD interfaces.
 *
 * Provides type definitions and function prototypes used to link the
 * DHD OS, bus, and protocol modules.
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_sec_feature.h 309548 2012-01-20 01:13:08Z $
 */

#ifndef _dhd_sec_feature_h_
#define _dhd_sec_feature_h_

/* PROJECTS */

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO)\
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10)\
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
#undef USE_CID_CHECK
#define READ_MACADDR
#define HW_OOB
#endif

/* Module */

#if defined(CONFIG_MACH_U1) || defined(CONFIG_MACH_TRATS)\
	|| defined(CONFIG_MACH_SAMSUNG_PALAU)\
	|| defined(CONFIG_MACH_SAMSUNG_SUPERIOR)\
	|| defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_OPEN)\
	|| defined(CONFIG_MACH_SAMSUNG_SUPERIOR_CHN_CMCC)
#ifdef CONFIG_MACH_Q1_BD
#define HW_OOB
#endif
#define USE_CID_CHECK
#define WRITE_MACADDR
#endif

#ifdef CONFIG_MACH_SAMSUNG_T1
#define HW_OOB
#define USE_CID_CHECK
#define WRITE_MACADDR
#endif

#ifdef CONFIG_MACH_GC1
#undef USE_CID_CHECK
#define READ_MACADDR
#endif

#ifdef CONFIG_MACH_SAMSUNG_KONA
#undef USE_CID_CHECK
#define READ_MACADDR
#endif

#ifdef CONFIG_MACH_SAMSUNG_GOKEY
#undef USE_CID_CHECK
#define READ_MACADDR
#endif

/* REGION CODE */
#ifndef CONFIG_WLAN_REGION_CODE
#define CONFIG_WLAN_REGION_CODE 100
#endif /* CONFIG_WLAN_REGION_CODE */

/* REGION CODE */

//#if (WLAN_REGION_CODE >= 100) && (WLAN_REGION_CODE < 200) /*EUR*/
//#if (WLAN_REGION_CODE == 101) /*EUR ORG*/
/* GAN LITE NAT KEEPALIVE FILTER */
//#define GAN_LITE_NAT_KEEPALIVE_FILTER
//#endif /* WLAN_REGION_CODE == 101 */
//#endif /* WLAN_REGION_CODE >= 100 && WLAN_REGION_CODE < 200 */

#if (WLAN_REGION_CODE >= 200) && (WLAN_REGION_CODE < 300) /* KOR */
#undef USE_INITIAL_2G_SCAN_ORG
#ifndef ROAM_ENABLE
#define ROAM_ENABLE
#endif /* ROAM_ENABLE */
#ifndef ROAM_API
#define ROAM_API
#endif /* ROAM_API */
#ifndef ROAM_CHANNEL_CACHE
#define ROAM_CHANNEL_CACHE
#endif /* ROAM_CHANNEL_CACHE */
#ifndef OKC_SUPPORT
#define OKC_SUPPORT
#endif /* OKC_SUPPORT */

#ifndef ROAM_AP_ENV_DETECTION
#define ROAM_AP_ENV_DETECTION
#endif /* ROAM_AP_ENV_DETECTION */

#undef WRITE_MACADDR
#undef READ_MACADDR
#ifdef CONFIG_BCM4334
#define READ_MACADDR
#else
#define RDWR_MACADDR
#endif /* CONFIG_BCM4334 */

#if (WLAN_REGION_CODE == 201) /* SKT */
#endif /* WLAN_REGION_CODE == 201 */

#if (WLAN_REGION_CODE == 202) /* KTT */
#define VLAN_MODE_OFF
#define CUSTOM_KEEP_ALIVE_SETTING	30000
#define FULL_ROAMING_SCAN_PERIOD_60_SEC
#endif /* WLAN_REGION_CODE == 202 */

#if (WLAN_REGION_CODE == 203) /* LGT */
#endif /* WLAN_REGION_CODE == 203 */
#endif /* WLAN_REGION_CODE >= 200 && WLAN_REGION_CODE < 300 */

#if (WLAN_REGION_CODE >= 300) && (WLAN_REGION_CODE < 400) /* CHN */
#define BCMWAPI_WPI
#define BCMWAPI_WAI
#endif /* WLAN_REGION_CODE >= 300 && WLAN_REGION_CODE < 400 */

#if !defined(READ_MACADDR) && !defined(WRITE_MACADDR)\
	&& !defined(RDWR_KORICS_MACADDR) && !defined(RDWR_MACADDR)
#define GET_MAC_FROM_OTP
#endif /* !READ_MACADDR && !WRITE_MACADDR && !RDWR_KORICS_MACADDR && !RDWR_MACADDR */

#endif /* _dhd_sec_feature_h_ */
