#ifdef USE_SECFEATURE
#include <sec_feature/GlobalConfig.h>
#include <sec_feature/CustFeature.h>
#endif

/* PROJECTS */

#if defined(CONFIG_MACH_SAMSUNG_ESPRESSO)\
	|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_10)\
		|| defined(CONFIG_MACH_SAMSUNG_ESPRESSO_CHN_CMCC)
#define READ_MACADDR
#define HW_OOB
#endif

#ifdef CONFIG_MACH_U1
#define USE_CID_CHECK
#define U1_MACADDR
#define WRITE_MACADDR
#endif

#ifdef CONFIG_MACH_SAMSUNG_T1
#define USE_CID_CHECK
#define WRITE_MACADDR
#endif

/* REGION CODE */

#if (WLAN_REGION_CODE >= 100) && (WLAN_REGION_CODE < 200) /*EUR*/
#if (WLAN_REGION_CODE == 101) /*EUR ORG*/
/* GAN LITE NAT KEEPALIVE FILTER */
#define GAN_LITE_NAT_KEEPALIVE_FILTER
#endif
#endif

#if (WLAN_REGION_CODE >= 200) && (WLAN_REGION_CODE < 300) /* KOR */
#undef USE_INITIAL_2G_SCAN
#ifndef ROAM_ENABLE
#define ROAM_ENABLE
#endif
#ifndef ROAM_API
#define ROAM_API
#endif
#ifndef ROAM_CHANNEL_CACHE
#define ROAM_CHANNEL_CACHE
#endif
#ifndef OKC_SUPPORT
#define OKC_SUPPORT
#endif

/* for debug */
#ifdef RSSI_OFFSET
#undef RSSI_OFFSET
#define RSSI_OFFSET 8
#else
#define RSSI_OFFSET 8
#endif

#undef WRITE_MACADDR
#undef READ_MACADDR
#ifdef CONFIG_BCM4334
#define RDWR_KORICS_MACADDR
#else
#define RDWR_MACADDR
#endif

#if (WLAN_REGION_CODE == 201) /* SKT */
#endif

#if (WLAN_REGION_CODE == 202) /* KTT */
#define VLAN_MODE_OFF
#define KEEP_ALIVE_PACKET_PERIOD_30_SEC
#define FULL_ROAMING_SCAN_PERIOD_60_SEC
#endif

#if (WLAN_REGION_CODE == 203) /* LGT */
#endif
#endif

#if (WLAN_REGION_CODE >= 300) && (WLAN_REGION_CODE < 400) /* CHN */
#define BCMWAPI_WPI
#define BCMWAPI_WAI
#endif

