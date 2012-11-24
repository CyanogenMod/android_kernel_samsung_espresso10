/*
 * Core Source for:
 * Cypress TrueTouch(TM) Standard Product (TTSP) touchscreen drivers.
 * For use with Cypress Gen4 and Solo parts.
 * Supported parts include:
 * CY8CTMA884/616
 * CY8CTMA4XX
 *
 * Copyright (C) 2009-2012 Cypress Semiconductor, Inc.
 * Copyright (C) 2011 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <kev@cypress.com>
 *
 */

#define DEBUG_PRINT			1

#if DEBUG_PRINT
#define	tsp_log(fmt, args...) \
				pr_info("tsp: %s: " fmt, __func__, ## args)
#else
#define tsp_log(fmt, args...)
#endif

#include "cyttsp4_core.h"

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/platform_data/cypress_cyttsp4.h>
#include <linux/platform_data/sec_ts.h>
#include <linux/firmware.h>	/* This enables firmware class loader code */

#ifdef CY_AUTO_LOAD_TOUCH_PARAMS
#include "cyttsp4_params.h"
#endif

#ifdef CY_AUTO_LOAD_FW
#include "cyttsp4_img.h"
#endif

#ifdef CONFIG_TOUCH_DVFS
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <mach/cpufreq_limits.h>
#ifdef CONFIG_DVFS_LIMIT
#include <linux/cpufreq.h>
#endif
#define DVFS_TIME			1000
bool boost;
struct timer_list dvfs_timer;
#endif

/* platform address lookup offsets */
#define CY_TCH_ADDR_OFS		0
#define CY_LDR_ADDR_OFS		1

/* helpers */
#define GET_NUM_TOUCHES(x)          ((x) & 0x1F)
#define IS_LARGE_AREA(x)            ((x) & 0x20)
#define IS_BAD_PKT(x)               ((x) & 0x20)
#define GET_HSTMODE(reg)            ((reg & 0x70) >> 4)
#define IS_BOOTLOADERMODE(reg)      (reg & 0x01)

/* maximum number of concurrent tracks */
#define CY_NUM_TCH_ID               10
/* maximum number of track IDs */
#define CY_NUM_TRK_ID               16
/* maximum number of command data bytes */
#define CY_NUM_DAT                  6
/* maximum number of config block read data */
#define CY_NUM_CONFIG_BYTES        128

#define CY_REG_BASE                 0x00
#define CY_DELAY_DFLT               20		/* ms */
#define CY_DELAY_MAX                (500/CY_DELAY_DFLT)	/* half second */
#define CY_HALF_SEC_TMO_MS          500		/* half second in msecs */
#define CY_TEN_SEC_TMO_MS           10000	/* ten seconds in msecs */
#define CY_HANDSHAKE_BIT            0x80
#define CY_WAKE_DFLT                99	/* causes wake strobe on INT line
					 * in sample board configuration
					 * platform data->hw_recov() function
					 */
/* power mode select bits */
#define CY_SOFT_RESET_MODE          0x01
#define CY_DEEP_SLEEP_MODE          0x02
#define CY_LOW_POWER_MODE           0x04
/* device mode bits */
#define CY_MODE_CHANGE              0x08 /* rd/wr hst_mode */
#define CY_OPERATE_MODE             0x00 /* rd/wr hst_mode */
#define CY_SYSINFO_MODE             0x10 /* rd/wr hst_mode */
#define CY_CONFIG_MODE              0x20 /* rd/wr hst_mode */
#define CY_BL_MODE                  0x01 /* wr hst mode == soft reset
					  * was 0x10 to rep_stat for LTS
					  */
#define CY_CMD_RDY_BIT              0x40

#define CY_REG_OP_START             0
#define CY_REG_SI_START             0
#define CY_REG_OP_END               0x20
#define CY_REG_SI_END               0x20

#define CY_TCH_CRC_LOC_TMA400       5884 /* location of CRC in touch EBID */

/* register field lengths */
#define CY_NUM_REVCTRL              8
#define CY_NUM_MFGID                8
#define CY_NUM_TCHREC               10
#define CY_NUM_DDATA                32
#define CY_NUM_MDATA                64
#define CY_TMA400_MAX_BYTES         512 /*
					  * max reg access for TMA400
					  * in config mode
					  */

/* touch event id codes */
#define CY_GET_EVENTID(reg)         ((reg & 0x60) >> 5)
#define CY_GET_TRACKID(reg)         (reg & 0x1F)
#define CY_NOMOVE                   0
#define CY_TOUCHDOWN                1
#define CY_MOVE                     2
#define CY_LIFTOFF                  3

#define CY_CFG_BLK_SIZE             126
#define CY_EBID_ROW_SIZE_DFLT       128

#define CY_BL_VERS_SIZE             12
#define CY_NUM_TMA400_TT_CFG_BLK    51 /* Rev84 mapping */

#if defined(CY_USE_FORCE_LOAD) || defined(CONFIG_TOUCHSCREEN_DEBUG)
#define CY_BL_FW_NAME_SIZE          NAME_MAX
#endif

#ifdef CY_USE_REG_ACCESS
#define CY_RW_REGID_MAX             0xFFFF
#define CY_RW_REG_DATA_MAX          0xFF
#endif

/* abs settings */
#define CY_IGNORE_VALUE             0xFFFF
/* abs signal capabilities offsets in the frameworks array */
enum cyttsp4_sig_caps {
	CY_SIGNAL_OST   = 0,
	CY_MIN_OST      = 1,
	CY_MAX_OST      = 2,
	CY_FUZZ_OST     = 3,
	CY_FLAT_OST     = 4,
	CY_NUM_ABS_SET	/* number of signal capability fields */
};

/* abs axis signal offsets in the framworks array  */
enum cyttsp4_sig_ost {
	CY_ABS_X_OST    = 0,
	CY_ABS_Y_OST    = 1,
	CY_ABS_P_OST    = 2,
	CY_ABS_W_OST    = 3,
	CY_ABS_ID_OST   = 4,
	CY_ABS_MAJ_OST	= 5,
	CY_ABS_MIN_OST	= 6,
	CY_ABS_OR_OST	= 7,
	CY_NUM_ABS_OST	/* number of abs signals */
};

/* touch record system information offset masks and shifts */
#define CY_BYTE_OFS_MASK            0x1F
#define CY_BOFS_MASK                0xE0
#define CY_BOFS_SHIFT               5

enum cyttsp4_driver_state {
	CY_IDLE_STATE,		/* IC cannot be reached */
	CY_READY_STATE,		/* pre-operational; ready to go to ACTIVE */
	CY_ACTIVE_STATE,	/* app is running, IC is scanning */
	CY_SLEEP_STATE,		/* app is running, IC is idle */
	CY_BL_STATE,		/* bootloader is running */
	CY_SYSINFO_STATE,	/* switching to sysinfo mode */
	CY_CMD_STATE,		/* command initiation mode */
	CY_EXIT_BL_STATE,	/* sync bl heartbeat to app ready int */
	CY_TRANSFER_STATE,	/* changing states */
	CY_WAKE_STATE,		/* absorb irq from driver pulse on int line */
	CY_INVALID_STATE	/* always last in the list */
};

static const char * const cyttsp4_driver_state_string[] = {
	/* Order must match enum cyttsp4_driver_state above */
	"IDLE",
	"READY",
	"ACTIVE",
	"SLEEP",
	"BOOTLOADER",
	"SYSINFO",
	"CMD",
	"EXIT_BL",
	"TRANSFER",
	"WAKE",
	"INVALID"
};

enum cyttsp4_controller_mode {
	CY_MODE_BOOTLOADER,
	CY_MODE_SYSINFO,
	CY_MODE_OPERATIONAL,
	CY_MODE_CONFIG,
	CY_MODE_NUM
};

enum cyttsp4_ic_grpnum {
	CY_IC_GRPNUM_RESERVED = 0,
	CY_IC_GRPNUM_CMD_REGS,
	CY_IC_GRPNUM_TCH_REP,
	CY_IC_GRPNUM_DATA_REC,
	CY_IC_GRPNUM_TEST_REC,
	CY_IC_GRPNUM_PCFG_REC,
	CY_IC_GRPNUM_TCH_PARM_VAL,
	CY_IC_GRPNUM_TCH_PARM_SIZ,
	CY_IC_GRPNUM_RESERVED1,
	CY_IC_GRPNUM_RESERVED2,
	CY_IC_GRPNUM_OPCFG_REC,
	CY_IC_GRPNUM_DDATA_REC,
	CY_IC_GRPNUM_MDATA_REC,
	CY_IC_GRPNUM_TEST_REGS,
	CY_IC_GRPNUM_BTN_KEYS,
	CY_IC_GRPNUM_NUM
};

enum cyttsp4_ic_op_mode_commands {
	CY_GET_PARAM_CMD = 0x02,
	CY_SET_PARAM_CMD = 0x03,
	CY_GET_CFG_BLK_CRC = 0x05,
};

enum cyttsp4_ic_config_mode_commands {
	CY_GET_EBID_ROW_SIZE = 0x02,
	CY_READ_EBID_DATA = 0x03,
	CY_WRITE_EBID_DATA = 0x04,
	CY_VERIFY_EBID_CRC = 0x11,
};

enum cyttsp4_ic_ebid {
	CY_TCH_PARM_EBID = 0x00,
	CY_MDATA_EBID = 0x01,
	CY_DDATA_EBID = 0x02,
};

enum cyttsp4_flags {
	CY_FLAG_NONE = 0x00,
	CY_FLAG_HOVER = 0x04,
};

enum cyttsp4_event_id {
	CY_EV_NO_EVENT = 0,
	CY_EV_TOUCHDOWN = 1,
	CY_EV_MOVE = 2,		/* significant displacement (> act dist) */
	CY_EV_LIFTOFF = 3,	/* record reports last position */
};

enum cyttsp4_object_id {
	CY_OBJ_STANDARD_FINGER = 0,
	CY_OBJ_LARGE_OBJECT = 1,
	CY_OBJ_STYLUS = 2,
	CY_OBJ_HOVER = 3,
};

enum cyttsp4_test_cmd {
	CY_TEST_CMD_NULL = 0,
};

/* test mode NULL command driver codes; D */
enum cyttsp4_null_test_cmd_code {
	CY_NULL_CMD_NULL = 0,
	CY_NULL_CMD_MODE,
	CY_NULL_CMD_STATUS_SIZE,
	CY_NULL_CMD_HANDSHAKE,
};

enum cyttsp_test_mode {
	CY_TEST_MODE_NORMAL_OP = 0,	/* Send touch data to OS; normal op */
	CY_TEST_MODE_CAT,		/* Configuration and Test */
	CY_TEST_MODE_CLOSED_UNIT,	/* Send scan data to sysfs */
};

struct cyttsp4_test_mode {
	int cur_mode;
	int cur_cmd;
	size_t cur_status_size;
};

/* GEN4/SOLO Operational interface definitions */
enum cyttsp4_tch_abs {	/* for ordering within the extracted touch data array */
	CY_TCH_X = 0,	/* X */
	CY_TCH_Y,	/* Y */
	CY_TCH_P,	/* P (Z) */
	CY_TCH_T,	/* TOUCH ID */
	CY_TCH_E,	/* EVENT ID */
	CY_TCH_O,	/* OBJECT ID */
	CY_TCH_W,	/* SIZE (SOLO - Corresponds to TOUCH_MAJOR) */
	CY_TCH_MAJ,	/* TOUCH_MAJOR */
	CY_TCH_MIN,	/* TOUCH_MINOR */
	CY_TCH_OR,	/* ORIENTATION */
	CY_TCH_NUM_ABS
};
static const char * const cyttsp4_tch_abs_string[] = {
	/* Order must match enum cyttsp4_tch_descriptor above */
	"X",
	"Y",
	"P",
	"T",
	"E",
	"O",
	"W",
	"MAJ",
	"MIN",
	"OR",
	"INVALID"
};

#define CY_NUM_NEW_TCH_FIELDS	3

#define CY_NUM_OLD_TCH_FIELDS	(CY_TCH_NUM_ABS - CY_NUM_NEW_TCH_FIELDS)

struct cyttsp4_touch {
	int abs[CY_TCH_NUM_ABS];
};

/* TTSP System Information interface definitions */
struct cyttsp4_cydata {
	u8 ttpidh;
	u8 ttpidl;
	u8 fw_ver_major;
	u8 fw_ver_minor;
	u8 revctrl[CY_NUM_REVCTRL];
	u8 blver_major;
	u8 blver_minor;
	u8 jtag_si_id3;
	u8 jtag_si_id2;
	u8 jtag_si_id1;
	u8 jtag_si_id0;
	u8 mfgid_sz;
	u8 mfg_id[CY_NUM_MFGID];
	u8 cyito_idh;
	u8 cyito_idl;
	u8 cyito_verh;
	u8 cyito_verl;
	u8 ttsp_ver_major;
	u8 ttsp_ver_minor;
	u8 device_info;
} __packed;

struct cyttsp4_test {
	u8 post_codeh;
	u8 post_codel;
} __packed;

struct cyttsp4_pcfg {
	u8 electrodes_x;
	u8 electrodes_y;
	u8 len_xh;
	u8 len_xl;
	u8 len_yh;
	u8 len_yl;
	u8 axis_xh;
	u8 axis_xl;
	u8 axis_yh;
	u8 axis_yl;
	u8 max_zh;
	u8 max_zl;
} __packed;

struct cyttsp4_tch_rec_params {
	u8 loc;
	u8 size;
} __packed;

struct cyttsp4_opcfg {
	u8 cmd_ofs;
	u8 rep_ofs;
	u8 rep_szh;
	u8 rep_szl;
	u8 num_btns;
	u8 tt_stat_ofs;
	u8 obj_cfg0;
	u8 max_tchs;
	u8 tch_rec_siz;
	struct cyttsp4_tch_rec_params tch_rec_old[CY_NUM_OLD_TCH_FIELDS];
	u8 btn_rec_siz;	/* btn record size (in bytes) */
	u8 btn_diff_ofs;/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	u8 btn_diff_siz;/* btn size of diff counts (in bits) */
	struct cyttsp4_tch_rec_params tch_rec_new[CY_NUM_NEW_TCH_FIELDS];
} __packed;

struct cyttsp4_sysinfo_data {
	u8 hst_mode;
	u8 reserved;
	u8 map_szh;
	u8 map_szl;
	u8 cydata_ofsh;
	u8 cydata_ofsl;
	u8 test_ofsh;
	u8 test_ofsl;
	u8 pcfg_ofsh;
	u8 pcfg_ofsl;
	u8 opcfg_ofsh;
	u8 opcfg_ofsl;
	u8 ddata_ofsh;
	u8 ddata_ofsl;
	u8 mdata_ofsh;
	u8 mdata_ofsl;
} __packed;

struct cyttsp4_sysinfo_ptr {
	struct cyttsp4_cydata *cydata;
	struct cyttsp4_test *test;
	struct cyttsp4_pcfg *pcfg;
	struct cyttsp4_opcfg *opcfg;
	struct cyttsp4_ddata *ddata;
	struct cyttsp4_mdata *mdata;
} __packed;

struct cyttsp4_tch_abs_params {
	size_t ofs;	/* abs byte offset */
	size_t size;	/* size in bits */
	size_t max;	/* max value */
	size_t bofs;	/* bit offset */
};

struct cyttsp4_sysinfo_ofs {
	size_t cmd_ofs;
	size_t rep_ofs;
	size_t rep_sz;
	size_t num_btns;
	size_t num_btn_regs;	/* ceil(num_btns/4) */
	size_t tt_stat_ofs;
	size_t tch_rec_siz;
	size_t obj_cfg0;
	size_t max_tchs;
	size_t mode_size;
	size_t data_size;
	size_t map_sz;
	size_t cydata_ofs;
	size_t test_ofs;
	size_t pcfg_ofs;
	size_t opcfg_ofs;
	size_t ddata_ofs;
	size_t mdata_ofs;
	size_t cydata_size;
	size_t test_size;
	size_t pcfg_size;
	size_t opcfg_size;
	size_t ddata_size;
	size_t mdata_size;
	size_t btn_keys_size;
	struct cyttsp4_tch_abs_params tch_abs[CY_TCH_NUM_ABS];
	size_t btn_rec_siz;	/* btn record size (in bytes) */
	size_t btn_diff_ofs;/* btn data loc ,diff counts, (Op-Mode byte ofs) */
	size_t btn_diff_siz;/* btn size of diff counts (in bits) */
};

/* button to keycode support */
#define CY_NUM_BTN_PER_REG	4
#define CY_NUM_BTN_EVENT_ID	4
#define CY_BITS_PER_BTN		2

enum cyttsp4_btn_state {
	CY_BTN_RELEASED = 0,
	CY_BTN_PRESSED = 1,
	CY_BTN_NUM_STATE
};

struct cyttsp4_btn {
	bool enabled;
	int state;	/* CY_BTN_PRESSED, CY_BTN_RELEASED */
	int key_code;
};

#if defined(CY_AUTO_LOAD_TOUCH_PARAMS)
static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *) &cyttsp4_param_regs[0],
	.size = ARRAY_SIZE(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *) &cyttsp4_param_size[0],
	.size = ARRAY_SIZE(cyttsp4_param_size),
	.tag = 0,
};
#endif

#ifdef CY_AUTO_LOAD_FW
static struct touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#endif

#define FW_VERSION	0x0100

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
#define TSP_VENDOR			"CYPRESS"
#define TSP_IC				"GEN4"

#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		512
#define TSP_CMD_PARAM_NUM		8

struct factory_data {
	struct list_head	cmd_list_head;
	u8			cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	char			cmd_buff[TSP_CMD_RESULT_STR_LEN];
	struct mutex		cmd_lock;
	bool			cmd_is_running;
};

struct node_data {
	s16			*cm_delta_data;
	s16			*cm_abs_data;
	s16			*intensity_data;
	s16			*reference_data;
};

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func
#define TOSTRING(x) #x

enum {	/* this is using by cmd_state valiable. */
	WAITING = 0,
	RUNNING,
	OK,
	FAIL,
	NOT_APPLICABLE,
};

struct tsp_cmd {
	struct list_head	list;
	const char		*cmd_name;
	void			(*cmd_func)(void *device_data);
};

#endif

/* driver context structure definitions */

#define CY_I2C_DATA_SIZE  (3 * 256)

#define MAX_TOUCH			10
#define MAX_PRESSURE			255
#define MAX_TOUCH_MAJOR			255
#define MAX_TOUCH_MINOR			255
#define MIN_ORIENTATION			-128
#define MAX_ORIENTATION			127

struct ts_data {
	bool			finger_state[MAX_TOUCH];
	int irq;
	struct i2c_client	*client;
	struct input_dev *input_dev;
	struct mutex data_lock;		/* prevent concurrent accesses */
	struct workqueue_struct		*cyttsp4_wq;
	struct work_struct		cyttsp4_resume_startup_work;
	char phys[32];
	struct sec_ts_platform_data *platform_data;
	struct touch_platform_data *private_data;
#ifdef CY_AUTO_LOAD_FW
	struct touch_firmware *fw;
#endif
#ifdef CY_AUTO_LOAD_TOUCH_PARAMS
	struct touch_settings *params_regs;
	struct touch_settings *params_size;
#endif
	u8 *xy_mode;			/* operational mode and status regs */
	u8 *xy_data;			/* operational touch regs */
	u8 *xy_data_touch1;		/* includes 1-byte for tt_stat */
	u8 *btn_rec_data;		/* button diff count data */
	struct cyttsp4_sysinfo_data sysinfo_data;
	struct cyttsp4_sysinfo_ptr sysinfo_ptr;
	struct cyttsp4_sysinfo_ofs *si_ofs;
	struct cyttsp4_btn *btn;
	struct cyttsp4_test_mode test;
	struct completion int_running;
	struct completion si_int_running;
	struct completion ready_int_running;
	enum cyttsp4_driver_state driver_state;
	enum cyttsp4_controller_mode current_mode;
	bool irq_enabled;
	bool powered; /* protect against multiple open */
	bool was_suspended;
	bool switch_flag;
	bool soft_reset_asserted;
	u16 flags;
	size_t max_config_bytes;
	size_t ebid_row_size;
	int num_prv_tch;
	bool starting_up;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
#ifdef CY_USE_WATCHDOG
	struct work_struct work;
	struct timer_list timer;
#endif
#if defined(CY_USE_FORCE_LOAD) || defined(CONFIG_TOUCHSCREEN_DEBUG)
	bool waiting_for_fw;
	char *fwname;
#endif
#ifdef CY_USE_REG_ACCESS
	size_t rw_regid;
#endif
	u8 wr_buf[CY_I2C_DATA_SIZE];
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	struct factory_data *factory_data;
	struct node_data	*node_data;
#endif

};

#if defined(CY_AUTO_LOAD_FW) || \
	defined(CY_USE_FORCE_LOAD) || \
	defined(CONFIG_TOUCHSCREEN_DEBUG)
static int _cyttsp4_load_app(struct ts_data *ts, const u8 *fw, int fw_size);
#endif /* CY_AUTO_LOAD_FW || CY_USE_FORCE_LOAD || CONFIG_TOUCHSCREEN_DEBUG */
static int _cyttsp4_ldr_exit(struct ts_data *ts);
static int _cyttsp4_startup(struct ts_data *ts);
static int _cyttsp4_get_ic_crc(struct ts_data *ts,
	enum cyttsp4_ic_ebid ebid, u8 *crc_h, u8 *crc_l);
static int _cyttsp4_set_mode(struct ts_data *ts, u8 new_mode);

static void _cyttsp4_pr_state(struct ts_data *ts)
{
	pr_info("%s: %s\n", __func__,
		ts->driver_state < CY_INVALID_STATE ?
		cyttsp4_driver_state_string[ts->driver_state] :
		"INVALID");
}

static void _cyttsp4_pr_buf(struct ts_data *ts, u8 *dptr, int size,
	const char *data_name)
{
	return;
}

static int _cyttsp4_read_block_data(struct ts_data *ts, u16 command,
	size_t length, void *buf, int i2c_addr, bool use_subaddr)
{
	int retval = 0;
	u8 sub_addr[2];
	int subaddr_len;

	if ((buf == NULL) || (length == 0)) {
		pr_err("%s: pointer or length error"
			" buf=%p length=%d\n", __func__, buf, length);
		retval = -EINVAL;
	} else {
		if (use_subaddr) {
			subaddr_len = 1;
			sub_addr[0] = command;
		}

		ts->client->addr = i2c_addr;
		if (use_subaddr) {
			/* write subaddr */
			retval = i2c_master_send(
					ts->client, sub_addr, subaddr_len);
			if (retval != subaddr_len)
				retval = -EIO;
		}

		if (retval < 0)
			return retval;
		retval = i2c_master_recv(ts->client, buf, length);
		if (retval != length)
			retval = -EIO;

		if (retval < 0) {
			msleep(CY_DELAY_DFLT);
			/*
			 * TODO: remove the extra sleep delay when
			 * the loader exit sequence is streamlined
			  */
			msleep(150);
			pr_err("%s: bus read block data fail (ret=%d)\n",
				__func__, retval);
		}
	}

	return retval;
}

static int _cyttsp4_write_block_data(struct ts_data *ts, u16 command,
	size_t length, const void *buf, int i2c_addr, bool use_subaddr)
{
	int retval = 0;

	if ((buf == NULL) || (length == 0)) {
		pr_err("%s: pointer or length error"
			" buf=%p length=%d\n", __func__, buf, length);
		retval = -EINVAL;
	} else {
		if (use_subaddr) {
			ts->wr_buf[0] = command;
			memcpy(&ts->wr_buf[1], buf, length);
			length += 1;
		} else {
			memcpy(&ts->wr_buf[0], buf, length);
		}
		ts->client->addr = i2c_addr;
		retval = i2c_master_send(ts->client, ts->wr_buf, length);
		if (retval < 0) {
			msleep(CY_DELAY_DFLT);
			pr_err("%s: bus write block data fail (ret=%d)\n",
				__func__, retval);
		}
	}

	return retval;
}

static int _cyttsp4_wait_ready_int_no_init(struct ts_data *ts,
	unsigned long timeout_ms)
{
	unsigned long uretval;
	int retval = 0;

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(
		&ts->ready_int_running, msecs_to_jiffies(timeout_ms));
	mutex_lock(&ts->data_lock);
	if (uretval == 0) {
		pr_err("%s: timeout waiting for interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
	}

	return retval;
}

static int _cyttsp4_wait_int_no_init(struct ts_data *ts,
	unsigned long timeout_ms)
{
	unsigned long uretval;
	int retval = 0;

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(
		&ts->int_running, msecs_to_jiffies(timeout_ms));
	mutex_lock(&ts->data_lock);
	if (uretval == 0) {
		pr_err("%s: timeout waiting for interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
	}

	return retval;
}

static int _cyttsp4_wait_int(struct ts_data *ts, unsigned long timeout_ms)
{
	int retval = 0;

	INIT_COMPLETION(ts->int_running);
	retval = _cyttsp4_wait_int_no_init(ts, timeout_ms);
	if (retval < 0) {
		pr_err("%s: timeout waiting for interrupt\n",
			__func__);
	}

	return retval;
}

static int _cyttsp4_wait_si_int(struct ts_data *ts, unsigned long timeout_ms)
{
	unsigned long uretval;
	int retval = 0;

	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(
		&ts->si_int_running, msecs_to_jiffies(timeout_ms));
	mutex_lock(&ts->data_lock);
	if (uretval == 0) {
		pr_err("%s: timeout waiting for bootloader interrupt\n",
			__func__);
		retval = -ETIMEDOUT;
	}

	return retval;
}

static void _cyttsp4_queue_startup(struct ts_data *ts, bool was_suspended)
{
	ts->was_suspended = was_suspended;
	queue_work(ts->cyttsp4_wq,
		&ts->cyttsp4_resume_startup_work);
	pr_info("%s: startup queued\n", __func__);
}

#if defined(CY_AUTO_LOAD_TOUCH_PARAMS) || \
	defined(CY_AUTO_LOAD_DDATA) || defined(CY_AUTO_LOAD_MDATA) || \
	defined(CY_USE_DEV_DEBUG_TOOLS) || \
	defined(CONFIG_SEC_TSP_FACTORY_TEST)
static u16 _cyttsp4_calc_partial_crc(struct ts_data *ts,
	u8 *pdata, size_t ndata, u16 crc)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < ndata; i++) {
		crc ^= ((u16)pdata[i] << 8);

		for (j = 8; j > 0; --j) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc = crc << 1;
		}
	}

	return crc;
}

static void _cyttsp4_calc_crc(struct ts_data *ts,
	u8 *pdata, size_t ndata, u8 *crc_h, u8 *crc_l)
{
	u16 crc = 0;

	if (pdata == NULL)
		pr_err("%s: Null data ptr\n", __func__);
	else if (ndata == 0)
		pr_err("%s: Num data is 0\n", __func__);
	else {
		/* Calculate CRC */
		crc = 0xFFFF;
		crc = _cyttsp4_calc_partial_crc(ts, pdata, ndata, crc);
		*crc_h = crc / 256;
		*crc_l = crc % 256;
	}
}
#endif /* --CY_AUTO_LOAD_TOUCH_PARAMS --CY_AUTO_LOAD_DDATA
	--CY_AUTO_LOAD_MDATA --CY_USE_DEV_DEBUG_TOOLS */

static bool _cyttsp4_chk_cmd_rdy(struct ts_data *ts, u8 cmd)
{
	bool cond = !!(cmd & CY_CMD_RDY_BIT);
	pr_debug("%s: cmd=%02X cond=%d\n", __func__, cmd, (int)cond);

	return cond;
}

static bool _cyttsp4_chk_mode_change(struct ts_data *ts, u8 cmd)
{
	bool cond = !(cmd & CY_MODE_CHANGE);
	pr_debug("%s: cmd=%02X cond=%d\n", __func__, cmd, (int)cond);

	return cond;
}

static void _cyttsp4_change_state(struct ts_data *ts,
	enum cyttsp4_driver_state new_state)
{
	ts->driver_state = new_state;
	_cyttsp4_pr_state(ts);
}

static int _cyttsp4_put_cmd_wait(struct ts_data *ts, u16 ofs,
	size_t cmd_len, const void *cmd_buf, unsigned long timeout_ms,
	bool (*cond)(struct ts_data *, u8), u8 *retcmd,
	int i2c_addr, bool use_subaddr)
{
	enum cyttsp4_driver_state tmp_state;
	unsigned long uretval = 0;
	u8 cmd = 0;
	int tries = 0;
	int retval = 0;

	/* unlock here to allow any pending irq to complete */
	tmp_state = ts->driver_state;
	_cyttsp4_change_state(ts, CY_TRANSFER_STATE);
	mutex_unlock(&ts->data_lock);
	mutex_lock(&ts->data_lock);
	_cyttsp4_change_state(ts, CY_CMD_STATE);
	INIT_COMPLETION(ts->int_running);
	mutex_unlock(&ts->data_lock);
	retval = _cyttsp4_write_block_data(ts, ofs, cmd_len,
		cmd_buf, i2c_addr, use_subaddr);
	if (retval < 0) {
		pr_err("%s: Fail writing cmd buf r=%d\n",
			__func__, retval);
		mutex_lock(&ts->data_lock);
		goto _cyttsp4_put_cmd_wait_exit;
	}
_cyttsp4_put_cmd_wait_retry:
	uretval = wait_for_completion_interruptible_timeout(
		&ts->int_running, msecs_to_jiffies(timeout_ms));
	mutex_lock(&ts->data_lock);

	retval = _cyttsp4_read_block_data(ts, ofs,
		sizeof(cmd), &cmd, i2c_addr, use_subaddr);
	if (retval < 0) {
		pr_err("%s: fail read cmd status  r=%d\n",
			__func__, retval);
	}
	if ((cond != NULL) && !cond(ts, cmd)) {
		if (uretval == 0) {
			pr_err("%s: timeout waiting for cmd ready\n",
				__func__);
			retval = -ETIMEDOUT;
		} else {
			if (tries++ < 2) {
				INIT_COMPLETION(ts->int_running);
				mutex_unlock(&ts->data_lock);
				goto _cyttsp4_put_cmd_wait_retry;
			} else {
				pr_err("%s: cmd not ready error"
					" cmd_stat=0x%02X\n",
					__func__, cmd);
				retval = -EIO;
			}
		}
	} else {
		/* got command ready */
		if (retcmd != NULL)
			*retcmd = cmd;
		retval = 0;
		pr_debug("%s: got command ready; cmd=%02X retcmd=%p tries=%d\n",
			__func__, cmd, retcmd, tries);
	}

_cyttsp4_put_cmd_wait_exit:
	_cyttsp4_change_state(ts, tmp_state);
	return retval;
}

static int _cyttsp4_handshake(struct ts_data *ts, u8 hst_mode)
{
	int retval = 0;
	u8 cmd = 0;

	cmd = hst_mode & CY_HANDSHAKE_BIT ?
		hst_mode & ~CY_HANDSHAKE_BIT :
		hst_mode | CY_HANDSHAKE_BIT;

	retval = _cyttsp4_write_block_data(ts, CY_REG_BASE,
		sizeof(cmd), (u8 *)&cmd,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);

	if (retval < 0) {
		pr_err("%s: bus write fail on handshake (ret=%d)\n",
			__func__, retval);
	}

	return retval;
}

static int _cyttsp4_cmd_handshake(struct ts_data *ts)
{
	u8 host_mode = 0;
	int retval = 0;

	retval = _cyttsp4_read_block_data(ts, CY_REG_BASE,
		sizeof(host_mode), &host_mode,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail read host mode r=%d\n",
			__func__, retval);
	} else {
		retval = _cyttsp4_handshake(ts, host_mode);
		if (retval < 0) {
			pr_err("%s: Fail handshake r=%d\n",
				__func__, retval);
		}
	}

	return retval;
}

#if defined(CY_AUTO_LOAD_TOUCH_PARAMS) || defined(CY_USE_DEV_DEBUG_TOOLS)
static void _cyttsp_read_table_crc(struct ts_data *ts, const u8 *ptable,
	u8 *crc_h, u8 *crc_l)
{
	size_t crc_loc = (ptable[3] * 256) + ptable[2];

	*crc_l = ptable[crc_loc];
	*crc_h = ptable[crc_loc + 1];
}
#endif

/* Get EBID Row Size is a Config mode command */
static int _cyttsp4_get_ebid_row_size(struct ts_data *ts)
{
	int retval = 0;
	u8 cmd = 0;
	u8 cmd_dat[CY_NUM_DAT + 1];	/* +1 for cmd byte */

	memset(cmd_dat, 0, sizeof(cmd_dat));
	cmd_dat[0] = CY_GET_EBID_ROW_SIZE;	/* get EBID row size command */

	retval = _cyttsp4_put_cmd_wait(ts, ts->si_ofs->cmd_ofs,
		sizeof(cmd_dat), cmd_dat, CY_HALF_SEC_TMO_MS,
		_cyttsp4_chk_cmd_rdy, &cmd,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail Get EBID row size command r=%d\n",
			__func__, retval);
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->cmd_ofs,
			sizeof(cmd_dat), cmd_dat,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: Fail get EBID row size r=%d\n",
				__func__, retval);
			ts->ebid_row_size = CY_EBID_ROW_SIZE_DFLT;
			pr_err("%s: Use default EBID row size=%d\n",
				__func__, ts->ebid_row_size);
		} else {
			ts->ebid_row_size = (cmd_dat[1] * 256) + cmd_dat[2];
			retval = _cyttsp4_cmd_handshake(ts);
			if (retval < 0) {
				pr_err("%s: Command handshake error r=%d\n",
					__func__, retval);
				/* continue anyway; rely on handshake tmo */
				retval = 0;
			}
		}
	}

	return retval;
}

static const u8 cyttsp4_security_key[] = {
	0xA5, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0x5A
};

#if defined(CY_AUTO_LOAD_TOUCH_PARAMS) || \
	defined(CY_AUTO_LOAD_DDATA) || defined(CY_AUTO_LOAD_MDATA) || \
	defined(CY_USE_DEV_DEBUG_TOOLS) || \
	defined(CONFIG_SEC_TSP_FACTORY_TEST)
/* Get EBID Row Data is a Config mode command */
static int _cyttsp4_get_ebid_data_tma400(struct ts_data *ts,
	enum cyttsp4_ic_ebid ebid, size_t row_id, u8 *pdata)
{
	int rc = 0;
	int retval = 0;
	u8 crc_h = 0;
	u8 crc_l = 0;
	u8 cmd = 0;
	u8 status = 0;
	u8 cmd_dat[CY_NUM_DAT + 1];	/* +1 for cmd byte */

	memset(cmd_dat, 0, sizeof(cmd_dat));
	cmd_dat[0] = CY_READ_EBID_DATA;	/* get EBID data command */
	cmd_dat[1] = row_id / 256;
	cmd_dat[2] = row_id % 256;
	cmd_dat[3] = ts->ebid_row_size / 256;
	cmd_dat[4] = ts->ebid_row_size % 256;
	cmd_dat[5] = ebid;

	if (pdata == NULL) {
		pr_err("%s: Get EBID=%d row=%d Data buffer err ptr=%p\n",
			__func__, ebid, row_id, pdata);
		goto _cyttsp4_get_ebid_data_tma400_exit;
	}

	retval = _cyttsp4_put_cmd_wait(ts, ts->si_ofs->cmd_ofs,
		sizeof(cmd_dat), cmd_dat, CY_HALF_SEC_TMO_MS,
		_cyttsp4_chk_cmd_rdy, &cmd,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail Get EBID=%d row=%d Data cmd r=%d\n",
			__func__, ebid, row_id, retval);
		goto _cyttsp4_get_ebid_data_tma400_exit;
	}

	retval = _cyttsp4_read_block_data(ts, ts->si_ofs->cmd_ofs + 1,
		sizeof(status), &status,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail get EBID=%d row=%d status r=%d\n",
			__func__, ebid, row_id, retval);
		goto _cyttsp4_get_ebid_data_tma400_exit;
	}

	if (status != 0x00) {
		pr_err("%s: Get EBID=%d row=%d status=%d error\n",
			__func__, ebid, row_id, status);
		retval = -EIO;
		goto _cyttsp4_get_ebid_data_tma400_exit;
	}

	retval = _cyttsp4_read_block_data(ts, ts->si_ofs->cmd_ofs + 1 + 5,
		ts->ebid_row_size + 2, pdata,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: fail EBID=%d row=%d data r=%d\n",
			__func__, ebid, row_id, retval);
		retval = -EIO;
	} else {
		_cyttsp4_calc_crc(ts, pdata, ts->ebid_row_size, &crc_h, &crc_l);
		if (pdata[ts->ebid_row_size] != crc_h ||
			pdata[ts->ebid_row_size + 1] != crc_l) {
			pr_err("%s: EBID=%d row_id=%d row_data_crc=%02X%02X"
				" not equal to calc_crc=%02X%02X\n",
				__func__, ebid, row_id,
				pdata[ts->ebid_row_size],
				pdata[ts->ebid_row_size + 1],
				crc_h, crc_l);
			/* continue anyway; allow handshake */
			rc = -EIO;
		}
		retval = _cyttsp4_cmd_handshake(ts);
		if (retval < 0) {
			pr_err("%s: Command handshake error r=%d\n",
				__func__, retval);
			/* continue anyway; rely on handshake tmo */
			retval = 0;
		}
		retval = rc;
	}

_cyttsp4_get_ebid_data_tma400_exit:
	return retval;
}

/* Put EBID Row Data is a Config mode command */
static int _cyttsp4_put_ebid_data_tma400(struct ts_data *ts,
	enum cyttsp4_ic_ebid ebid, size_t row_id, u8 *out_data)
{
	u8 calc_crc[2];
	u8 *pdata = NULL;
	u8 ret_cmd = 0;
	size_t psize = 0;
	u8 status = 0;
	int retval = 0;

	memset(calc_crc, 0, sizeof(calc_crc));
	psize = 1 + 5 + ts->ebid_row_size + sizeof(cyttsp4_security_key) + 2;
	pdata = kzalloc(psize, GFP_KERNEL);
	if (pdata == NULL || out_data == NULL) {
		pr_err("%s: Buffer ptr err EBID=%d row=%d"
			" alloc_ptr=%p out_data=%p\n",
			__func__, ebid, row_id, pdata, out_data);
		retval = -EINVAL;
	} else {
		pdata[0] = CY_WRITE_EBID_DATA;	/* put ebid data command */
		pdata[1] = row_id / 256;
		pdata[2] = row_id % 256;
		pdata[3] = ts->ebid_row_size / 256;
		pdata[4] = ts->ebid_row_size % 256;
		pdata[5] = ebid;
		memcpy(&pdata[1 + 5], out_data, ts->ebid_row_size);
		memcpy(&pdata[1 + 5 + ts->ebid_row_size],
			cyttsp4_security_key, sizeof(cyttsp4_security_key));
		_cyttsp4_calc_crc(ts, &pdata[1 + 5], ts->ebid_row_size,
			&calc_crc[0], &calc_crc[1]);
		memcpy(&pdata[1 + 5 + ts->ebid_row_size +
			sizeof(cyttsp4_security_key)],
			calc_crc, sizeof(calc_crc));

		retval = _cyttsp4_put_cmd_wait(ts, ts->si_ofs->cmd_ofs,
			psize, pdata, CY_HALF_SEC_TMO_MS,
			_cyttsp4_chk_cmd_rdy, &ret_cmd,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: Fail Put EBID=%d row=%d Data cmd r=%d\n",
				__func__, ebid, row_id, retval);
			goto _cyttsp4_put_ebid_data_tma400_exit;
		}

		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->cmd_ofs + 1,
			sizeof(status), &status,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: Fail put EBID=%d row=%d"
				" read status r=%d\n",
				__func__, ebid, row_id, retval);
			goto _cyttsp4_put_ebid_data_tma400_exit;
		}

		retval = _cyttsp4_cmd_handshake(ts);
		if (retval < 0) {
			pr_err("%s: Fail handshake on Put EBID=%d row=%d"
				" r=%d\n", __func__, ebid, row_id, retval);
			/* continue; rely on handshake tmo */
			retval = 0;
		}

		if (status != 0x00) {
			pr_err("%s: Put EBID=%d row=%d status=%d error\n",
				__func__, ebid, row_id, status);
			retval = -EIO;
		} else
			retval = 0;
	}
_cyttsp4_put_ebid_data_tma400_exit:
	if (pdata != NULL)
		kfree(pdata);
	return retval;
}
#endif /* --CY_AUTO_LOAD_TOUCH_PARAMS --CY_AUTO_LOAD_DDATA
	--CY_AUTO_LOAD_MDATA --CY_USE_DEV_DEBUG_TOOLS */

#if defined(CY_AUTO_LOAD_TOUCH_PARAMS) || defined(CY_USE_DEV_DEBUG_TOOLS)
/* Put All Touch Params is a Config mode command */
static int _cyttsp4_put_all_params_tma400(struct ts_data *ts)
{
	enum cyttsp4_ic_ebid ebid = CY_TCH_PARM_EBID;
	size_t row_id = 0;
	size_t num_rows = 0;
	size_t table_size = 0;
	size_t residue = 0;
	u8 *pdata = NULL;
	u8 *ptable = NULL;
	int retval = 0;

	pdata = kzalloc(ts->ebid_row_size, GFP_KERNEL);
	if (pdata == NULL) {
		pr_err("%s: Alloc error ebid=%d\n",
			__func__, ebid);
		retval = -ENOMEM;
	} else if (ts->params_regs == NULL)
		pr_err("%s: NULL param values table\n", __func__);
	else if (ts->params_regs
		->data == NULL)
		pr_err("%s: NULL param values table data\n", __func__);
	else if (ts->params_regs->size == 0)
		pr_err("%s: param values table size is 0\n", __func__);
	else {
		ptable = (u8 *)ts->private_data->sett
			[CY_IC_GRPNUM_TCH_PARM_VAL]->data;
		table_size = ts->private_data->sett
			[CY_IC_GRPNUM_TCH_PARM_VAL]->size;
		num_rows = table_size / ts->ebid_row_size;
		pr_debug("%s: num_rows=%d row_size=%d"
			" table_size=%d\n", __func__,
			num_rows, ts->ebid_row_size, table_size);
		for (row_id = 0; row_id < num_rows;) {
			memcpy(pdata, ptable, ts->ebid_row_size);
			pr_debug("%s: row=%d pdata=%p\n",
				__func__, row_id, pdata);
			_cyttsp4_pr_buf(ts, pdata, ts->ebid_row_size,
				"ebid_data");
			retval = _cyttsp4_put_ebid_data_tma400(ts,
				ebid, row_id, pdata);
			if (retval < 0) {
				pr_err("%s: Fail put row=%d r=%d\n",
					__func__, row_id, retval);
				break;
			} else {
				ptable += ts->ebid_row_size;
				row_id++;
			}
		}
		if (!(retval < 0)) {
			residue = table_size % ts->ebid_row_size;
			if (residue) {
				memset(pdata, 0, ts->ebid_row_size);
				memcpy(pdata, ptable, residue);
				pr_debug("%s: ebid=%d row=%d data:\n",
					__func__, ebid, row_id);
				_cyttsp4_pr_buf(ts, pdata, ts->ebid_row_size,
					"ebid_data");
				retval = _cyttsp4_put_ebid_data_tma400(ts,
					ebid, row_id, pdata);
				if (retval < 0) {
					pr_err("%s: Fail put row=%d r=%d\n",
						__func__, row_id, retval);
				}
			}
		}
	}

	if (pdata != NULL)
		kfree(pdata);

	return retval;
}
#endif /* --CY_AUTO_LOAD_TOUCH_PARAMS */

/* Check MDDATA is a Config mode command */
static int _cyttsp4_check_mddata_tma400(struct ts_data *ts, bool *updated)
{
	bool ddata_updated = false;
	bool mdata_updated = false;
#if defined(CY_AUTO_LOAD_DDATA) || defined(CY_AUTO_LOAD_MDATA)
	enum cyttsp4_ic_ebid ebid = CY_DDATA_EBID;
	size_t num_data = 0;
	size_t crc_ofs = 0;
	u8 crc_h = 0;
	u8 crc_l = 0;
#endif
	u8 *pdata = NULL;
	u8 *pmddata = NULL;
	int retval = 0;

	if (ts->ebid_row_size == 0) {
		pr_err("%s: fail allocate set MDDATA buffer\n", __func__);
		retval = -EINVAL;
		goto _cyttsp4_check_mddata_tma400_exit;
	}
	pdata = kzalloc(ts->ebid_row_size, GFP_KERNEL);
	if (pdata == NULL) {
		pr_err("%s: fail allocate set MDDATA buffer\n", __func__);
		retval = -ENOMEM;
		goto _cyttsp4_check_mddata_tma400_exit;
	}
	pmddata = kzalloc(ts->ebid_row_size, GFP_KERNEL);
	if (pmddata == NULL) {
		pr_err("%s: fail allocate set MDDATA buffer\n", __func__);
		retval = -ENOMEM;
		goto _cyttsp4_check_mddata_tma400_exit;
	}

#ifdef CY_AUTO_LOAD_DDATA
	/* check for platform_data DDATA */
	ebid = CY_DDATA_EBID;
	if (ts->private_data->sett[CY_IC_GRPNUM_DDATA_REC] == NULL) {
		pr_debug("%s: No platform DDATA table\n", __func__);
		goto _cyttsp4_check_mdata_block;
	}
	if (ts->private_data->sett[CY_IC_GRPNUM_DDATA_REC]->data == NULL) {
		pr_debug("%s: No platform DDATA table data\n", __func__);
		goto _cyttsp4_check_mdata_block;
	}
	if (ts->private_data->sett[CY_IC_GRPNUM_DDATA_REC]->size == 0) {
		pr_debug("%s: Platform DDATA table has size=0\n", __func__);
		goto _cyttsp4_check_mdata_block;
	}

	pr_debug("%s: call get ebid data for DDATA\n", __func__);
	retval = _cyttsp4_get_ebid_data_tma400(ts, ebid, 0, pdata);
	if (retval < 0) {
		pr_err("%s: Fail get DDATA r=%d\n", __func__, retval);
		goto _cyttsp4_check_mdata_block;
	}

	pr_debug("%s: copy pdata -> pmddata\n", __func__);
	memcpy(pmddata, pdata, 4);
	num_data = ts->private_data->sett
		[CY_IC_GRPNUM_DDATA_REC]->size < CY_NUM_DDATA ?
		ts->private_data->sett
		[CY_IC_GRPNUM_DDATA_REC]->size : CY_NUM_DDATA;
	pr_debug("%s: copy %d bytes from platform data to ddata array\n",
		__func__, num_data);
	memcpy(&pmddata[4], ts->private_data->sett
		[CY_IC_GRPNUM_DDATA_REC]->data, num_data);
	if (num_data < CY_NUM_DDATA)
		memset(&pmddata[4 + num_data], 0, CY_NUM_DDATA - num_data);
	crc_ofs = (pmddata[3] * 256) + pmddata[2];
	if (crc_ofs == 0)
		crc_ofs = 126;
	pr_debug("%s: ddata crc_ofs=%d num_data=%d\n",
		__func__, crc_ofs, num_data);

	_cyttsp4_calc_crc(ts, pmddata, crc_ofs, &crc_h, &crc_l);
	pmddata[crc_ofs] = crc_l;
	pmddata[crc_ofs+1] = crc_h;
	_cyttsp4_pr_buf(ts, pdata, ts->ebid_row_size, "pdata");
	_cyttsp4_pr_buf(ts, pmddata, ts->ebid_row_size, "pmddata");
	if (pmddata[crc_ofs] != pdata[crc_ofs] ||
		pmddata[crc_ofs+1] != pdata[crc_ofs+1]) {
		retval = _cyttsp4_put_ebid_data_tma400(ts, ebid, 0, pmddata);
		if (retval < 0)
			pr_err("%s: Fail put DDATA r=%d\n", __func__, retval);
		else
			ddata_updated = true;
	}

_cyttsp4_check_mdata_block:
#else
	ddata_updated = false;
#endif /* --CY_AUTO_LOAD_DDATA */

#ifdef CY_AUTO_LOAD_MDATA
	/* check for platform_data MDATA */
	memset(pdata, 0, ts->ebid_row_size);
	memset(pmddata, 0, ts->ebid_row_size);
	ebid = CY_MDATA_EBID;
	if (ts->private_data->sett[CY_IC_GRPNUM_MDATA_REC] == NULL) {
		pr_debug("%s: No platform MDATA table\n", __func__);
		goto _cyttsp4_check_mddata_tma400_exit;
	}
	if (ts->private_data->sett[CY_IC_GRPNUM_MDATA_REC]->data == NULL) {
		pr_debug("%s: No platform MDATA table data\n", __func__);
		goto _cyttsp4_check_mddata_tma400_exit;
	}
	if (ts->private_data->sett[CY_IC_GRPNUM_MDATA_REC]->size == 0) {
		pr_debug("%s: Platform MDATA table has size=0\n", __func__);
		goto _cyttsp4_check_mddata_tma400_exit;
	}

	retval = _cyttsp4_get_ebid_data_tma400(ts, ebid, 0, pdata);
	if (retval < 0) {
		pr_err("%s: Fail get MDATA r=%d\n", __func__, retval);
		goto _cyttsp4_check_mddata_tma400_exit;
	}

	memcpy(pmddata, pdata, 4);
	num_data = ts->private_data->sett
		[CY_IC_GRPNUM_MDATA_REC]->size < CY_NUM_MDATA ?
		ts->private_data->sett
		[CY_IC_GRPNUM_MDATA_REC]->size : CY_NUM_MDATA;
	pr_debug("%s: copy %d bytes from platform data to mdata array\n",
		__func__, num_data);
	memcpy(&pmddata[4], ts->private_data->sett
		[CY_IC_GRPNUM_MDATA_REC]->data, num_data);
	if (num_data < CY_NUM_MDATA)
		memset(&pmddata[4 + num_data], 0, CY_NUM_MDATA - num_data);
	crc_ofs = (pmddata[3] * 256) + pmddata[2];
	if (crc_ofs == 0)
		crc_ofs = 124;
	pr_debug("%s: mdata crc_ofs=%d num_data=%d\n",
		__func__, crc_ofs, num_data);
	_cyttsp4_calc_crc(ts, pmddata, crc_ofs, &crc_h, &crc_l);
	pmddata[crc_ofs] = crc_l;
	pmddata[crc_ofs+1] = crc_h;
	_cyttsp4_pr_buf(ts, pdata, ts->ebid_row_size, "pdata");
	_cyttsp4_pr_buf(ts, pmddata, ts->ebid_row_size, "pmddata");
	if (pmddata[crc_ofs] != pdata[crc_ofs] ||
		pmddata[crc_ofs+1] != pdata[crc_ofs+1]) {
		retval = _cyttsp4_put_ebid_data_tma400(ts, ebid, 0, pmddata);
		if (retval < 0)
			pr_err("%s: Fail put MDATA r=%d\n", __func__, retval);
		else
			mdata_updated = true;
	}
#else
	mdata_updated = false;
#endif /* --CY_AUTO_LOAD_MDATA */

_cyttsp4_check_mddata_tma400_exit:
	if (pdata != NULL)
		kfree(pdata);
	if (pmddata != NULL)
		kfree(pmddata);
	if (updated != NULL)
		*updated = ddata_updated || mdata_updated;
	return retval;
}

/*
 * change device mode - For example, change from
 * system information mode to operating mode
 */
static int _cyttsp4_set_device_mode(struct ts_data *ts,
	u8 new_mode, u8 new_cur_mode, char *mode)
{
	u8 cmd = 0;
	int retval = 0;

	cmd = new_mode + CY_MODE_CHANGE;

	retval = _cyttsp4_put_cmd_wait(ts, CY_REG_BASE,
		sizeof(cmd), &cmd, CY_HALF_SEC_TMO_MS,
		_cyttsp4_chk_mode_change, &cmd,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail Set mode command new_mode=%02X r=%d\n",
			__func__, new_mode, retval);
		goto _cyttsp4_set_device_mode_exit;
	}

	if (cmd != new_mode) {
		pr_err("%s: failed to switch to %s mode\n", __func__, mode);
		retval = -EIO;
	} else {
		ts->current_mode = new_cur_mode;
		retval = _cyttsp4_handshake(ts, cmd);
		if (retval < 0) {
			pr_err("%s: Fail handshake r=%d\n", __func__, retval);
			/* continue; rely on handshake tmo */
			retval = 0;
		}
	}

	pr_debug("%s: check op ready ret=%d host_mode=%02X\n",
		__func__, retval, cmd);

_cyttsp4_set_device_mode_exit:
	return retval;
}

static int _cyttsp4_set_mode(struct ts_data *ts, u8 new_mode)
{
	enum cyttsp4_driver_state new_state = CY_TRANSFER_STATE;
	u8 new_cur_mode = CY_MODE_OPERATIONAL;
	char *mode = NULL;
	unsigned long uretval = 0;
	int retval = 0;

	switch (new_mode) {
	case CY_OPERATE_MODE:
		new_cur_mode = CY_MODE_OPERATIONAL;
		mode = "operational";
		INIT_COMPLETION(ts->ready_int_running);
		_cyttsp4_change_state(ts, CY_READY_STATE);
		new_state = CY_ACTIVE_STATE;
		break;
	case CY_SYSINFO_MODE:
		new_cur_mode = CY_MODE_SYSINFO;
		mode = "sysinfo";
		new_state = CY_SYSINFO_STATE;
		break;
	case CY_CONFIG_MODE:
		new_cur_mode = CY_MODE_OPERATIONAL;
		mode = "config";
		new_state = ts->driver_state;

		break;
	default:
		pr_err("%s: invalid mode change request m=0x%02X\n",
			__func__, new_mode);
		retval = -EINVAL;
		goto _cyttsp_set_mode_exit;
	}

	retval = _cyttsp4_set_device_mode(ts,
		new_mode, new_cur_mode, mode);
	if (retval < 0) {
		pr_err("%s: Fail switch to %s mode\n", __func__, mode);
		_cyttsp4_change_state(ts, CY_IDLE_STATE);
	} else {
		if ((new_mode == CY_OPERATE_MODE) && ts->starting_up) {
			uretval = _cyttsp4_wait_ready_int_no_init(ts,
				CY_HALF_SEC_TMO_MS * 5);
		}
		_cyttsp4_change_state(ts, new_state);
	}

_cyttsp_set_mode_exit:
	return retval;
}

static int _cyttsp4_bits_2_bytes(struct ts_data *ts, int nbits, int *max)
{
	int nbytes;

	*max = 1 << nbits;

	for (nbytes = 0; nbits > 0;) {
		pr_debug("%s: nbytes=%d nbits=%d\n", __func__, nbytes, nbits);
		nbytes++;
		if (nbits > 8)
			nbits -= 8;
		else
			nbits = 0;
		pr_debug("%s: nbytes=%d nbits=%d\n", __func__, nbytes, nbits);
	}

	return nbytes;
}

static int _cyttsp4_get_sysinfo_regs(struct ts_data *ts)
{
	int btn = 0;
	int num_defined_keys = 0;
	u16 *key_table = NULL;
	enum cyttsp4_tch_abs abs = 0;
	int i = 0;
	int retval = 0;

	/* taejin already has ofs  */
	if (ts->si_ofs != NULL)
		goto _cyttsp4_get_sysinfo_regs_exit;
	/* pre-clear si_ofs structure */
	ts->si_ofs = kzalloc(sizeof(struct cyttsp4_sysinfo_ofs), GFP_KERNEL);

	/* get the sysinfo data offsets */
	retval = _cyttsp4_read_block_data(ts, CY_REG_BASE,
		sizeof(ts->sysinfo_data), &(ts->sysinfo_data),
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: fail read sysinfo data offsets r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_sysinfo_regs_exit_no_handshake;
	} else {
		/* Print sysinfo data offsets */
		_cyttsp4_pr_buf(ts, (u8 *)&ts->sysinfo_data,
			sizeof(ts->sysinfo_data), "sysinfo_data_offsets");

		/* convert sysinfo data offset bytes into integers */
		ts->si_ofs->map_sz = (ts->sysinfo_data.map_szh * 256) +
			ts->sysinfo_data.map_szl;
		ts->si_ofs->cydata_ofs = (ts->sysinfo_data.cydata_ofsh * 256) +
			ts->sysinfo_data.cydata_ofsl;
		ts->si_ofs->test_ofs = (ts->sysinfo_data.test_ofsh * 256) +
			ts->sysinfo_data.test_ofsl;
		ts->si_ofs->pcfg_ofs = (ts->sysinfo_data.pcfg_ofsh * 256) +
			ts->sysinfo_data.pcfg_ofsl;
		ts->si_ofs->opcfg_ofs = (ts->sysinfo_data.opcfg_ofsh * 256) +
			ts->sysinfo_data.opcfg_ofsl;
		ts->si_ofs->ddata_ofs = (ts->sysinfo_data.ddata_ofsh * 256) +
			ts->sysinfo_data.ddata_ofsl;
		ts->si_ofs->mdata_ofs = (ts->sysinfo_data.mdata_ofsh * 256) +
			ts->sysinfo_data.mdata_ofsl;
	}

	/* get the sysinfo cydata */
	ts->si_ofs->cydata_size = ts->si_ofs->test_ofs - ts->si_ofs->cydata_ofs;
	ts->sysinfo_ptr.cydata = kzalloc(ts->si_ofs->cydata_size, GFP_KERNEL);
	if (ts->sysinfo_ptr.cydata == NULL) {
		retval = -ENOMEM;
		pr_err("%s: fail alloc cydata memory r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_sysinfo_regs_exit;
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->cydata_ofs,
			ts->si_ofs->cydata_size, ts->sysinfo_ptr.cydata,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read cydata r=%d\n",
				__func__, retval);
			goto _cyttsp4_get_sysinfo_regs_exit;
		}
		/* Print sysinfo cydata */
		_cyttsp4_pr_buf(ts, (u8 *)ts->sysinfo_ptr.cydata,
			ts->si_ofs->cydata_size, "sysinfo_cydata");
	}
	/* get the sysinfo test data */
	ts->si_ofs->test_size = ts->si_ofs->pcfg_ofs - ts->si_ofs->test_ofs;
	ts->sysinfo_ptr.test = kzalloc(ts->si_ofs->test_size, GFP_KERNEL);
	if (ts->sysinfo_ptr.test == NULL) {
		retval = -ENOMEM;
		pr_err("%s: fail alloc test memory r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_sysinfo_regs_exit;
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->test_ofs,
			ts->si_ofs->test_size, ts->sysinfo_ptr.test,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read test data r=%d\n",
				__func__, retval);
			goto _cyttsp4_get_sysinfo_regs_exit;
		}
		/* Print sysinfo test data */
		_cyttsp4_pr_buf(ts, (u8 *)ts->sysinfo_ptr.test,
			ts->si_ofs->test_size, "sysinfo_test_data");
		if (ts->sysinfo_ptr.test->post_codel & 0x01) {
			pr_info("%s: Reset was a WATCHDOG RESET codel=%02X\n",
				__func__, ts->sysinfo_ptr.test->post_codel);
		}

		if (!(ts->sysinfo_ptr.test->post_codel & 0x02)) {
			pr_info("%s: Config Data CRC FAIL codel=%02X\n",
				__func__, ts->sysinfo_ptr.test->post_codel);
		}

		if (!(ts->sysinfo_ptr.test->post_codel & 0x04)) {
			pr_info("%s: PANEL TEST FAIL codel=%02X\n",
				__func__, ts->sysinfo_ptr.test->post_codel);
		}

		pr_info("%s: SCANNING is %s codel=%02X\n", __func__,
			ts->sysinfo_ptr.test->post_codel & 0x08 ? "ENABLED" :
			"DISABLED", ts->sysinfo_ptr.test->post_codel);
	}
	/* get the sysinfo pcfg data */
	ts->si_ofs->pcfg_size = ts->si_ofs->opcfg_ofs - ts->si_ofs->pcfg_ofs;
	ts->sysinfo_ptr.pcfg = kzalloc(ts->si_ofs->pcfg_size, GFP_KERNEL);
	if (ts->sysinfo_ptr.pcfg == NULL) {
		retval = -ENOMEM;
		pr_err("%s: fail alloc pcfg memory r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_sysinfo_regs_exit;
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->pcfg_ofs,
			ts->si_ofs->pcfg_size, ts->sysinfo_ptr.pcfg,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read pcfg data r=%d\n",
				__func__, retval);
			goto _cyttsp4_get_sysinfo_regs_exit;
		}
		/* Print sysinfo pcfg data */
		_cyttsp4_pr_buf(ts, (u8 *)ts->sysinfo_ptr.pcfg,
			ts->si_ofs->pcfg_size, "sysinfo_pcfg_data");
	}
	/* get the sysinfo opcfg data */
	ts->si_ofs->opcfg_size = ts->si_ofs->ddata_ofs - ts->si_ofs->opcfg_ofs;
	ts->sysinfo_ptr.opcfg = kzalloc(ts->si_ofs->opcfg_size, GFP_KERNEL);
	if (ts->sysinfo_ptr.opcfg == NULL) {
		retval = -ENOMEM;
		pr_err("%s: fail alloc opcfg memory r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_sysinfo_regs_exit;
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->opcfg_ofs,
			ts->si_ofs->opcfg_size, ts->sysinfo_ptr.opcfg,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read opcfg data r=%d\n",
				__func__, retval);
			goto _cyttsp4_get_sysinfo_regs_exit;
		}
		ts->si_ofs->cmd_ofs = ts->sysinfo_ptr.opcfg->cmd_ofs;
		ts->si_ofs->rep_ofs = ts->sysinfo_ptr.opcfg->rep_ofs;
		ts->si_ofs->rep_sz = (ts->sysinfo_ptr.opcfg->rep_szh * 256) +
			ts->sysinfo_ptr.opcfg->rep_szl;
		ts->si_ofs->num_btns = ts->sysinfo_ptr.opcfg->num_btns;
		if (ts->si_ofs->num_btns == 0)
			ts->si_ofs->num_btn_regs = 0;
		else {
			ts->si_ofs->num_btn_regs = ts->si_ofs->num_btns /
				CY_NUM_BTN_PER_REG;
			if (ts->si_ofs->num_btns % CY_NUM_BTN_PER_REG)
				ts->si_ofs->num_btn_regs++;
		}
		ts->si_ofs->tt_stat_ofs = ts->sysinfo_ptr.opcfg->tt_stat_ofs;
		ts->si_ofs->obj_cfg0 = ts->sysinfo_ptr.opcfg->obj_cfg0;
		ts->si_ofs->max_tchs = ts->sysinfo_ptr.opcfg->max_tchs &
			CY_BYTE_OFS_MASK;
		ts->si_ofs->tch_rec_siz = ts->sysinfo_ptr.opcfg->tch_rec_siz &
			CY_BYTE_OFS_MASK;

		/* Get the old touch fields */
		for (abs = CY_TCH_X; abs < CY_NUM_OLD_TCH_FIELDS; abs++) {
			ts->si_ofs->tch_abs[abs].ofs =
				ts->sysinfo_ptr.opcfg->tch_rec_old[abs].loc &
				CY_BYTE_OFS_MASK;
			ts->si_ofs->tch_abs[abs].size =
				_cyttsp4_bits_2_bytes(ts,
				ts->sysinfo_ptr.opcfg->tch_rec_old[abs].size,
				&ts->si_ofs->tch_abs[abs].max);
			ts->si_ofs->tch_abs[abs].bofs =
				(ts->sysinfo_ptr.opcfg->tch_rec_old[abs].loc &
				CY_BOFS_MASK) >> CY_BOFS_SHIFT;
		}

		/* skip over the button fields */

		/* Get the new touch fields */
		for (i = 0; abs < CY_TCH_NUM_ABS; abs++, i++) {
			ts->si_ofs->tch_abs[abs].ofs =
				ts->sysinfo_ptr.opcfg->tch_rec_new[i].loc &
				CY_BYTE_OFS_MASK;
			ts->si_ofs->tch_abs[abs].size =
				_cyttsp4_bits_2_bytes(ts,
				ts->sysinfo_ptr.opcfg->tch_rec_new[i].size,
				&ts->si_ofs->tch_abs[abs].max);
			ts->si_ofs->tch_abs[abs].bofs =
				(ts->sysinfo_ptr.opcfg->tch_rec_new[i].loc &
				CY_BOFS_MASK) >> CY_BOFS_SHIFT;
		}

		ts->si_ofs->btn_rec_siz = ts->sysinfo_ptr.opcfg->btn_rec_siz;
		ts->si_ofs->btn_diff_ofs = ts->sysinfo_ptr.opcfg->btn_diff_ofs;
		ts->si_ofs->btn_diff_siz = ts->sysinfo_ptr.opcfg->btn_diff_siz;
		ts->si_ofs->mode_size = ts->si_ofs->tt_stat_ofs + 1;
		ts->si_ofs->data_size = ts->si_ofs->max_tchs *
			ts->sysinfo_ptr.opcfg->tch_rec_siz;
		if (ts->si_ofs->num_btns)
			ts->si_ofs->mode_size += ts->si_ofs->num_btn_regs;

		/* Print sysinfo opcfg data */
		_cyttsp4_pr_buf(ts, (u8 *)ts->sysinfo_ptr.opcfg,
			ts->si_ofs->opcfg_size, "sysinfo_opcfg_data");
	}

	/* get the sysinfo ddata data */
	ts->si_ofs->ddata_size = ts->si_ofs->mdata_ofs - ts->si_ofs->ddata_ofs;
	ts->sysinfo_ptr.ddata = kzalloc(ts->si_ofs->ddata_size, GFP_KERNEL);
	if (ts->sysinfo_ptr.ddata == NULL) {
		pr_err("%s: fail alloc ddata memory r=%d\n",
			__func__, retval);
		/* continue */
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->ddata_ofs,
			ts->si_ofs->ddata_size, ts->sysinfo_ptr.ddata,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read ddata data r=%d\n",
				__func__, retval);
			goto _cyttsp4_get_sysinfo_regs_exit;
		}
		/* Print sysinfo ddata */
		_cyttsp4_pr_buf(ts, (u8 *)ts->sysinfo_ptr.ddata,
			ts->si_ofs->ddata_size, "sysinfo_ddata");
	}
	/* get the sysinfo mdata data */
	ts->si_ofs->mdata_size = ts->si_ofs->map_sz - ts->si_ofs->mdata_ofs;
	ts->sysinfo_ptr.mdata = kzalloc(ts->si_ofs->mdata_size, GFP_KERNEL);
	if (ts->sysinfo_ptr.mdata == NULL) {
		pr_err("%s: fail alloc mdata memory r=%d\n",
			__func__, retval);
		/* continue */
	} else {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->mdata_ofs,
			ts->si_ofs->mdata_size, ts->sysinfo_ptr.mdata,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read mdata data r=%d\n",
				__func__, retval);
			goto _cyttsp4_get_sysinfo_regs_exit;
		}
		/* Print sysinfo mdata */
		_cyttsp4_pr_buf(ts, (u8 *)ts->sysinfo_ptr.mdata,
			ts->si_ofs->mdata_size, "sysinfo_mdata");
	}

	if (ts->si_ofs->num_btns) {
		ts->si_ofs->btn_keys_size = ts->si_ofs->num_btns *
			sizeof(struct cyttsp4_btn);
		ts->btn = kzalloc(ts->si_ofs->btn_keys_size, GFP_KERNEL);
		if (ts->btn == NULL) {
			pr_err("%s: fail alloc btn_keys memory r=%d\n",
				__func__, retval);
		} else {
			if (ts->private_data->sett
				[CY_IC_GRPNUM_BTN_KEYS] == NULL)
				num_defined_keys = 0;
			else if (ts->private_data->sett
				[CY_IC_GRPNUM_BTN_KEYS]->data == NULL)
				num_defined_keys = 0;
			else
				num_defined_keys = ts->private_data->sett
					[CY_IC_GRPNUM_BTN_KEYS]->size;
			for (btn = 0; btn < ts->si_ofs->num_btns &&
				btn < num_defined_keys; btn++) {
				key_table = (u16 *)ts->private_data->sett
					[CY_IC_GRPNUM_BTN_KEYS]->data;
				ts->btn[btn].key_code = key_table[btn];
				ts->btn[btn].enabled = true;
			}
			for (; btn < ts->si_ofs->num_btns; btn++) {
				ts->btn[btn].key_code = KEY_RESERVED;
				ts->btn[btn].enabled = true;
			}
		}
	} else {
		ts->si_ofs->btn_keys_size = 0;
		ts->btn = NULL;
	}

	if (ts->xy_mode == NULL)
		ts->xy_mode = kzalloc(ts->si_ofs->mode_size, GFP_KERNEL);
	if (ts->xy_data == NULL)
		ts->xy_data = kzalloc(ts->si_ofs->data_size, GFP_KERNEL);
	if (ts->xy_data_touch1 == NULL) {
		ts->xy_data_touch1 = kzalloc(ts->si_ofs->tch_rec_siz + 1,
			GFP_KERNEL);
	}
	if (ts->btn_rec_data == NULL) {
		ts->btn_rec_data = kzalloc(ts->si_ofs->btn_rec_siz *
			ts->si_ofs->num_btns, GFP_KERNEL);
	}
	if ((ts->xy_mode == NULL) || (ts->xy_data == NULL) ||
		(ts->xy_data_touch1 == NULL) || (ts->btn_rec_data == NULL)) {
		pr_err("%s: fail memory alloc xy_mode=%p xy_data=%p"
			"xy_data_touch1=%p btn_rec_data=%p\n", __func__,
			ts->xy_mode, ts->xy_data,
			ts->xy_data_touch1, ts->btn_rec_data);
		/* continue */
	}

	pr_debug("%s: xy_mode=%p xy_data=%p xy_data_touch1=%p\n",
		__func__, ts->xy_mode, ts->xy_data, ts->xy_data_touch1);

_cyttsp4_get_sysinfo_regs_exit:
	/* provide flow control handshake */
	retval = _cyttsp4_handshake(ts, ts->sysinfo_data.hst_mode);
	if (retval < 0) {
		pr_err("%s: handshake fail on sysinfo reg\n",
			__func__);
		/* continue; rely on handshake tmo */
	}

_cyttsp4_get_sysinfo_regs_exit_no_handshake:
	return retval;
}

static int _cyttsp4_load_status_regs(struct ts_data *ts)
{
	int rep_stat_ofs = 0;
	int retval = 0;

	rep_stat_ofs = ts->si_ofs->rep_ofs + 1;
	if (ts->xy_mode == NULL) {
		pr_err("%s: mode ptr not yet initialized xy_mode=%p\n",
			__func__, ts->xy_mode);
		/* continue */
	} else {
		retval = _cyttsp4_read_block_data(ts, CY_REG_BASE,
			ts->si_ofs->mode_size, ts->xy_mode,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: fail read mode regs r=%d\n",
				__func__, retval);
			retval = -EIO;
		}
		_cyttsp4_pr_buf(ts, ts->xy_mode, ts->si_ofs->mode_size,
			"xy_mode");
	}
	return retval;
}

static void _cyttsp4_btn_key_release(struct ts_data *ts,
	int cur_btn, u8 cur_btn_mask, int num_btns)
{
	int btn = 0;

	/* Check for button releases */
	for (btn = 0; btn < num_btns; btn++) {
		if (ts->btn[cur_btn + btn].enabled) {
			switch ((cur_btn_mask >> (btn * CY_BITS_PER_BTN)) &
				(CY_NUM_BTN_EVENT_ID - 1)) {
			case (CY_BTN_RELEASED):
				if (ts->btn[cur_btn + btn].state ==
					CY_BTN_PRESSED) {
					input_report_key(ts->input_dev,
						ts->btn[cur_btn + btn].key_code,
						CY_BTN_RELEASED);
					ts->btn[cur_btn + btn].state =
						CY_BTN_RELEASED;
					input_sync(ts->input_dev);
					pr_debug("%s: btn=%d key_code=%d"
						" RELEASED\n", __func__,
						cur_btn + btn, ts->btn
						[cur_btn + btn].key_code);
				}
				break;
			case (CY_BTN_PRESSED):
				break;
			default:
				break;
			}
		}
	}
	return;
}

static void _cyttsp4_btn_key_press(struct ts_data *ts,
	int cur_btn, u8 cur_btn_mask, int num_btns)
{
	int btn = 0;

	/* Check for button presses */
	for (btn = 0; btn < num_btns; btn++) {
		if (ts->btn[cur_btn + btn].enabled) {
			switch ((cur_btn_mask >> (btn * CY_BITS_PER_BTN)) &
				(CY_NUM_BTN_EVENT_ID - 1)) {
			case (CY_BTN_RELEASED):
				break;
			case (CY_BTN_PRESSED):
				if (ts->btn[cur_btn + btn].state ==
					CY_BTN_RELEASED) {
					input_report_key(ts->input_dev,
						ts->btn[cur_btn + btn].key_code,
						CY_BTN_PRESSED);
					ts->btn[cur_btn + btn].state =
						CY_BTN_PRESSED;
					input_sync(ts->input_dev);
					pr_debug("%s: btn=%d key_code=%d"
						" PRESSED\n", __func__,
						cur_btn + btn, ts->btn
						[cur_btn + btn].key_code);
				}
				break;
			default:
				break;
			}
		}
	}
	return;
}

static void _cyttsp4_get_touch_axis(struct ts_data *ts,
	enum cyttsp4_tch_abs abs, int *axis, int size,
	int max, u8 *xy_data, int bofs)
{
	int nbyte = 0;
	int next = 0;

	for (nbyte = 0, *axis = 0, next = 0; nbyte < size; nbyte++) {
		*axis = (*axis * 256) + (xy_data[next] >> bofs);
		next++;
	}

	*axis &= max - 1;

	/* sign extend signals that can have negative values */
	if (abs == CY_TCH_OR) {
		if (*axis >= (max / 2))
			*axis = -((~(*axis) & (max - 1)) + 1);
	}
}

static void _cyttsp4_get_touch(struct ts_data *ts,
	struct cyttsp4_touch *touch, u8 *xy_data)
{
	enum cyttsp4_tch_abs abs = 0;

	for (abs = CY_TCH_X; abs < CY_TCH_NUM_ABS; abs++) {
		_cyttsp4_get_touch_axis(ts, abs, &touch->abs[abs],
			ts->si_ofs->tch_abs[abs].size,
			ts->si_ofs->tch_abs[abs].max,
			xy_data + ts->si_ofs->tch_abs[abs].ofs,
			ts->si_ofs->tch_abs[abs].bofs);
	}

}

static void _cyttsp4_get_mt_touches(struct ts_data *ts, int num_cur_tch)
{
	struct cyttsp4_touch touch;
	int i = 0;

	memset(&touch, 0, sizeof(struct cyttsp4_touch));
	for (i = 0; i < num_cur_tch; i++) {
		_cyttsp4_get_touch(ts, &touch,
			ts->xy_data + (i * ts->si_ofs->tch_rec_siz));
		if (touch.abs[CY_TCH_T] < 0 ||
				touch.abs[CY_TCH_T] > MAX_TOUCH) {
			pr_err("%s: touch=%d has bad track_id=%d max_id=%d\n",
				__func__, i, touch.abs[CY_TCH_T],
				MAX_TOUCH);
			return;
		}

		if (touch.abs[CY_TCH_E] == CY_EV_LIFTOFF) {
			/* If lift-off, then skip the touch */
			pr_debug("%s: e=%d lift-off\n",
				__func__, touch.abs[CY_TCH_E]);
			input_mt_slot(ts->input_dev, touch.abs[CY_TCH_T]);
			input_mt_report_slot_state(ts->input_dev,
					MT_TOOL_FINGER, false);
			ts->finger_state[touch.abs[CY_TCH_T]] = 0;
			continue;
		}

		/*
		 * TMA400 size and orientation fields:
		 * if pressure is non-zero and major touch
		 * signal is zero, then set major and minor touch
		 * signal to minimum non-zero value
		 */
		if ((touch.abs[CY_TCH_P] > 0) &&
				(touch.abs[CY_TCH_MAJ] == 0)) {
			touch.abs[CY_TCH_MAJ] = 1;
			touch.abs[CY_TCH_MIN] = 1;
		}

		input_mt_slot(ts->input_dev, touch.abs[CY_TCH_T]);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
				touch.abs[CY_TCH_X]);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
				touch.abs[CY_TCH_Y]);
		input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
				touch.abs[CY_TCH_P]);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
				touch.abs[CY_TCH_MAJ]);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR,
				touch.abs[CY_TCH_MIN]);
		input_report_abs(ts->input_dev, ABS_MT_ORIENTATION,
				touch.abs[CY_TCH_OR]);

		if (ts->finger_state[touch.abs[CY_TCH_T]] == 0)
			ts->finger_state[touch.abs[CY_TCH_T]] = 1;

		pr_debug("%s: x=%d y=%d z=%d M=%d m=%d o=%d e=%d\n",
			__func__,
			touch.abs[CY_TCH_X],
			touch.abs[CY_TCH_Y],
			touch.abs[CY_TCH_P],
			touch.abs[CY_TCH_MAJ],
			touch.abs[CY_TCH_MIN],
			touch.abs[CY_TCH_OR],
			touch.abs[CY_TCH_E]);
	}
	input_sync(ts->input_dev);
	ts->num_prv_tch = num_cur_tch;

	return;
}

/* read xy_data for all current touches */
static int _cyttsp4_xy_worker(struct ts_data *ts)
{
	struct cyttsp4_touch touch;
	u8 num_cur_tch = 0;
	u8 hst_mode = 0;
	u8 rep_len = 0;
	u8 rep_stat = 0;
	u8 tt_stat = 0;
	int i = 0;
	int num_cur_btn = 0;
	int cur_reg = 0;
	u8 cur_btn_mask = 0;
	int cur_btn = 0;

	enum cyttsp4_btn_state btn_state = CY_BTN_RELEASED;
	int retval = 0;

	/*
	 * Get event data from CYTTSP device.
	 * The event data includes all data
	 * for all active touches.
	 */
	/*
	 * Use 2 reads: first to get mode bytes,
	 * second to get status (touch count) and touch 1 data.
	 * An optional 3rd read to get touch 2 - touch n data.
	 */
	memset(&touch, 0, sizeof(struct cyttsp4_touch));
	memset(ts->xy_mode, 0, ts->si_ofs->mode_size);
	memset(ts->xy_data_touch1, 0, 1 + ts->si_ofs->tch_rec_siz);

	retval = _cyttsp4_load_status_regs(ts);
	if (retval < 0) {
		/*
		 * bus failure implies Watchdog -> bootloader running
		 * on TMA884 parts
		*/
		pr_err("%s: 1st read fail on mode regs r=%d\n",
			__func__, retval);
		retval = -EIO;
		goto _cyttsp4_xy_worker_exit;
	}
	retval = _cyttsp4_read_block_data(ts, ts->si_ofs->tt_stat_ofs,
		1+ts->si_ofs->tch_rec_siz, ts->xy_data_touch1,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		/* bus failure may imply bootloader running */
		pr_err("%s: read fail on mode regs r=%d\n",
			__func__, retval);
		retval = -EIO;
		goto _cyttsp4_xy_worker_exit;
	}

	hst_mode = ts->xy_mode[CY_REG_BASE];
	rep_len = ts->xy_mode[ts->si_ofs->rep_ofs];
	rep_stat = ts->xy_mode[ts->si_ofs->rep_ofs + 1];
	tt_stat = ts->xy_data_touch1[0];
	pr_debug("%s: hst_mode=%02X rep_len=%d rep_stat=%02X tt_stat=%02X\n",
		__func__, hst_mode, rep_len, rep_stat, tt_stat);

	if (rep_len == 0) {
		pr_err("%s: report length error rep_len=%d\n",
			__func__, rep_len);
		goto _cyttsp4_xy_worker_exit;
	}

	if (GET_NUM_TOUCHES(tt_stat) > 0) {
		memcpy(ts->xy_data, ts->xy_data_touch1 + 1,
			ts->si_ofs->tch_rec_siz);
	}
	if (GET_NUM_TOUCHES(tt_stat) > 1) {
		retval = _cyttsp4_read_block_data(ts, ts->si_ofs->tt_stat_ofs +
			1 + ts->si_ofs->tch_rec_siz,
			(GET_NUM_TOUCHES(tt_stat) - 1) *
			ts->si_ofs->tch_rec_siz,
			ts->xy_data + ts->si_ofs->tch_rec_siz,
			ts->private_data->addr[CY_TCH_ADDR_OFS], true);
		if (retval < 0) {
			pr_err("%s: read fail on touch regs r=%d\n",
				__func__, retval);
			goto _cyttsp4_xy_worker_exit;
		}
	}

	/* provide flow control handshake */
	retval = _cyttsp4_handshake(ts, hst_mode);
	if (retval < 0) {
		pr_err("%s: handshake fail on operational reg\n",
			__func__);
		/* continue; rely on handshake tmo */
		retval = 0;
	}

	/* determine number of currently active touches */
	num_cur_tch = GET_NUM_TOUCHES(tt_stat);

	/* print xy data */
	_cyttsp4_pr_buf(ts, ts->xy_data, num_cur_tch *
		ts->si_ofs->tch_rec_siz, "xy_data");

	/* check for any error conditions */
	if (ts->driver_state == CY_IDLE_STATE) {
		pr_err("%s: IDLE STATE detected\n", __func__);
		retval = 0;
		goto _cyttsp4_xy_worker_exit;
	} else if (IS_BAD_PKT(rep_stat)) {
		pr_err("%s: Invalid buffer detected\n", __func__);
		retval = 0;
		goto _cyttsp4_xy_worker_exit;
	} else if (IS_BOOTLOADERMODE(rep_stat)) {
		pr_info("%s: BL mode found in ACTIVE state\n",
			__func__);
		retval = -EIO;
		goto _cyttsp4_xy_worker_exit;
	} else if (GET_HSTMODE(hst_mode) == GET_HSTMODE(CY_SYSINFO_MODE)) {
		/* if in sysinfo mode switch to op mode */
		pr_err("%s: Sysinfo mode=0x%02X detected in ACTIVE state\n",
			__func__, hst_mode);
		retval = _cyttsp4_set_mode(ts, CY_OPERATE_MODE);
		if (retval < 0) {
			_cyttsp4_change_state(ts, CY_IDLE_STATE);
			pr_err("%s: Fail set operational mode (r=%d)\n",
				__func__, retval);
		} else {
			_cyttsp4_change_state(ts, CY_ACTIVE_STATE);
			pr_debug("%s: enable handshake\n", __func__);
		}
		goto _cyttsp4_xy_worker_exit;
	} else if (IS_LARGE_AREA(tt_stat)) {
		/* terminate all active tracks */
		num_cur_tch = 0;
		pr_debug("%s: Large area detected\n", __func__);
	} else if (num_cur_tch > ts->si_ofs->max_tchs) {
		if (num_cur_tch == 0x1F) {
			/* terminate all active tracks */
			pr_err("%s: Num touch err detected (n=%d)\n",
				__func__, num_cur_tch);
			num_cur_tch = 0;
		} else {
			pr_err("%s: too many tch; set to max tch (n=%d c=%d)\n",
				__func__, num_cur_tch, ts->si_ofs->max_tchs);
			num_cur_tch = ts->si_ofs->max_tchs;
		}
	}

	pr_debug("%s: tsp: num_cur_tch=%d\n", __func__, num_cur_tch);

	/* extract xy_data for all currently reported touches */
	if (num_cur_tch) {
		_cyttsp4_get_mt_touches(ts, num_cur_tch);
	} else {
#ifdef CONFIG_TOUCH_DVFS
		mod_timer(&dvfs_timer,
			jiffies + msecs_to_jiffies(DVFS_TIME));
#endif
		if (ts->num_prv_tch != 0) {
			for (i = 0; i < MAX_TOUCH; i++) {
				ts->finger_state[i] = 0;
				input_mt_slot(ts->input_dev, i);
				input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, false);
			}
			input_sync(ts->input_dev);
		}
		ts->num_prv_tch = 0;
	}

	if (ts->si_ofs->num_btns > 0) {
		for (btn_state = CY_BTN_RELEASED; btn_state < CY_BTN_NUM_STATE;
			btn_state++) {
			for (cur_reg = 0, cur_btn = 0,
				num_cur_btn = ts->si_ofs->num_btns;
				cur_reg < ts->si_ofs->num_btn_regs;
				cur_reg++,
				cur_btn += CY_NUM_BTN_PER_REG,
				num_cur_btn -= CY_NUM_BTN_PER_REG) {
				if (num_cur_btn > 0) {
					cur_btn_mask = ts->xy_mode
						[ts->si_ofs->rep_ofs +
						2 + cur_reg];
					if (num_cur_btn / CY_NUM_BTN_PER_REG)
						i = CY_NUM_BTN_PER_REG;
					else
						i = num_cur_btn;
					switch (btn_state) {
					case CY_BTN_RELEASED:
						_cyttsp4_btn_key_release(ts,
							cur_btn,
							cur_btn_mask, i);
						break;
					case CY_BTN_PRESSED:
						_cyttsp4_btn_key_press(ts,
							cur_btn,
							cur_btn_mask, i);
						break;
					default:
						break;
					}
				}
			}
		}
	}

	pr_debug("%s:\n", __func__);

	retval = 0;
_cyttsp4_xy_worker_exit:
	return retval;
}

#ifdef CY_USE_WATCHDOG
#define CY_TIMEOUT msecs_to_jiffies(1000)
static void _cyttsp4_start_wd_timer(struct ts_data *ts)
{
	mod_timer(&ts->timer, jiffies + CY_TIMEOUT);

	return;
}

static void _cyttsp4_stop_wd_timer(struct ts_data *ts)
{
	del_timer(&ts->timer);
	cancel_work_sync(&ts->work);

	return;
}

static void cyttsp4_timer_watchdog(struct work_struct *work)
{
	struct ts_data *ts = container_of(work, struct ts_data, work);
	u8 rep_stat = 0;
	int retval = 0;

	if (ts == NULL) {
		pr_err("%s: NULL context pointer\n", __func__);
		return;
	}

	mutex_lock(&ts->data_lock);
	if (ts->driver_state == CY_ACTIVE_STATE) {
		retval = _cyttsp4_load_status_regs(ts);
		if (retval < 0) {
			pr_err("%s: failed to access device"
				" in watchdog timer r=%d\n", __func__, retval);
			_cyttsp4_queue_startup(ts, false);
			goto cyttsp4_timer_watchdog_exit_error;
		}
		rep_stat = ts->xy_mode[ts->si_ofs->rep_ofs + 1];
		if (IS_BOOTLOADERMODE(rep_stat)) {
			pr_err("%s: device found in bootloader mode"
				" when operational mode rep_stat=0x%02X\n",
				__func__, rep_stat);
			_cyttsp4_queue_startup(ts, false);
			goto cyttsp4_timer_watchdog_exit_error;
		}
	}

	_cyttsp4_start_wd_timer(ts);
 cyttsp4_timer_watchdog_exit_error:
	mutex_unlock(&ts->data_lock);
	return;
}

static void cyttsp4_timer(unsigned long handle)
{
	struct ts_data *ts = (struct ts_data *)handle;

	if (!work_pending(&ts->work))
		schedule_work(&ts->work);

	return;
}
#endif

static int _cyttsp4_soft_reset(struct ts_data *ts)
{
	u8 cmd = CY_SOFT_RESET_MODE;

	return _cyttsp4_write_block_data(ts, CY_REG_BASE,
		sizeof(cmd), &cmd,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
}

static int _cyttsp4_reset(struct ts_data *ts)
{
	enum cyttsp4_driver_state tmp_state = ts->driver_state;
	int retval = 0;

	if (ts->private_data->hw_reset) {
		retval = ts->private_data->hw_reset();
		if (retval == -ENOSYS) {
			retval = _cyttsp4_soft_reset(ts);
			ts->soft_reset_asserted = true;
		} else
			ts->soft_reset_asserted = false;
	} else {
		retval = _cyttsp4_soft_reset(ts);
		ts->soft_reset_asserted = true;
	}

	if (retval < 0) {
		_cyttsp4_pr_state(ts);
		return retval;
	} else {
		ts->current_mode = CY_MODE_BOOTLOADER;
		ts->driver_state = CY_BL_STATE;
		if (tmp_state != CY_BL_STATE)
			_cyttsp4_pr_state(ts);
		return retval;
	}
}

static void cyttsp4_ts_work_func(struct work_struct *work)
{
	struct ts_data *ts =
		container_of(work, struct ts_data, cyttsp4_resume_startup_work);
	int retval = 0;
	int i;

	pr_err("%s: %d: wd timer stop\n", __func__, __LINE__);
#ifdef CY_USE_WATCHDOG
	_cyttsp4_stop_wd_timer(ts);
#endif
	mutex_lock(&ts->data_lock);

	ts->num_prv_tch = 0;
	for (i = 0; i < ts->si_ofs->max_tchs; i++)
		input_mt_sync(ts->input_dev);
	input_report_key(ts->input_dev, KEY_MENU, 0);
	input_report_key(ts->input_dev, KEY_BACK, 0);
	input_sync(ts->input_dev);
	retval = _cyttsp4_startup(ts);
	if (retval < 0) {
		pr_err("%s: Startup failed with error code %d\n",
			__func__, retval);
		_cyttsp4_change_state(ts, CY_IDLE_STATE);
#ifdef CY_USE_WATCHDOG
	} else {
		_cyttsp4_start_wd_timer(ts);
#endif
	}

	mutex_unlock(&ts->data_lock);

	return;
}

static int _cyttsp4_enter_sleep(struct ts_data *ts)
{
	int retval = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	uint8_t sleep = CY_DEEP_SLEEP_MODE;

	pr_debug("%s: Put the part back to sleep\n", __func__);

	retval = _cyttsp4_write_block_data(ts, CY_REG_BASE,
		sizeof(sleep), &sleep,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Failed to write sleep bit r=%d\n",
			__func__, retval);
	} else
		_cyttsp4_change_state(ts, CY_SLEEP_STATE);
#endif
	return retval;
}

static int _cyttsp4_wakeup(struct ts_data *ts)
{
	int retval = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	u8 hst_mode = 0;
	u8 rep_stat = 0;
	int wake = CY_WAKE_DFLT;
	unsigned long timeout;
	unsigned long uretval;
	_cyttsp4_change_state(ts, CY_CMD_STATE);
	INIT_COMPLETION(ts->int_running);
	if (ts->private_data->hw_recov == NULL) {
		pr_debug("%s: no hw_recov function\n", __func__);
		retval = -ENOSYS;
	} else {
		/* wake using strobe on host alert pin */
		/* use the wake pulse interrupt to stage resume interrupt */
		_cyttsp4_change_state(ts, CY_WAKE_STATE);
		retval = ts->private_data->hw_recov(wake);
		if (retval < 0) {
			if (retval == -ENOSYS) {
				pr_debug("%s: no hw_recov wake code=%d"
					" function\n", __func__, wake);
			} else {
				pr_err("%s: fail hw_recov(wake=%d)"
					" function r=%d\n",
					__func__, wake, retval);
				retval = -ENOSYS;
			}
		}
	}

	if (retval == -ENOSYS) {
		/*
		 * Wake the chip with bus traffic
		 * The first few reads should always fail because
		 * the part is not ready to respond,
		 * but the retries should succeed.
		 */
		/*
		 * Even though this is hardware-specific, it is done
		 * here because the board config file doesn't have
		 * access to the bus read routine
		 */
		/*
		 * if there was no code to generate a wake pulse,
		 * reset to CMD_STATE and try waking with a read
		 */
		_cyttsp4_change_state(ts, CY_CMD_STATE);
		retval = _cyttsp4_read_block_data(ts, CY_REG_BASE,
			sizeof(hst_mode), &hst_mode,
			ts->private_data->addr[CY_TCH_ADDR_OFS],
			true);
		if (retval < 0) {
			/* device may not be ready even with the
			 * bus read retries so just go ahead and
			 * wait for the cmd rdy interrupt or timeout
			 */
			retval = 0;
		} else {
			/* IC is awake but still need to check for
			 * proper mode
			 */
		}
	} else
		retval = 0;

	/* Wait for cmd rdy interrupt to signal device wake */
	timeout = msecs_to_jiffies(CY_HALF_SEC_TMO_MS);
	mutex_unlock(&ts->data_lock);
	uretval = wait_for_completion_interruptible_timeout(
		&ts->int_running, timeout);
	mutex_lock(&ts->data_lock);

	/* read registers even if wait ended with timeout */
	if (uretval == 0)
		pr_err("%s: wake tmo; continue\n", __func__);
	retval = _cyttsp4_read_block_data(ts,
		CY_REG_BASE, sizeof(hst_mode), &hst_mode,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);

	/* TMA884 indicates bootloader mode by changing addr */
	if (retval < 0) {
		pr_err("%s: failed to resume or in bootloader (r=%d)\n",
			__func__, retval);
	} else {
		/* read rep stat register for bootloader status */
		retval = _cyttsp4_load_status_regs(ts);
		if (retval < 0) {
			pr_err("%s: failed to access device on resume r=%d\n",
				__func__, retval);
			goto _cyttsp4_wakeup_exit;
		}
		rep_stat = ts->xy_mode[ts->si_ofs->rep_ofs + 1];
		if (IS_BOOTLOADERMODE(rep_stat)) {
			pr_err("%s: device in bootloader mode on wakeup"
				" rep_stat=0x%02X\n",
				__func__, rep_stat);
			retval = -EIO;
			goto _cyttsp4_wakeup_exit;
		}
		retval = _cyttsp4_handshake(ts, hst_mode);
		if (retval < 0) {
			pr_err("%s: fail resume INT handshake (r=%d)\n",
				__func__, retval);
			/* continue; rely on handshake tmo */
			retval = 0;
		}
		_cyttsp4_change_state(ts, CY_ACTIVE_STATE);
	}
_cyttsp4_wakeup_exit:
#endif
	return retval;
}

#ifdef CY_AUTO_LOAD_FW
static int _cyttsp4_boot_loader(struct ts_data *ts, bool *upgraded)
{
	int retval = 0;
	int i = 0;
	u32 fw_vers_platform = 0;
	u32 fw_vers_img = 0;
	u32 fw_revctrl_platform_h = 0;
	u32 fw_revctrl_platform_l = 0;
	u32 fw_revctrl_img_h = 0;
	u32 fw_revctrl_img_l = 0;
	bool new_fw_vers = false;
	bool new_fw_revctrl = false;
	bool new_vers = false;

	*upgraded = false;
	if (ts->driver_state == CY_SLEEP_STATE) {
		pr_err("%s: cannot load firmware in sleep state\n",
			__func__);
		retval = 0;
	} else if ((ts->fw->ver == NULL) ||
		(ts->fw->img == NULL)) {
		pr_err("%s: empty version list or no image\n",
			__func__);
		retval = 0;
	} else if (ts->fw->vsize != CY_BL_VERS_SIZE) {
		pr_err("%s: bad fw version list size=%d\n",
			__func__, ts->fw->vsize);
		retval = 0;
	} else {
		/* automatically update firmware if new version detected */
		fw_vers_img = (ts->sysinfo_ptr.cydata->fw_ver_major * 256);
		fw_vers_img += ts->sysinfo_ptr.cydata->fw_ver_minor;
		fw_vers_platform = ts->fw->ver[2] * 256;
		fw_vers_platform += ts->fw->ver[3];
#ifdef CY_ANY_DIFF_NEW_VER
		if (fw_vers_platform != fw_vers_img)
			new_fw_vers = true;
		else
			new_fw_vers = false;
#else
		if (fw_vers_platform > fw_vers_img)
			new_fw_vers = true;
		else
			new_fw_vers = false;
#endif
		pr_debug("%s: fw_vers_platform=%04X fw_vers_img=%04X\n",
			__func__, fw_vers_platform, fw_vers_img);

		fw_revctrl_img_h = ts->sysinfo_ptr.cydata->revctrl[0];
		fw_revctrl_img_l = ts->sysinfo_ptr.cydata->revctrl[4];
		fw_revctrl_platform_h = ts->fw->ver[4];
		fw_revctrl_platform_l = ts->fw->ver[8];
		for (i = 1; i < 4; i++) {
			fw_revctrl_img_h = (fw_revctrl_img_h * 256) +
				ts->sysinfo_ptr.cydata->revctrl[0+i];
			fw_revctrl_img_l = (fw_revctrl_img_l * 256) +
				ts->sysinfo_ptr.cydata->revctrl[4+i];
			fw_revctrl_platform_h = (fw_revctrl_platform_h * 256) +
				ts->fw->ver[4+i];
			fw_revctrl_platform_l = (fw_revctrl_platform_l * 256) +
				ts->fw->ver[8+i];
		}
#ifdef CY_ANY_DIFF_NEW_VER
		if (fw_revctrl_platform_h != fw_revctrl_img_h)
			new_fw_revctrl = true;
		else if (fw_revctrl_platform_h == fw_revctrl_img_h) {
			if (fw_revctrl_platform_l != fw_revctrl_img_l)
				new_fw_revctrl = true;
			else
				new_fw_revctrl = false;
		} else
			new_fw_revctrl = false;
#else
		if (fw_revctrl_platform_h > fw_revctrl_img_h)
			new_fw_revctrl = true;
		else if (fw_revctrl_platform_h == fw_revctrl_img_h) {
			if (fw_revctrl_platform_l > fw_revctrl_img_l)
				new_fw_revctrl = true;
			else
				new_fw_revctrl = false;
		} else
			new_fw_revctrl = false;
#endif
		if (new_fw_vers || new_fw_revctrl)
			new_vers = true;

		pr_debug("%s: fw_revctrl_platform_h=%08X"
			" fw_revctrl_img_h=%08X\n", __func__,
			fw_revctrl_platform_h, fw_revctrl_img_h);
		pr_debug("%s: fw_revctrl_platform_l=%08X"
			" fw_revctrl_img_l=%08X\n", __func__,
			fw_revctrl_platform_l, fw_revctrl_img_l);
		pr_debug("%s: new_fw_vers=%d new_fw_revctrl=%d new_vers=%d\n",
			__func__,
			(int)new_fw_vers, (int)new_fw_revctrl, (int)new_vers);

		if (new_vers) {
			pr_info("%s: upgrading firmware...\n", __func__);
			retval = _cyttsp4_load_app(ts,
				ts->fw->img,
				ts->fw->size);
			if (retval < 0) {
				pr_err("%s: communication fail"
					" on load fw r=%d\n",
					__func__, retval);
				_cyttsp4_change_state(ts, CY_IDLE_STATE);
				retval = -EIO;
			} else
				*upgraded = true;
		} else {
			pr_debug("%s: No auto firmware upgrade required\n",
				__func__);
		}
	}

	return retval;
}
#endif /* --CY_AUTO_LOAD_FW */

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)

static void set_node_data(struct ts_data *ts_data, const u8 data_type,
						int *max_value, int *min_value)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void set_default_result(struct factory_data *data)
{
	char delim = ':';

	memset(data->cmd_result, 0x00, ARRAY_SIZE(data->cmd_result));
	memset(data->cmd_buff, 0x00, ARRAY_SIZE(data->cmd_buff));
	memcpy(data->cmd_result, data->cmd, strlen(data->cmd));
	strncat(data->cmd_result, &delim, 1);
}

static void set_cmd_result(struct factory_data *data, char *buff, int len)
{
	strncat(data->cmd_result, buff, len);
}

static void not_support_cmd(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", "NA");
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));
	data->cmd_state = NOT_APPLICABLE;
	pr_info("tsp factory : %s: \"%s(%d)\"\n", __func__,
			data->cmd_buff,	strlen(data->cmd_buff));
	return;
}

static void fw_update(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void get_fw_ver_bin(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%.4x", FW_VERSION);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	u8 buf[2] = {0, 0};

	data->cmd_state = RUNNING;
	if (ts_data->sysinfo_ptr.ddata != NULL) {
		buf[0] = *((u8 *)ts_data->sysinfo_ptr.ddata+21);
		buf[1] = *((u8 *)ts_data->sysinfo_ptr.ddata+20);
	}

	set_default_result(data);
	sprintf(data->cmd_buff, "%.2x%.2x", buf[1], buf[0]);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_config_ver(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	u8 buf = 0;

	data->cmd_state = RUNNING;
	if (ts_data->sysinfo_ptr.ddata != NULL)
		buf = *((u8 *)ts_data->sysinfo_ptr.ddata+22);
	set_default_result(data);
	sprintf(data->cmd_buff, "%.2x", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_threshold(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;
	enum cyttsp4_ic_ebid ebid = CY_TCH_PARM_EBID;
	u8 *pdata = NULL;
	u8 buf = 0;
	int retval = 0;

	data->cmd_state = RUNNING;

	pdata = kzalloc(ts_data->ebid_row_size, GFP_KERNEL);
	memset(pdata, 0, ts_data->ebid_row_size);
	mutex_lock(&ts_data->data_lock);
	retval = _cyttsp4_set_mode(ts_data, CY_CONFIG_MODE);
	if (retval < 0) {
		pr_err("%s: Fail set config mode 1 r=%d\n", __func__, retval);
		goto cyttsp_threshold_get_fail;
	}

	retval = _cyttsp4_get_ebid_data_tma400(ts_data, ebid, 3, pdata);
	if (retval < 0) {
		pr_err("%s: Fail get threshold value r=%d\n",
			__func__, retval);
		goto cyttsp_threshold_get_fail;
	}

	retval = _cyttsp4_set_mode(ts_data, CY_OPERATE_MODE);
	if (retval < 0) {
		pr_err("%s: Fail set operational mode 1 (r=%d)\n",
			__func__, retval);
		goto cyttsp_threshold_get_fail;
	}

	mutex_unlock(&ts_data->data_lock);

	buf = *(pdata+23);

cyttsp_threshold_get_fail:
	set_default_result(data);

	sprintf(data->cmd_buff, "%.2x", buf);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	if (pdata != NULL)
		kfree(pdata);

	return;
}

static void module_off_master(void *device_data)
{

}

static void module_on_master(void *device_data)
{

}

static void get_chip_vendor(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", TSP_VENDOR);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_chip_name(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	sprintf(data->cmd_buff, "%s", TSP_IC);
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_x_num(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	/* TODO : fix later
	sprintf(data->cmd_buff, "%d", ts_data->platform_data->rx_channel_no);
	 */
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_y_num(void *device_data)
{
	struct ts_data *ts_data = (struct ts_data *)device_data;
	struct factory_data *data = ts_data->factory_data;

	data->cmd_state = RUNNING;

	set_default_result(data);
	/* TODO : fix later
	sprintf(data->cmd_buff, "%d", ts_data->platform_data->tx_channel_no);
	 */
	set_cmd_result(data, data->cmd_buff, strlen(data->cmd_buff));

	data->cmd_state = OK;
	return;
}

static void get_reference(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void get_cm_abs(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void get_cm_delta(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void get_intensity(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void run_reference_read(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void run_cm_abs_read(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void run_cm_delta_read(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

static void run_intensity_read(void *device_data)
{
	/* TODO : fix later
	 * Add factory func. for cypress GEN4
	 */
}

struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("get_cm_delta", get_cm_delta),},
	{TSP_CMD("get_intensity", get_intensity),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t cmd_store(struct device *dev, struct device_attribute *devattr,
						const char *buf, size_t count)
{
	struct ts_data *ts_data = dev_get_drvdata(dev);
	struct factory_data *data = ts_data->factory_data;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0, };
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (data == NULL) {
		pr_err("factory_data is NULL.\n");
		goto err_out;
	}

	if (data->cmd_is_running == true) {
		pr_err("tsp cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = true;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = RUNNING;

	for (i = 0; i < ARRAY_SIZE(data->cmd_param); i++)
		data->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(data->cmd, 0x00, ARRAY_SIZE(data->cmd));
	memcpy(data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &data->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &data->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		do {
			memset(buff, 0x00, ARRAY_SIZE(buff));
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				if (kstrtoint(buff, 10,
					data->cmd_param + param_cnt) < 0)
					break;
				start = cur + 1;
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	pr_info("cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		pr_info("cmd param %d= %d\n", i, data->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(ts_data);

err_out:
	return count;
}

static ssize_t cmd_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ts_data *ts_data = dev_get_drvdata(dev);
	struct factory_data *data = ts_data->factory_data;
	char buff[16];

	pr_info("tsp cmd: status:%d\n", data->cmd_state);

	switch (data->cmd_state) {
	case WAITING:
		sprintf(buff, "%s", TOSTRING(WAITING));
		break;
	case RUNNING:
		sprintf(buff, "%s", TOSTRING(RUNNING));
		break;
	case OK:
		sprintf(buff, "%s", TOSTRING(OK));
		break;
	case FAIL:
		sprintf(buff, "%s", TOSTRING(FAIL));
		break;
	case NOT_APPLICABLE:
		sprintf(buff, "%s", TOSTRING(NOT_APPLICABLE));
		break;
	default:
		sprintf(buff, "%s", TOSTRING(NOT_APPLICABLE));
		break;
	}

	return sprintf(buf, "%s\n", buff);
}

static ssize_t cmd_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ts_data *ts_data = dev_get_drvdata(dev);
	struct factory_data *data = ts_data->factory_data;

	pr_info("tsp factory : tsp cmd: result: \"%s(%d)\"\n",
				data->cmd_result, strlen(data->cmd_result));

	mutex_lock(&data->cmd_lock);
	data->cmd_is_running = false;
	mutex_unlock(&data->cmd_lock);

	data->cmd_state = WAITING;

	return sprintf(buf, "%s\n", data->cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, cmd_store);
static DEVICE_ATTR(cmd_status, S_IRUGO, cmd_status_show, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, cmd_result_show, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};
#endif

static ssize_t touchkey_led_control(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t size)
{
	struct ts_data *ts = dev_get_drvdata(dev);
	int ret;
	int data;
	sscanf(buf, "%d\n", &data);
	pr_info("[TSP] %s : LED buf is %d\n", __func__, data);
	if (data == 1) {
		ret = ts->private_data->led_power(true);
		if (ret) {
			pr_err("tsp: Fail to enable led\n");
			return size;
		}
	} else if (data == 2) {
		ret = ts->private_data->led_power(false);
		if (ret) {
			pr_err("tsp: Fail to disable led\n");
			return size;
		}
	}

	return size;
}
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		touchkey_led_control);

#ifdef CONFIG_TOUCH_DVFS
static void disable_dvfs(struct work_struct *unused)
{
	omap_cpufreq_min_limit_free(DVFS_LOCK_ID_TSP);
	boost = false;
	return;
}
static DECLARE_WORK(tsp_wq, disable_dvfs);

static void timer_cb(unsigned long data)
{
	schedule_work(&tsp_wq);
	return;
}
#endif

#define CY_CMD_I2C_ADDR					0
#define CY_STATUS_SIZE_BYTE				1
#define CY_STATUS_TYP_DELAY				2
#define CY_CMD_TAIL_LEN					3
#define CY_CMD_BYTE					1
#define CY_STATUS_BYTE					1
#define CY_MAX_STATUS_SIZE				32
#define CY_MIN_STATUS_SIZE				5
#define CY_START_OF_PACKET				0x01
#define CY_END_OF_PACKET				0x17
#define CY_DATA_ROW_SIZE				288
#define CY_DATA_ROW_SIZE_TMA400				128
#define CY_PACKET_DATA_LEN				96
#define CY_MAX_PACKET_LEN				512
#define CY_COMM_BUSY					0xFF
#define CY_CMD_BUSY					0xFE
#define CY_SEPARATOR_OFFSET				0
#define CY_ARRAY_ID_OFFSET				0
#define CY_ROW_NUM_OFFSET				1
#define CY_ROW_SIZE_OFFSET				3
#define CY_ROW_DATA_OFFSET				5
#define CY_FILE_SILICON_ID_OFFSET			0
#define CY_FILE_REV_ID_OFFSET				4
#define CY_CMD_LDR_HOST_SYNC				0xFF /* tma400 */
#define CY_CMD_LDR_EXIT					0x3B
#define CY_CMD_LDR_EXIT_CMD_SIZE			7
#define CY_CMD_LDR_EXIT_STAT_SIZE			7

enum ldr_status {
	ERROR_SUCCESS = 0,
	ERROR_COMMAND = 1,
	ERROR_FLASH_ARRAY = 2,
	ERROR_PACKET_DATA = 3,
	ERROR_PACKET_LEN = 4,
	ERROR_PACKET_CHECKSUM = 5,
	ERROR_FLASH_PROTECTION = 6,
	ERROR_FLASH_CHECKSUM = 7,
	ERROR_VERIFY_IMAGE = 8,
	ERROR_UKNOWN1 = 9,
	ERROR_UKNOWN2 = 10,
	ERROR_UKNOWN3 = 11,
	ERROR_UKNOWN4 = 12,
	ERROR_UKNOWN5 = 13,
	ERROR_UKNOWN6 = 14,
	ERROR_INVALID_COMMAND = 15,
	ERROR_INVALID
};

static u16 _cyttsp4_compute_crc(struct ts_data *ts, u8 *buf, int size)
{
	u16 crc = 0xffff;
	u16 tmp;
	int i;

	/* RUN CRC */

	if (size == 0)
		crc = ~crc;
	else {

		do {
			for (i = 0, tmp = 0x00ff & *buf++; i < 8;
				i++, tmp >>= 1) {
				if ((crc & 0x0001) ^ (tmp & 0x0001))
					crc = (crc >> 1) ^ 0x8408;
				else
					crc >>= 1;
			}
		} while (--size);

		crc = ~crc;
		tmp = crc;
		crc = (crc << 8) | (tmp >> 8 & 0xFF);
	}

	return crc;
}

static int _cyttsp4_get_status(struct ts_data *ts,
	u8 *buf, int size, unsigned long timeout_ms)
{
	unsigned long uretval = 0;
	int tries = 0;
	int retval = 0;

	if (timeout_ms != 0) {
		/* wait until status ready interrupt or timeout occurs */
		uretval = wait_for_completion_interruptible_timeout(
			&ts->int_running, msecs_to_jiffies(timeout_ms));

		/* read the status packet */
		if (buf == NULL) {
			pr_err("%s: Status buf ptr is NULL\n", __func__);
			retval = -EINVAL;
			goto _cyttsp4_get_status_exit;
		}
		for (tries = 0; tries < 2; tries++) {
			retval = _cyttsp4_read_block_data(ts, CY_REG_BASE, size,
				buf, ts->private_data->addr[CY_LDR_ADDR_OFS],
		true);
			/*
			 * retry if bus read error or
			 * status byte shows not ready
			 */
			if ((buf[1] == CY_COMM_BUSY) || (buf[1] == CY_CMD_BUSY))
				msleep(CY_DELAY_DFLT);
			else
				break;
		}
		pr_debug("%s: tries=%d ret=%d status=%02X\n",
			__func__, tries, retval, buf[1]);
	}

_cyttsp4_get_status_exit:
	mutex_lock(&ts->data_lock);
	return retval;
}

/*
 * Send a bootloader command to the device;
 * Wait for the ISR to execute indicating command
 * was received and status is ready;
 * Releases data_lock mutex to allow ISR to run,
 * then locks it again.
 */
static int _cyttsp4_send_cmd(struct ts_data *ts, const u8 *cmd_buf,
	int cmd_size, u8 *stat_ret, size_t num_stat_byte,
	size_t status_size, unsigned long timeout_ms)
{
	u8 *status_buf = NULL;
	int retval = 0;

	if (timeout_ms > 0) {
		status_buf = kzalloc(CY_MAX_STATUS_SIZE, GFP_KERNEL);
		if (status_buf == NULL) {
			pr_err("%s: Fail alloc status buffer=%p\n",
				__func__, status_buf);
			goto _cyttsp4_send_cmd_exit;
		}
	}

	if (cmd_buf == NULL) {
		pr_err("%s: bad cmd_buf=%p\n", __func__, cmd_buf);
		goto _cyttsp4_send_cmd_exit;
	}

	if (cmd_size == 0) {
		pr_err("%s: bad cmd_size=%d\n", __func__, cmd_size);
		goto _cyttsp4_send_cmd_exit;
	}

	_cyttsp4_pr_buf(ts, (u8 *)cmd_buf, cmd_size, "send_cmd");

	mutex_unlock(&ts->data_lock);
	if (timeout_ms > 0)
		INIT_COMPLETION(ts->int_running);
	retval = _cyttsp4_write_block_data(ts, CY_REG_BASE, cmd_size, cmd_buf,
		ts->private_data->addr[CY_LDR_ADDR_OFS],
		true);
	if (retval < 0) {
		pr_err("%s: Fail writing command=%02X\n",
			__func__, cmd_buf[CY_CMD_BYTE]);
		mutex_lock(&ts->data_lock);
		goto _cyttsp4_send_cmd_exit;
	}

	/* get the status and lock the mutex */
	if (timeout_ms > 0) {
		retval = _cyttsp4_get_status(ts, status_buf,
			status_size, timeout_ms);
		if ((retval < 0) || (status_buf[0] != CY_START_OF_PACKET)) {
			pr_err("%s: Error getting status r=%d"
				" status_buf[0]=%02X\n",
				__func__, retval, status_buf[0]);
			if (!(retval < 0))
				retval = -EIO;
			goto _cyttsp4_send_cmd_exit;
		} else {
			if (status_buf[CY_STATUS_BYTE] != ERROR_SUCCESS) {
				pr_err("%s: Status=0x%02X error\n",
					__func__, status_buf[CY_STATUS_BYTE]);
				retval = -EIO;
			} else if (stat_ret != NULL) {
				if (num_stat_byte < status_size)
					*stat_ret = status_buf[num_stat_byte];
				else
					*stat_ret = 0;
			}
		}
	} else {
		if (stat_ret != NULL)
			*stat_ret = ERROR_SUCCESS;
		mutex_lock(&ts->data_lock);
	}

_cyttsp4_send_cmd_exit:
	if (status_buf != NULL)
		kfree(status_buf);
	return retval;
}

struct cyttsp4_dev_id {
	u32 silicon_id;
	u8 rev_id;
	u32 bl_ver;
};

#if defined(CY_AUTO_LOAD_FW) || \
	defined(CY_USE_FORCE_LOAD) || \
	defined(CONFIG_TOUCHSCREEN_DEBUG)
#define CY_CMD_LDR_ENTER				0x38
#define CY_CMD_LDR_ENTER_CMD_SIZE			7
#define CY_CMD_LDR_ENTER_STAT_SIZE			15
#define CY_CMD_LDR_INIT					0x48
#define CY_CMD_LDR_INIT_CMD_SIZE			15
#define CY_CMD_LDR_INIT_STAT_SIZE			7
#define CY_CMD_LDR_ERASE_ROW				0x34
#define CY_CMD_LDR_ERASE_ROW_CMD_SIZE			10
#define CY_CMD_LDR_ERASE_ROW_STAT_SIZE			7
#define CY_CMD_LDR_SEND_DATA				0x37
#define CY_CMD_LDR_SEND_DATA_CMD_SIZE			4 /* hdr bytes only */
#define CY_CMD_LDR_SEND_DATA_STAT_SIZE			8
#define CY_CMD_LDR_PROG_ROW				0x39
#define CY_CMD_LDR_PROG_ROW_CMD_SIZE			7 /* hdr bytes only */
#define CY_CMD_LDR_PROG_ROW_STAT_SIZE			7
#define CY_CMD_LDR_VERIFY_ROW				0x3A
#define CY_CMD_LDR_VERIFY_ROW_STAT_SIZE			8
#define CY_CMD_LDR_VERIFY_ROW_CMD_SIZE			10
#define CY_CMD_LDR_VERIFY_CHKSUM			0x31
#define CY_CMD_LDR_VERIFY_CHKSUM_CMD_SIZE		7
#define CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE		8

static u16 _cyttsp4_get_short(u8 *buf)
{
	return ((u16)(*buf) << 8) + *(buf+1);
}

static u8 *_cyttsp4_get_row(struct ts_data *ts,
	u8 *row_buf, u8 *image_buf, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		/* copy a row from the image */
		row_buf[i] = image_buf[i];
	}

	image_buf = image_buf + size;
	return image_buf;
}

static int _cyttsp4_ldr_enter(struct ts_data *ts, struct cyttsp4_dev_id *dev_id)
{
	u16 crc;
	int i = 0;
	size_t cmd_size;
	u8 status_buf[CY_MAX_STATUS_SIZE];
	u8 status = 0;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_enter_cmd[CY_CMD_LDR_ENTER_CMD_SIZE+1];

	memset(status_buf, 0, sizeof(status_buf));
	dev_id->bl_ver = 0;
	dev_id->rev_id = 0;
	dev_id->silicon_id = 0;

	ldr_enter_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_enter_cmd[i++] = CY_START_OF_PACKET;
	ldr_enter_cmd[i++] = CY_CMD_LDR_ENTER;
	ldr_enter_cmd[i++] = 0x00;	/* data len lsb */
	ldr_enter_cmd[i++] = 0x00;	/* data len msb */
	crc = _cyttsp4_compute_crc(ts, &ldr_enter_cmd[1], i - 1);
	cmd_size = sizeof(ldr_enter_cmd);
	ldr_enter_cmd[i++] = (u8)crc;
	ldr_enter_cmd[i++] = (u8)(crc >> 8);
	ldr_enter_cmd[i++] = CY_END_OF_PACKET;

	mutex_unlock(&ts->data_lock);
	INIT_COMPLETION(ts->int_running);
	retval = _cyttsp4_write_block_data(ts, CY_REG_BASE, cmd_size,
		ldr_enter_cmd, ts->private_data->addr[CY_LDR_ADDR_OFS],
		true);
	if (retval < 0) {
		pr_err("%s: write block failed %d\n", __func__, retval);
		goto _cyttsp4_ldr_enter_exit;
	}

	/* Wait for ISR, get status and lock mutex */
	retval = _cyttsp4_get_status(ts, status_buf,
		CY_CMD_LDR_ENTER_STAT_SIZE, CY_HALF_SEC_TMO_MS);

	if (retval < 0) {
		pr_err("%s: Fail get status to Enter Loader command r=%d\n",
			__func__, retval);
	} else {
		status = status_buf[CY_STATUS_BYTE];
		if (status == ERROR_SUCCESS) {
			dev_id->bl_ver =
				status_buf[11] << 16 |
				status_buf[10] <<  8 |
				status_buf[9] <<  0;
			dev_id->rev_id =
				status_buf[8] <<  0;
			dev_id->silicon_id =
				status_buf[7] << 24 |
				status_buf[6] << 16 |
				status_buf[5] <<  8 |
				status_buf[4] <<  0;
			retval = 0;
		} else
			retval = -EIO;
		pr_debug("%s: status=%d "
			"bl_ver=%08X rev_id=%02X silicon_id=%08X\n",
			__func__, status,
			dev_id->bl_ver, dev_id->rev_id, dev_id->silicon_id);
	}

_cyttsp4_ldr_enter_exit:
	return retval;
}

static int _cyttsp4_ldr_init(struct ts_data *ts)
{
	u16 crc;
	int i = 0;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_init_cmd[CY_CMD_LDR_INIT_CMD_SIZE+1];

	ldr_init_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_init_cmd[i++] = CY_START_OF_PACKET;
	ldr_init_cmd[i++] = CY_CMD_LDR_INIT;
	ldr_init_cmd[i++] = 0x08;	/* data len lsb */
	ldr_init_cmd[i++] = 0x00;	/* data len msb */
	memcpy(&ldr_init_cmd[i], cyttsp4_security_key,
		sizeof(cyttsp4_security_key));
	i += sizeof(cyttsp4_security_key);
	crc = _cyttsp4_compute_crc(ts, &ldr_init_cmd[1], i - 1);
	ldr_init_cmd[i++] = (u8)crc;
	ldr_init_cmd[i++] = (u8)(crc >> 8);
	ldr_init_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ts, ldr_init_cmd, i, NULL, 0,
		CY_CMD_LDR_INIT_STAT_SIZE, CY_TEN_SEC_TMO_MS);
	if (retval < 0) {
		pr_err("%s: Fail ldr init r=%d\n",
			__func__, retval);
	}

	return retval;
}

struct cyttsp4_hex_image {
	u8 array_id;
	u16 row_num;
	u16 row_size;
	u8 row_data[CY_DATA_ROW_SIZE];
} __packed;

static int _cyttsp4_ldr_parse_row(struct ts_data *ts, u8 *row_buf,
	struct cyttsp4_hex_image *row_image)
{
	u16 i, j;
	int retval = 0;

	if (!row_buf) {
		pr_err("%s parse row error - buf is null\n", __func__);
		retval = -EINVAL;
		goto cyttsp4_ldr_parse_row_exit;
	}

	row_image->array_id = row_buf[CY_ARRAY_ID_OFFSET];
	row_image->row_num = _cyttsp4_get_short(&row_buf[CY_ROW_NUM_OFFSET]);
	row_image->row_size = _cyttsp4_get_short(&row_buf[CY_ROW_SIZE_OFFSET]);

	if (row_image->row_size > ARRAY_SIZE(row_image->row_data)) {
		pr_err("%s: row data buffer overflow\n", __func__);
		retval = -EOVERFLOW;
		goto cyttsp4_ldr_parse_row_exit;
	}

	for (i = 0, j = CY_ROW_DATA_OFFSET;
		i < row_image->row_size; i++)
		row_image->row_data[i] = row_buf[j++];

	retval = 0;

cyttsp4_ldr_parse_row_exit:
	return retval;
}

static int _cyttsp4_ldr_prog_row(struct ts_data *ts,
	struct cyttsp4_hex_image *row_image)
{
	u16 crc;
	int next;
	int data;
	int row_data;
	u16 row_sum = 0;
	size_t data_len;
	int retval = 0;

	u8 *cmd = kzalloc(CY_MAX_PACKET_LEN, GFP_KERNEL);

	if (cmd != NULL) {
		row_data = 0;
		row_sum = 0;

		next = 0;
		cmd[next++] = CY_CMD_LDR_HOST_SYNC;
		cmd[next++] = CY_START_OF_PACKET;
		cmd[next++] = CY_CMD_LDR_PROG_ROW;
		/*
		 * include array id size and row id size in CY_PACKET_DATA_LEN
		 */
		data_len = CY_DATA_ROW_SIZE_TMA400;
		cmd[next++] = (u8)(data_len+3);
		cmd[next++] = (u8)((data_len+3) >> 8);
		cmd[next++] = row_image->array_id;
		cmd[next++] = (u8)row_image->row_num;
		cmd[next++] = (u8)(row_image->row_num >> 8);

		for (data = 0;
			data < data_len; data++) {
			cmd[next] = row_image->row_data[row_data++];
			row_sum += cmd[next];
			next++;
		}

		crc = _cyttsp4_compute_crc(ts, &cmd[1], next - 1);
		cmd[next++] = (u8)crc;
		cmd[next++] = (u8)(crc >> 8);
		cmd[next++] = CY_END_OF_PACKET;

		retval = _cyttsp4_send_cmd(ts, cmd, next, NULL, 0,
			CY_CMD_LDR_PROG_ROW_STAT_SIZE, CY_HALF_SEC_TMO_MS);

		if (retval < 0) {
			pr_err("%s: prog row=%d fail r=%d\n",
				__func__, row_image->row_num, retval);
			goto cyttsp4_ldr_prog_row_exit;
		}

	} else {
		pr_err("%s prog row error - cmd buf is NULL\n", __func__);
		retval = -EIO;
	}

cyttsp4_ldr_prog_row_exit:
	if (cmd != NULL)
		kfree(cmd);
	return retval;
}

static int _cyttsp4_ldr_verify_row(struct ts_data *ts,
	struct cyttsp4_hex_image *row_image)
{
	u16 crc;
	int i = 0;
	u8 verify_checksum;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_verify_row_cmd[CY_CMD_LDR_VERIFY_ROW_CMD_SIZE+1];

	ldr_verify_row_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_verify_row_cmd[i++] = CY_START_OF_PACKET;
	ldr_verify_row_cmd[i++] = CY_CMD_LDR_VERIFY_ROW;
	ldr_verify_row_cmd[i++] = 0x03;	/* data len lsb */
	ldr_verify_row_cmd[i++] = 0x00;	/* data len msb */
	ldr_verify_row_cmd[i++] = row_image->array_id;
	ldr_verify_row_cmd[i++] = (u8)row_image->row_num;
	ldr_verify_row_cmd[i++] = (u8)(row_image->row_num >> 8);
	crc = _cyttsp4_compute_crc(ts, &ldr_verify_row_cmd[1], i - 1);
	ldr_verify_row_cmd[i++] = (u8)crc;
	ldr_verify_row_cmd[i++] = (u8)(crc >> 8);
	ldr_verify_row_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ts, ldr_verify_row_cmd, i,
		&verify_checksum, 4,
		CY_CMD_LDR_VERIFY_ROW_STAT_SIZE, CY_HALF_SEC_TMO_MS);

	if (retval < 0) {
		pr_err("%s: verify row=%d fail r=%d\n",
			__func__, row_image->row_num, retval);
	}

	return retval;
}

static int _cyttsp4_ldr_verify_chksum(struct ts_data *ts, u8 *app_chksum)
{
	u16 crc;
	int i = 0;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_verify_chksum_cmd[CY_CMD_LDR_VERIFY_CHKSUM_CMD_SIZE+1];

	ldr_verify_chksum_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_verify_chksum_cmd[i++] = CY_START_OF_PACKET;
	ldr_verify_chksum_cmd[i++] = CY_CMD_LDR_VERIFY_CHKSUM;
	ldr_verify_chksum_cmd[i++] = 0x00;	/* data len lsb */
	ldr_verify_chksum_cmd[i++] = 0x00;	/* data len msb */
	crc = _cyttsp4_compute_crc(ts, &ldr_verify_chksum_cmd[1], i - 1);
	ldr_verify_chksum_cmd[i++] = (u8)crc;
	ldr_verify_chksum_cmd[i++] = (u8)(crc >> 8);
	ldr_verify_chksum_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ts, ldr_verify_chksum_cmd, i,
		app_chksum, 4,
		CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE, CY_HALF_SEC_TMO_MS);

	if (retval < 0) {
		pr_err("%s: verify checksum fail r=%d\n",
			__func__, retval);
	}

	return retval;
}

static int _cyttsp4_load_app(struct ts_data *ts, const u8 *fw, int fw_size)
{
	u8 *p;
	int ret;
	int retval;	/* need separate return value at exit stage */
	struct cyttsp4_dev_id *file_id = NULL;
	struct cyttsp4_dev_id *dev_id = NULL;
	struct cyttsp4_hex_image *row_image = NULL;
	u8 app_chksum;

	u8 *row_buf = NULL;
	size_t image_rec_size;
	size_t row_buf_size = 1024 > CY_MAX_PRBUF_SIZE ?
		1024 : CY_MAX_PRBUF_SIZE;
	int row_count = 0;

	image_rec_size = CY_DATA_ROW_SIZE_TMA400 +
		(sizeof(struct cyttsp4_hex_image) - CY_DATA_ROW_SIZE);

	if (!fw_size || (fw_size % image_rec_size != 0)) {
		pr_err("%s: Firmware image is misaligned\n", __func__);
		retval = -EINVAL;
		goto _cyttsp4_load_app_exit;
	}

#ifdef CY_USE_WATCHDOG
	_cyttsp4_stop_wd_timer(ts);
#endif

	pr_info("%s: start load app\n", __func__);

	row_buf = kzalloc(row_buf_size, GFP_KERNEL);
	row_image = kzalloc(sizeof(struct cyttsp4_hex_image), GFP_KERNEL);
	file_id = kzalloc(sizeof(struct cyttsp4_dev_id), GFP_KERNEL);
	dev_id = kzalloc(sizeof(struct cyttsp4_dev_id), GFP_KERNEL);
	if ((row_buf == NULL) || (row_image == NULL) ||
		(file_id == NULL) || (dev_id == NULL)) {
		pr_err("%s: Unable to alloc row buffers(%p %p %p %p)\n",
			__func__, row_buf, row_image, file_id, dev_id);
		retval = -ENOMEM;
		goto _cyttsp4_load_app_error_exit;
	}

	p = (u8 *)fw;
	/* Enter Loader and return Silicon ID and Rev */

	retval = _cyttsp4_reset(ts);
	if (retval < 0) {
		pr_err("%s: Fail reset device r=%d\n", __func__, retval);
		goto _cyttsp4_load_app_exit;
	}
	retval = _cyttsp4_wait_int(ts, CY_TEN_SEC_TMO_MS);
	if (retval < 0) {
		pr_err("%s: Fail waiting for bootloader interrupt\n",
			__func__);
		goto _cyttsp4_load_app_exit;
	}

	_cyttsp4_change_state(ts, CY_BL_STATE);
	pr_info("%s: Send BL Loader Enter\n", __func__);
	retval = _cyttsp4_ldr_enter(ts, dev_id);
	if (retval < 0) {
		pr_err("%s: Error cannot start Loader (ret=%d)\n",
			__func__, retval);
		goto _cyttsp4_load_app_error_exit;
	}

	pr_debug("%s: dev: silicon id=%08X rev=%02X bl=%08X\n",
		__func__, dev_id->silicon_id,
		dev_id->rev_id, dev_id->bl_ver);

	udelay(1000);
	retval = _cyttsp4_ldr_init(ts);
	if (retval < 0) {
		pr_err("%s: Error cannot init Loader (ret=%d)\n",
			__func__, retval);
		goto _cyttsp4_load_app_error_exit;
	}

	pr_info("%s: Send BL Loader Blocks\n", __func__);
	while (p < (fw + fw_size)) {
		/* Get row */
		pr_debug("%s: read row=%d\n", __func__, ++row_count);
		memset(row_buf, 0, row_buf_size);
		p = _cyttsp4_get_row(ts, row_buf, p, image_rec_size);

		/* Parse row */
		pr_debug("%s: p=%p buf=%p buf[0]=%02X\n", __func__,
			p, row_buf, row_buf[0]);
		retval = _cyttsp4_ldr_parse_row(ts, row_buf, row_image);
		pr_debug("%s: array_id=%02X row_num=%04X(%d)"
				" row_size=%04X(%d)\n", __func__,
			row_image->array_id,
			row_image->row_num, row_image->row_num,
			row_image->row_size, row_image->row_size);
		if (retval < 0) {
			pr_err("%s: Parse Row Error "
				"(a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num,
				retval);
			goto bl_exit;
		} else {
			pr_debug("%s: Parse Row "
				"(a=%d r=%d ret=%d\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
		}

		/* program row */
		retval = _cyttsp4_ldr_prog_row(ts, row_image);
		if (retval < 0) {
			pr_err("%s: Program Row Error "
				"(array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
			goto _cyttsp4_load_app_error_exit;
		}

		/* verify row */
		retval = _cyttsp4_ldr_verify_row(ts, row_image);
		if (retval < 0) {
			pr_err("%s: Verify Row Error "
				"(array=%d row=%d ret=%d)\n",
				__func__, row_image->array_id,
				row_image->row_num, retval);
			goto _cyttsp4_load_app_error_exit;
		}

		pr_debug("%s: array=%d row_cnt=%d row_num=%04X\n",
			__func__, row_image->array_id, row_count,
			row_image->row_num);
	}

	/* verify app checksum */
	retval = _cyttsp4_ldr_verify_chksum(ts, &app_chksum);
	pr_debug("%s: Application Checksum = %02X r=%d\n",
		__func__, app_chksum, retval);
	if (retval < 0) {
		pr_err("%s: ldr_verify_chksum fail r=%d\n", __func__, retval);
		retval = 0;
	}

	/* exit loader */
bl_exit:
	pr_info("%s: Send BL Loader Terminate\n", __func__);
	ret = _cyttsp4_ldr_exit(ts);
	if (ret) {
		pr_err("%s: Error on exit Loader (ret=%d)\n",
			__func__, ret);
		retval = ret;
		goto _cyttsp4_load_app_error_exit;
	}

	/*
	 * this is a temporary parking state;
	 * the driver will always run startup
	 * after the loader has completed
	 */
	_cyttsp4_change_state(ts, CY_TRANSFER_STATE);
	goto _cyttsp4_load_app_exit;

_cyttsp4_load_app_error_exit:
	_cyttsp4_change_state(ts, CY_BL_STATE);
_cyttsp4_load_app_exit:
	kfree(row_buf);
	kfree(row_image);
	kfree(file_id);
	kfree(dev_id);
	return retval;
}
#endif /* CY_AUTO_LOAD_FW || CY_USE_FORCE_LOAD || CONFIG_TOUCHSCREEN_DEBUG */

/* Constructs loader exit command and sends via _cyttsp4_send_cmd() */
static int _cyttsp4_ldr_exit(struct ts_data *ts)
{
	u16 crc;
	int i = 0;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_exit_cmd[CY_CMD_LDR_EXIT_CMD_SIZE+1];

	ldr_exit_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_exit_cmd[i++] = CY_START_OF_PACKET;
	ldr_exit_cmd[i++] = CY_CMD_LDR_EXIT;
	ldr_exit_cmd[i++] = 0x00;	/* data len lsb */
	ldr_exit_cmd[i++] = 0x00;	/* data len msb */
	crc = _cyttsp4_compute_crc(ts, &ldr_exit_cmd[1], i - 1);
	ldr_exit_cmd[i++] = (u8)crc;
	ldr_exit_cmd[i++] = (u8)(crc >> 8);
	ldr_exit_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ts, ldr_exit_cmd, i, NULL, 0,
		CY_CMD_LDR_EXIT_STAT_SIZE, 0);

	if (retval < 0) {
		pr_err("%s: BL Loader exit fail r=%d\n",
			__func__, retval);
	}

	pr_debug("%s: Exit BL Loader r=%d\n", __func__, retval);

	return retval;
}

#if defined(CY_USE_FORCE_LOAD) || defined(CONFIG_TOUCHSCREEN_DEBUG)
/* Force firmware upgrade */
static void cyttsp4_firmware_cont(const struct firmware *fw, void *context)
{
	int retval = 0;
	struct device *dev = context;
	struct ts_data *ts = dev_get_drvdata(dev);
	u8 header_size = 0;

	mutex_lock(&ts->data_lock);

	if (fw == NULL) {
		pr_err("%s: Firmware not found\n", __func__);
		goto cyttsp4_firmware_cont_exit;
	}

	if ((fw->data == NULL) || (fw->size == 0)) {
		pr_err("%s: No firmware received\n", __func__);
		goto cyttsp4_firmware_cont_release_exit;
	}

	header_size = fw->data[0];
	if (header_size >= (fw->size + 1)) {
		pr_err("%s: Firmware format is invalid\n", __func__);
		goto cyttsp4_firmware_cont_release_exit;
	}
	retval = _cyttsp4_load_app(ts, &(fw->data[header_size + 1]),
		fw->size - (header_size + 1));
	if (retval < 0) {
		pr_err("%s: Firmware update failed with error code %d\n",
			__func__, retval);
		_cyttsp4_change_state(ts, CY_IDLE_STATE);
		retval = -EIO;
		goto cyttsp4_firmware_cont_release_exit;
	}

	retval = _cyttsp4_startup(ts);
	if (retval < 0) {
		pr_err("%s: Failed to restart IC with error code %d\n",
			__func__, retval);
		_cyttsp4_change_state(ts, CY_IDLE_STATE);
	}

cyttsp4_firmware_cont_release_exit:
	release_firmware(fw);

cyttsp4_firmware_cont_exit:
	ts->waiting_for_fw = false;
	mutex_unlock(&ts->data_lock);
	return;
}
#endif /* CY_USE_FORCE_LOAD || CONFIG_TOUCHSCREEN_DEBUG */

/* Get IC CRC is operational mode command */
static int _cyttsp4_get_ic_crc(struct ts_data *ts,
	enum cyttsp4_ic_ebid ebid, u8 *crc_h, u8 *crc_l)
{
	int retval = 0;
	u8 cmd_dat[CY_NUM_DAT + 1];	/* +1 for cmd byte */

	memset(cmd_dat, 0, sizeof(cmd_dat));
	cmd_dat[0] = CY_GET_CFG_BLK_CRC;/* pack cmd */
	cmd_dat[1] = ebid;		/* pack EBID id */

	retval = _cyttsp4_put_cmd_wait(ts, ts->si_ofs->cmd_ofs,
		sizeof(cmd_dat), cmd_dat, CY_HALF_SEC_TMO_MS,
		_cyttsp4_chk_cmd_rdy, NULL,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail Get CRC command r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_ic_crc_exit;
	}

	memset(cmd_dat, 0, sizeof(cmd_dat));
	retval = _cyttsp4_read_block_data(ts, ts->si_ofs->cmd_ofs,
		sizeof(cmd_dat), cmd_dat,
		ts->private_data->addr[CY_TCH_ADDR_OFS], true);
	if (retval < 0) {
		pr_err("%s: Fail Get CRC status r=%d\n",
			__func__, retval);
		goto _cyttsp4_get_ic_crc_exit;
	}

	/* Check CRC status and assign values */
	if (cmd_dat[1] != 0) {
		pr_err("%s: Get CRC status=%d error\n",
			__func__, cmd_dat[1]);
		retval = -EIO;
		goto _cyttsp4_get_ic_crc_exit;
	}

	*crc_h = cmd_dat[2];
	*crc_l = cmd_dat[3];

	retval = _cyttsp4_cmd_handshake(ts);
	if (retval < 0) {
		pr_err("%s: Command handshake error r=%d\n",
			__func__, retval);
		/* continue anyway; rely on handshake tmo */
		retval = 0;
	}

_cyttsp4_get_ic_crc_exit:
	return retval;
}

static int _cyttsp4_startup(struct ts_data *ts)
{
	int tries;
	int retval = 0;
	u8 ic_crc[2];
#ifdef CY_AUTO_LOAD_TOUCH_PARAMS
	u8 table_crc[2];
#endif /* --CY_AUTO_LOAD_TOUCH_PARAMS */
	bool put_all_params_done = false;
	bool upgraded = false;
	bool mddata_updated = false;

	tries = 0;
	ts->starting_up = true;
	memset(&ts->test, 0, sizeof(struct cyttsp4_test_mode));

_cyttsp4_startup_tma400_restart:

	pr_err("%s: enter driver_state=%d\n", __func__, ts->driver_state);
	ts->current_mode = CY_MODE_BOOTLOADER;
	retval = _cyttsp4_reset(ts);
	if (retval < 0) {
		pr_err("%s: Fail reset device r=%d\n", __func__, retval);
		/* continue anyway in case device was already in bootloader */
	}
	/*
	 * Wait for heartbeat interrupt. If we didn't get the CPU quickly, this
	 * may not be the first interupt.
	 */
	pr_debug("%s: wait for first bootloader interrupt\n", __func__);
	retval = _cyttsp4_wait_int(ts, CY_HALF_SEC_TMO_MS);
	if (retval < 0) {
		pr_err("%s: Fail waiting for bootloader interrupt\n",
			__func__);
		goto _cyttsp4_startup_tma400_exit;
	}

	/*
	 * exit BL mode and eliminate race between heartbeat and
	 * command / response interrupts
	 */
	_cyttsp4_change_state(ts, CY_EXIT_BL_STATE);
	ts->switch_flag = true;
	retval = _cyttsp4_wait_si_int(ts, CY_TEN_SEC_TMO_MS);
	if (retval < 0) {
		pr_err("%s: Fail wait switch to Sysinfo r=%d\n",
			__func__, retval);
		/* continue anyway in case sync missed */
	}
	if (ts->driver_state != CY_SYSINFO_STATE) {
		pr_err("%s: Fail set sysinfo mode; switch to sysinfo anyway\r",
			__func__);
		_cyttsp4_change_state(ts, CY_SYSINFO_STATE);
	} else {
		pr_debug("%s: Exit BL ok; now in sysinfo mode\n", __func__);
		_cyttsp4_pr_state(ts);
	}

	pr_debug("%s: Read Sysinfo regs and get rev numbers try=%d\n",
		__func__, tries);
	retval = _cyttsp4_get_sysinfo_regs(ts);
	if (retval < 0) {
		pr_err("%s: Read Block fail -get sys regs (r=%d)\n",
			__func__, retval);
		pr_err("%s: Fail to switch from Bootloader "
			"to Application r=%d\n",
			__func__, retval);

		_cyttsp4_change_state(ts, CY_BL_STATE);

		if (upgraded) {
			pr_err("%s: app failed to launch after"
				" platform firmware upgrade\n", __func__);
			retval = -EIO;
			goto _cyttsp4_startup_tma400_exit;
		}

#ifdef CY_AUTO_LOAD_FW
		if (!ts->powered) {
			pr_info("%s: attempting to reflash IC...\n", __func__);
			if (ts->fw->img == NULL ||
					ts->fw->size == 0) {
				pr_err("%s: no platform firmware available"
					" for reflashing\n", __func__);
				_cyttsp4_change_state(ts, CY_INVALID_STATE);
				retval = -ENODATA;
				goto _cyttsp4_startup_tma400_exit;
			}
			retval = _cyttsp4_load_app(ts,
					ts->fw->img,
					ts->fw->size);
			if (retval) {
				pr_err("%s: failed to reflash IC (r=%d)\n",
					__func__, retval);
				_cyttsp4_change_state(ts, CY_INVALID_STATE);
				retval = -EIO;
				goto _cyttsp4_startup_tma400_exit;
			}
			upgraded = true;
			pr_info("%s: resetting IC after reflashing\n",
				__func__);
			/* Reset the part */
			goto _cyttsp4_startup_tma400_restart;
		}
#endif /* --CY_AUTO_LOAD_FW */
	}

#ifdef CY_AUTO_LOAD_FW
	if (!ts->powered) {
		retval = _cyttsp4_boot_loader(ts, &upgraded);
		if (retval < 0) {
			pr_err("%s: fail boot loader r=%d)\n",
				__func__, retval);
			_cyttsp4_change_state(ts, CY_IDLE_STATE);
			goto _cyttsp4_startup_tma400_exit;
		}
		if (upgraded)
			goto _cyttsp4_startup_tma400_restart;
	}
#endif /* --CY_AUTO_LOAD_FW */

	retval = _cyttsp4_set_mode(ts, CY_CONFIG_MODE);
	if (retval < 0) {
		pr_err("%s: Fail set config mode 1 r=%d\n", __func__, retval);
		goto _cyttsp4_startup_bypass_crc_check;
	}

	retval = _cyttsp4_get_ebid_row_size(ts);
	if (retval < 0) {
		pr_err("%s: Fail get EBID row size; using default r=%d\n",
			__func__, retval);
	}
	pr_debug("%s: get EBID row size=%d\n", __func__, ts->ebid_row_size);

	memset(ic_crc, 0, sizeof(ic_crc));
	pr_debug("%s: Read IC CRC values\n", __func__);
	/* Get settings CRC from touch IC */
	retval = _cyttsp4_set_mode(ts, CY_OPERATE_MODE);
	if (retval < 0) {
		pr_err("%s: Fail set operational mode 1 (r=%d)\n",
			__func__, retval);
		goto _cyttsp4_startup_tma400_exit;
	}
	retval = _cyttsp4_get_ic_crc(ts, CY_TCH_PARM_EBID,
		&ic_crc[0], &ic_crc[1]);
	if (retval < 0) {
		pr_err("%s: Fail read ic crc r=%d\n",
			__func__, retval);
	}

	_cyttsp4_pr_buf(ts, ic_crc, sizeof(ic_crc), "read_ic_crc");

	retval = _cyttsp4_set_mode(ts, CY_CONFIG_MODE);
	if (retval < 0) {
		pr_err("%s: Fail set config mode 2 r=%d\n", __func__, retval);
		goto _cyttsp4_startup_tma400_exit;
	}
#ifdef CY_AUTO_LOAD_TOUCH_PARAMS
	if (!ts->powered) {
		if (!put_all_params_done) {
			if (ts->private_data->sett
					[CY_IC_GRPNUM_TCH_PARM_VAL] == NULL) {
				pr_err("%s: missing param table\n", __func__);
				goto _cyttsp4_startup_bypass_crc_check;
			} else if (ts->private_data->sett
					[CY_IC_GRPNUM_TCH_PARM_VAL]->data
					== NULL) {
				pr_err("%s: missing param table data\n",
					__func__);
				goto _cyttsp4_startup_bypass_crc_check;
			} else if (ts->private_data->sett
					[CY_IC_GRPNUM_TCH_PARM_VAL]->size
					== 0) {
				pr_err("%s: param values table size is 0\n",
					__func__);
				goto _cyttsp4_startup_bypass_crc_check;
			}
			_cyttsp_read_table_crc(ts, ts->private_data->sett
					[CY_IC_GRPNUM_TCH_PARM_VAL]->data,
					&table_crc[0], &table_crc[1]);
			_cyttsp4_pr_buf(ts, table_crc, sizeof(table_crc),
					"read_table_crc");
			if ((ic_crc[0] != table_crc[0]) ||
					(ic_crc[1] != table_crc[1])) {
				retval = _cyttsp4_put_all_params_tma400(ts);
				if (retval < 0) {
					pr_err("%s: Fail put all params r=%d\n",
						__func__, retval);
					goto _cyttsp4_startup_bypass_crc_check;
				}
				put_all_params_done = true;
				goto _cyttsp4_startup_tma400_restart;
			}
		}
	}
#else
	put_all_params_done = true;
#endif /* --CY_AUTO_LOAD_TOUCH_PARAMS */

_cyttsp4_startup_bypass_crc_check:
	if (!mddata_updated) {
		retval = _cyttsp4_check_mddata_tma400(ts, &mddata_updated);
		if (retval < 0) {
			pr_err("%s: Fail update MDDATA r=%d\n",
				__func__, retval);
		} else if (mddata_updated)
			goto _cyttsp4_startup_tma400_restart;
	}

	pr_debug("%s: enter operational mode\n", __func__);
	/* mode=operational mode, state = active_state */
	retval = _cyttsp4_set_mode(ts, CY_OPERATE_MODE);
	if (retval < 0) {
		pr_err("%s: Fail set operational mode 2 (r=%d)\n",
			__func__, retval);
		goto _cyttsp4_startup_tma400_exit;
	}

	if (ts->was_suspended) {
		ts->was_suspended = false;
		retval = _cyttsp4_enter_sleep(ts);
		if (retval < 0) {
			pr_err("%s: fail resume sleep r=%d\n",
				__func__, retval);
		}
	} else {
#ifdef CY_USE_WATCHDOG
		_cyttsp4_start_wd_timer(ts);
#endif
	}
_cyttsp4_startup_tma400_exit:
	ts->starting_up = false;
	return retval;
}

static irqreturn_t ts_irq_handler(int irq, void *handle)
{
	struct ts_data *ts = handle;
	u8 rep_stat = 0;
	int retval = 0;

	pr_debug("%s: GOT IRQ ps=%d\n", __func__, ts->driver_state);
	mutex_lock(&ts->data_lock);

	pr_debug("%s: DO IRQ ps=%d\n", __func__, ts->driver_state);

#ifdef CONFIG_TOUCH_DVFS
	if (false == boost) {
		omap_cpufreq_min_limit(DVFS_LOCK_ID_TSP, CONFIG_TOUCH_DVFS);
		boost = true;
	}
#endif

	switch (ts->driver_state) {
	case CY_BL_STATE:
	case CY_CMD_STATE:
		complete(&ts->int_running);

		break;
	case CY_SYSINFO_STATE:
		complete(&ts->si_int_running);
		break;
	case CY_EXIT_BL_STATE:
		if (ts->switch_flag == true) {
			ts->switch_flag = false;
			retval = _cyttsp4_ldr_exit(ts);
			if (retval < 0) {
				pr_err("%s: Fail bl exit r=%d\n",
					__func__, retval);
			} else
				ts->driver_state = CY_SYSINFO_STATE;
		}
		break;
	case CY_SLEEP_STATE:
		pr_debug("%s: Attempt to process touch after enter sleep or"
			" unexpected wake event\n", __func__);
		retval = _cyttsp4_wakeup(ts); /* in case its really asleep */
		if (retval < 0) {
			pr_err("%s: wakeup fail r=%d\n",
				__func__, retval);
			_cyttsp4_pr_state(ts);
			_cyttsp4_queue_startup(ts, true);
			break;
		}
		/* Put the part back to sleep */
		retval = _cyttsp4_enter_sleep(ts);
		if (retval < 0) {
			pr_err("%s: fail resume sleep r=%d\n",
				__func__, retval);
			_cyttsp4_pr_state(ts);
			_cyttsp4_queue_startup(ts, true);
		}
		break;
	case CY_IDLE_STATE:
		if (ts->xy_mode == NULL) {
			/* initialization is not complete; invalid pointers */
			break;
		}

		/* device now available; signal initialization */
		pr_info("%s: Received IRQ in IDLE state\n",
			__func__);
		/* Try to determine the IC's current state */
		retval = _cyttsp4_load_status_regs(ts);
		if (retval < 0) {
			pr_err("%s: Still unable to access IC after IRQ r=%d\n",
				__func__, retval);
			_cyttsp4_queue_startup(ts, false);
			break;
		}
		rep_stat = ts->xy_mode[ts->si_ofs->rep_ofs + 1];
		if (IS_BOOTLOADERMODE(rep_stat)) {
			pr_info("%s: BL mode found in IDLE state\n",
				__func__);
			_cyttsp4_queue_startup(ts, false);
			break;
		}
		pr_err("%s: interrupt received in IDLE state -"
			" try processing touch\n",
			__func__);
		_cyttsp4_change_state(ts, CY_ACTIVE_STATE);
#ifdef CY_USE_WATCHDOG
		_cyttsp4_start_wd_timer(ts);
#endif
		retval = _cyttsp4_xy_worker(ts);
		if (retval < 0) {
			pr_err("%s: xy_worker IDLE fail r=%d\n",
				__func__, retval);
			_cyttsp4_queue_startup(ts, false);
			break;
		}

		break;
	case CY_READY_STATE:
		complete(&ts->ready_int_running);
		/* do not break; do worker */
	case CY_ACTIVE_STATE:
		if (ts->test.cur_mode == CY_TEST_MODE_CAT) {
			complete(&ts->int_running);
		} else {
			/* process the touches */
			retval = _cyttsp4_xy_worker(ts);
			if (retval < 0) {
				pr_err("%s: XY Worker fail r=%d\n",
					__func__, retval);
				_cyttsp4_queue_startup(ts, false);
			}
		}
		break;
	default:
		break;
	}

	mutex_unlock(&ts->data_lock);
	pr_debug("%s: DONE IRQ ps=%d\n", __func__, ts->driver_state);

	return IRQ_HANDLED;
}

static void reset_points(struct ts_data *ts)
{
	int i;

	for (i = 0; i < MAX_TOUCH; i++) {
		ts->finger_state[i] = 0;
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
									false);
	}
	input_sync(ts->input_dev);
	tsp_log("reset_all_fingers");
	return;
}

static bool init_tsp(struct ts_data *ts)
{
	int retval = 0;

	pr_debug("%s: Open call ts=%p\n", __func__, ts);
	mutex_lock(&ts->data_lock);
	if (!ts->powered) {
		/*
		 * execute complete startup procedure.  After this
		 * call the device is in active state and the worker
		 * is running
		 */
		retval = _cyttsp4_startup(ts);

		/* powered if no hard failure */
		if (retval < 0) {
			ts->powered = false;
			_cyttsp4_change_state(ts, CY_IDLE_STATE);
			pr_err("%s: startup fail at power on r=%d\n",
				__func__, retval);
		} else
			ts->powered = true;

		pr_info("%s: Powered ON(%d) r=%d\n",
			__func__, (int)ts->powered, retval);
	}
	mutex_unlock(&ts->data_lock);
	reset_points(ts);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
int cyttsp4_suspend(void *handle)
{
	struct ts_data *ts = handle;
	int retval = 0;

	if (ts->test.cur_mode != CY_TEST_MODE_NORMAL_OP) {
		retval = -EBUSY;
		pr_err("%s: Suspend Blocked while in test mode=%d\n",
			__func__, ts->test.cur_mode);
	} else {
		switch (ts->driver_state) {
		case CY_ACTIVE_STATE:
#if defined(CY_USE_FORCE_LOAD) || defined(CONFIG_TOUCHSCREEN_DEBUG)
			if (ts->waiting_for_fw) {
				retval = -EBUSY;
				pr_err("%s: Suspend Blocked while waiting for"
					" fw load in %s state\n", __func__,
					cyttsp4_driver_state_string
					[ts->driver_state]);
				break;
			}
#endif
			pr_debug("%s: Suspending...\n", __func__);
#ifdef CY_USE_WATCHDOG
			_cyttsp4_stop_wd_timer(ts);
#endif
			if (ts->irq_enabled)
				disable_irq(ts->irq);

			mutex_lock(&ts->data_lock);

			retval = _cyttsp4_enter_sleep(ts);
			if (retval < 0) {
				pr_err("%s: fail enter sleep r=%d\n",
					__func__, retval);
			} else
				_cyttsp4_change_state(ts, CY_SLEEP_STATE);

			mutex_unlock(&ts->data_lock);

			break;
		case CY_SLEEP_STATE:
			pr_err("%s: already in Sleep state\n", __func__);
			break;
		/*
		 * These states could be changing the device state
		 * Some of these states don't directly change device state
		 * but the next state could happen at any time and that
		 * state DOES modify the device state
		 * they must complete before allowing suspend.
		 */
		case CY_BL_STATE:
		case CY_CMD_STATE:
		case CY_SYSINFO_STATE:
		case CY_READY_STATE:
		case CY_TRANSFER_STATE:
			retval = -EBUSY;
			pr_err("%s: Suspend Blocked while in %s state\n",
				__func__, cyttsp4_driver_state_string
				[ts->driver_state]);
			break;
		case CY_IDLE_STATE:
		case CY_INVALID_STATE:
		default:
			pr_err("%s: Cannot enter suspend from %s state\n",
				__func__, cyttsp4_driver_state_string
				[ts->driver_state]);
			break;
		}
	}

	return retval;
}
EXPORT_SYMBOL_GPL(cyttsp4_suspend);

int cyttsp4_resume(void *handle)
{
	struct ts_data *ts = handle;
	int retval = 0;

	pr_debug("%s: Resuming...\n", __func__);

	mutex_lock(&ts->data_lock);

	switch (ts->driver_state) {
	case CY_SLEEP_STATE:
		retval = _cyttsp4_wakeup(ts);
		if (retval < 0) {
			pr_err("%s: wakeup fail r=%d\n",
				__func__, retval);
			_cyttsp4_pr_state(ts);
			if (ts->irq_enabled)
				enable_irq(ts->irq);
			_cyttsp4_queue_startup(ts, false);
			break;
		}
		_cyttsp4_change_state(ts, CY_ACTIVE_STATE);
		if (ts->irq_enabled)
			enable_irq(ts->irq);
#ifdef CY_USE_WATCHDOG
		_cyttsp4_start_wd_timer(ts);
#endif
		break;
	case CY_IDLE_STATE:
	case CY_READY_STATE:
	case CY_ACTIVE_STATE:
	case CY_BL_STATE:
	case CY_SYSINFO_STATE:
	case CY_CMD_STATE:
	case CY_TRANSFER_STATE:
	case CY_INVALID_STATE:
	default:
		pr_err("%s: Already in %s state\n", __func__,
			cyttsp4_driver_state_string[ts->driver_state]);
		break;
	}

	mutex_unlock(&ts->data_lock);

	pr_debug("%s: exit Resume r=%d\n", __func__, retval);
	return retval;
}
EXPORT_SYMBOL_GPL(cyttsp4_resume);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
void ts_early_suspend(struct early_suspend *h)
{
	struct ts_data *ts = container_of(h, struct ts_data, early_suspend);
	int retval = 0;

	pr_debug("%s: EARLY SUSPEND ts=%p\n", __func__, ts);
	reset_points(ts);
	retval = cyttsp4_suspend(ts);
	if (retval < 0) {
		pr_err("%s: Early suspend failed with error code %d\n",
			__func__, retval);
	}
	#ifdef CONFIG_TOUCH_DVFS
	if (true == boost) {
		omap_cpufreq_min_limit_free(DVFS_LOCK_ID_TSP);
		boost = false;
		del_timer_sync(&dvfs_timer);
	}
#endif

}
void ts_late_resume(struct early_suspend *h)
{
	struct ts_data *ts = container_of(h, struct ts_data, early_suspend);
	int retval = 0;

	pr_debug("%s: LATE RESUME ts=%p\n", __func__, ts);
	retval = cyttsp4_resume(ts);
	if (retval < 0) {
		pr_err("%s: Late resume failed with error code %d\n",
			__func__, retval);
	}
}
#endif

static int __devinit ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int i = 0;
	int retval = 0;
	struct ts_data *ts = NULL;
#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	struct device *fac_dev_ts;
	struct factory_data *factory_data;
	struct node_data *node_data;
#endif
	/* key led */
	struct device *sec_touchkey;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("tsp: ts_probe: need I2C_FUNC_I2C\n");
		retval = -ENODEV;
		goto err_probe_failed;
	}

	ts = kzalloc(sizeof(struct ts_data), GFP_KERNEL);
	if (unlikely(!ts)) {
		pr_err("tsp: ts_probe: failed to malloc ts_data!!\n");
		retval = -ENOMEM;
		goto err_probe_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	if (unlikely(!client->dev.platform_data)) {
		retval = -ENOSYS;
		pr_err("tsp: %s: Didn't find platform data", __func__);
		goto err_probe_failed;
	}

	ts->platform_data = client->dev.platform_data;
	ts->platform_data->driver_data = ts;

#if defined(CY_USE_FORCE_LOAD) || defined(CONFIG_TOUCHSCREEN_DEBUG)
	ts->fwname = kzalloc(CY_BL_FW_NAME_SIZE, GFP_KERNEL);
	if (ts->fwname == NULL) {
		pr_err("%s: Error, kzalloc fwname\n", __func__);
		goto error_alloc_failed;
	}
#endif

	ts->cyttsp4_wq =
		create_singlethread_workqueue("cyttsp4_resume_startup_wq");
	if (ts->cyttsp4_wq == NULL) {
		pr_err("%s: No memory for cyttsp4_resume_startup_wq\n",
			__func__);
		goto error_alloc_failed;
	}

	ts->driver_state = CY_INVALID_STATE;
	ts->current_mode = CY_MODE_BOOTLOADER;
	ts->powered = false;
	ts->was_suspended = false;
	ts->switch_flag = false;
	ts->soft_reset_asserted = false;
	ts->num_prv_tch = 0;

	ts->xy_data = NULL;
	ts->xy_mode = NULL;
	ts->xy_data_touch1 = NULL;
	ts->btn_rec_data = NULL;
	memset(&ts->test, 0, sizeof(struct cyttsp4_test_mode));

	if (ts->platform_data == NULL) {
		pr_err("%s: Error, platform data is Null\n", __func__);
		goto error_alloc_failed;
	}
	ts->private_data = ts->platform_data->private_data;

	mutex_init(&ts->data_lock);
	init_completion(&ts->int_running);
	init_completion(&ts->si_int_running);
	init_completion(&ts->ready_int_running);
	ts->flags = ts->private_data->flags;
#if defined(CY_USE_FORCE_LOAD) || defined(CONFIG_TOUCHSCREEN_DEBUG)
	ts->waiting_for_fw = false;
#endif

	ts->max_config_bytes = CY_TMA400_MAX_BYTES;

	/* Create the input device and register it. */
	pr_debug("%s: Create the input device and register it\n", __func__);
	ts->input_dev = input_allocate_device();
	if (unlikely(!ts->input_dev)) {
		pr_err("tsp: ts_probe: err_input_dev_alloc failed!!\n");
		retval = -ENOMEM;
		goto error_init;
	}

	ts->input_dev->name = SEC_TS_NAME;
#ifdef CY_USE_WATCHDOG
	INIT_WORK(&ts->work, cyttsp4_timer_watchdog);
	setup_timer(&ts->timer, cyttsp4_timer, (unsigned long)ts);
#endif

	input_mt_init_slots(ts->input_dev, MAX_TOUCH);

#if defined(CONFIG_SUPPORT_CYPRESS_TOUCH_KEY)
	set_bit(EV_KEY, ts->input_dev->evbit);
	for (i = 0; i < ts->platform_data->key_size; i++)
		set_bit(ts->platform_data->key[i].code, ts->input_dev->keybit);
	set_bit(EV_LED, ts->input_dev->evbit);
	set_bit(LED_MISC, ts->input_dev->ledbit);
#endif
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	input_mt_init_slots(ts->input_dev, MAX_TOUCH);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			0, ts->platform_data->x_pixel_size, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			0, ts->platform_data->y_pixel_size, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
			0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
			0, MAX_TOUCH_MAJOR, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR,
			0, MAX_TOUCH_MINOR, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION,
			MIN_ORIENTATION, MAX_ORIENTATION, 0, 0);

#ifdef CY_AUTO_LOAD_FW
	ts->fw = &cyttsp4_firmware;
#endif
#ifdef CY_AUTO_LOAD_TOUCH_PARAMS
	ts->params_regs = &cyttsp4_sett_param_regs;
	ts->params_size = &cyttsp4_sett_param_size;
#endif
	input_set_events_per_packet(ts->input_dev, 6 * CY_NUM_TCH_ID);

	if (input_register_device(ts->input_dev) < 0) {
		pr_err("tsp: ts_probe: err_input_register_device failed!!\n");
		retval = -ENOMEM;
		goto error_init;
	}

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	node_data = kzalloc(sizeof(struct node_data), GFP_KERNEL);
	if (unlikely(node_data == NULL)) {
		retval = -ENOMEM;
		goto err_alloc_node_data_failed;
	}
	factory_data = kzalloc(sizeof(struct factory_data), GFP_KERNEL);
	if (unlikely(factory_data == NULL)) {
		retval = -ENOMEM;
		goto err_alloc_factory_data_failed;
	}

	INIT_LIST_HEAD(&factory_data->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &factory_data->cmd_list_head);

	mutex_init(&factory_data->cmd_lock);
	factory_data->cmd_is_running = false;

	fac_dev_ts = device_create(sec_class, NULL, 0, ts, "tsp");
	if (!fac_dev_ts)
		pr_err("[TSP_FACTORY] Failed to create fac tsp dev\n");

	if (sysfs_create_group(&fac_dev_ts->kobj, &touchscreen_attr_group))
		pr_err("[TSP_FACTORY] Failed to create sysfs (touchscreen_attr_group).\n");

	ts->factory_data = factory_data;
	ts->node_data = node_data;

#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ts->early_suspend.suspend = ts_early_suspend;
	ts->early_suspend.resume = ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef CONFIG_TOUCH_DVFS
		setup_timer(&dvfs_timer, timer_cb, 0);
#endif

	ts->private_data->led_power(false);
	sec_touchkey = device_create(sec_class, NULL, 0, ts, "sec_touchkey");
		if (device_create_file(sec_touchkey,
					&dev_attr_brightness) < 0) {
			pr_err("Failed to create device file(%s)!\n",
					dev_attr_brightness.attr.name);
		}

	if (ts->client->irq) {
		tsp_log("trying to request irq: %s-%d",
					ts->client->name, ts->client->irq);
		retval = request_threaded_irq(client->irq, NULL,
					ts_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					ts->client->name, ts);
		if (retval > 0) {
			pr_err("tsp: ts_probe: Can't register irq %d, ret %d\n",
				ts->client->irq, retval);
			ts->irq_enabled = false;
			retval = -EBUSY;
			goto error_init;
		}
		ts->irq_enabled = true;
	}

	INIT_WORK(&ts->cyttsp4_resume_startup_work, cyttsp4_ts_work_func);
	init_tsp(ts);

	goto no_error;

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)

err_alloc_factory_data_failed:
	pr_err("tsp: ts_probe: err_alloc_factory_data failed.\n");

err_alloc_node_data_failed:
	pr_err("tsp: ts_probe: err_alloc_node_data failed.\n");
	kfree(ts->node_data->reference_data);
	kfree(ts->node_data->intensity_data);
	kfree(ts->node_data->cm_abs_data);
	kfree(ts->node_data->cm_delta_data);
	kfree(ts->node_data);

#endif

error_init:
	mutex_destroy(&ts->data_lock);
	if (ts->cyttsp4_wq) {
		destroy_workqueue(ts->cyttsp4_wq);
		ts->cyttsp4_wq = NULL;
	}
error_alloc_failed:
	if (ts != NULL) {
		kfree(ts);
		ts = NULL;
	}
	pr_err("%s: Failed Initialization\n", __func__);
err_probe_failed:
	return retval;

no_error:
	return 0;
}

static int __devexit ts_remove(struct i2c_client *client)
{
	struct ts_data *ts = i2c_get_clientdata(client);

	pr_debug("%s: Release call ts=%p\n",
		__func__, ts);
	if (ts == NULL) {
		pr_err("%s: Null context pointer on driver release\n",
			__func__);
		goto cyttsp4_core_release_exit;
	}

#if defined(CONFIG_SEC_TSP_FACTORY_TEST)
	kfree(ts->node_data->reference_data);
	kfree(ts->node_data->intensity_data);
	kfree(ts->node_data->cm_abs_data);
	kfree(ts->node_data->cm_delta_data);
	kfree(ts->node_data);
	kfree(ts->factory_data);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif
	if (mutex_is_locked(&ts->data_lock))
		mutex_unlock(&ts->data_lock);
	mutex_destroy(&ts->data_lock);
	free_irq(ts->irq, ts);
	input_unregister_device(ts->input_dev);
	if (ts->cyttsp4_wq) {
		destroy_workqueue(ts->cyttsp4_wq);
		ts->cyttsp4_wq = NULL;
	}

	if (ts->sysinfo_ptr.cydata != NULL)
		kfree(ts->sysinfo_ptr.cydata);
	if (ts->sysinfo_ptr.test != NULL)
		kfree(ts->sysinfo_ptr.test);
	if (ts->sysinfo_ptr.pcfg != NULL)
		kfree(ts->sysinfo_ptr.pcfg);
	if (ts->sysinfo_ptr.opcfg != NULL)
		kfree(ts->sysinfo_ptr.opcfg);
	if (ts->sysinfo_ptr.ddata != NULL)
		kfree(ts->sysinfo_ptr.ddata);
	if (ts->sysinfo_ptr.mdata != NULL)
		kfree(ts->sysinfo_ptr.mdata);
	if (ts->xy_mode != NULL)
		kfree(ts->xy_mode);
	if (ts->xy_data != NULL)
		kfree(ts->xy_data);
	if (ts->xy_data_touch1 != NULL)
		kfree(ts->xy_data_touch1);

#ifdef CONFIG_TOUCH_DVFS
	del_timer_sync(&dvfs_timer);
#endif

	kfree(ts);
cyttsp4_core_release_exit:
	return 0;
}

static const struct i2c_device_id cypress_ts_id[] = {
	{"cyttsp4", 0},
	{}
};

static struct i2c_driver cypress_ts_driver = {
	.driver = {
		.name = "cyttsp4",
	},
	.id_table = cypress_ts_id,
	.probe = ts_probe,
	.remove = __devexit_p(ts_remove),
};

static int __devinit cyttsp4_ts_init(void)
{
	return i2c_add_driver(&cypress_ts_driver);
}

static void __exit cyttsp4_ts_exit(void)
{
	i2c_del_driver(&cypress_ts_driver);
}

MODULE_DESCRIPTION("Driver for Cypress GEN4 Touchscreen Controller");
MODULE_AUTHOR("Taejin Hyeon <taejin.hyeon@samsung.com>");
MODULE_LICENSE("GPL");

module_init(cyttsp4_ts_init);
module_exit(cyttsp4_ts_exit);
