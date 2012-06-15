#ifndef __OMAP4430_TEMP_SENSOR_H__
#define __OMAP4430_TEMP_SENSOR_H__


#define OMAP4430_BGAP_TEMPSOFF                          (1 << 12)
#define OMAP4430_BGAP_TEMP_SENSOR_CONTCONV              (1 << 10)
#define OMAP4430_BGAP_TEMP_SENSOR_SOC                   (1 << 9)
#define OMAP4430_BGAP_TEMP_SENSOR_DTEMP_MASK            0x000000FF
#define POLL_DELAY_MS                                   2000

/*
 * omap_temp_sensor structure
 * @pdev - Platform device pointer
 * @dev - device pointer
 * @clock - Clock pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 * @phy_base - Physical base of the temp I/O
 * @is_efuse_valid - Flag to determine if eFuse is valid or not
 * @clk_on - Manages the current clock state
 * @clk_rate - Holds current clock rate
 * @current_temp - Holds current temperature value
 * @temp_poll_work - Polling function to get the temperature
 */
struct omap_temp_sensor {
	struct platform_device *pdev;
	struct device *dev;
	struct clk *clock;
	struct spinlock lock;
	unsigned long phy_base;
	int is_efuse_valid;
	u8 clk_on;
	unsigned long clk_rate;
	u32 current_temp;
	struct delayed_work temp_poll_work;
};

/*
 * @temp : Maximum temperature for the current section (In degree Celsius)
 * @duty_enabled : duty cycle will be enabled OR disabled
 * @nitro_percentage : percentage of nitro_interval cpu @ nitro_rate
 * @nitro_interval : Period of one duty cycle
 * @nitro_rate : maximux permissible frequency in current zone
 * @cooling_rate : cooling rate for duty cycle in current zone
 */

struct thermal_policy {
	int temp;
	unsigned long duty_enabled;
	unsigned long nitro_percentage;
	unsigned long nitro_interval;
	unsigned long nitro_rate;
	unsigned long cooling_rate;
};

/*
 * Configurable temperature Zone & corresponding
 * Action to be taken.You can have as much section as you want
 * But section *must* be assending temperature order & *must* contain
 * All the members defined.
 */

static struct thermal_policy temp_section[] = {
	{
		.temp = 53,
		.duty_enabled = 0,
		.nitro_percentage = 100,
		.nitro_rate = 1200000,
		.cooling_rate = 1008000,
		.nitro_interval = 20000,
	},
	{
		.temp = 60,
		.duty_enabled = 1,
		.nitro_percentage = 100,
		.nitro_rate = 800000,
		.cooling_rate = 800000,
		.nitro_interval = 20000,
	},
	{
		.temp = 61,
		.duty_enabled = 1,
		.nitro_percentage = 33,
		.nitro_rate = 1200000,
		.cooling_rate = 800000,
		.nitro_interval = 20000,
	},
	{
		.temp = 62,
		.duty_enabled = 1,
		.nitro_percentage = 25,
		.nitro_rate = 1200000,
		.cooling_rate = 800000,
		.nitro_interval = 20000,
	},
	{
		.temp = 63,
		.duty_enabled = 1,
		.nitro_percentage = 100,
		.nitro_rate = 800000,
		.cooling_rate = 800000,
		.nitro_interval = 20000,
	},
};

/*Enable OR Disable duty cycle*/
int set_duty_cycle_enable(unsigned long);
/*Set Nitro Percentage*/
int set_duty_nitro_percentage(unsigned long);
/*Set Nitro Interval */
int set_duty_nitro_interval(unsigned long);
/*Set Nitro Rate*/
int set_duty_nitro_rate(unsigned long);
/*Set Cooling Rate*/
int set_duty_cooling_rate(unsigned long);

#endif
