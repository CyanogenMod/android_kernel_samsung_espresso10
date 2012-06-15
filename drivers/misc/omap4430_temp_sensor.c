/*
 * OMAP4 Temperature sensor driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Mahesh Kumar<maheshk@ti.com>
 * This driver is based on OMAP4460 temperature sensor driver
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include <plat/common.h>
#include <plat/omap-pm.h>
#include <plat/omap_device.h>
#include <plat/temperature_sensor.h>
#include <plat/omap-pm.h>

/* TO DO: This needs to be fixed */
#include "../../../../arch/arm/mach-omap2/control.h"
#include "omap4430_temp_sensor.h"
/* #include <plat/control.h> */

#include <mach/ctrl_module_core_44xx.h>

static unsigned int curr_section = -1;

#ifdef CONFIG_PM
struct omap_temp_sensor_regs {
	u32 temp_sensor_ctrl;
};

static struct omap_temp_sensor_regs temp_sensor_context;
static struct omap_temp_sensor *temp_sensor_pm;
#endif

/*
 * Temperature values in degrees celsius ADC code values from 14 to 93
 */
static int omap4430_adc_to_temp[] = {
	-38, -35, -34, -32, -30, -28, -26, -24, -22, -20, -18, -17, -15, -13,
	-12, -10, -8, -6, -5, -3, -1, 0, 2, 3, 5, 6, 8, 10, 12, 13, 15, 17, 19,
	21, 23, 25, 27, 28, 30, 32, 33, 35, 37, 38, 40, 42, 43, 45, 47, 48, 50,
	52, 53, 55, 57, 58, 60, 62, 64, 66, 68, 70, 71, 73, 75, 77, 78, 80, 82,
	83, 85, 87, 88, 90, 92, 93, 95, 97, 98, 100, 102, 103, 105, 107, 109,
	111, 113, 115, 117, 118, 120, 122, 123
};


static unsigned long omap_temp_sensor_readl(struct omap_temp_sensor
					    *temp_sensor, u32 reg)
{
	return omap_ctrl_readl(temp_sensor->phy_base + reg);
}

static void omap_temp_sensor_writel(struct omap_temp_sensor *temp_sensor,
				    u32 val, u32 reg)
{
	omap_ctrl_writel(val, (temp_sensor->phy_base + reg));
}

static int adc_to_temp_conversion(int adc_val)
{
	if (adc_val >= 128)
		return -40;
	if (adc_val <= 13)
		return -40;
	else if (adc_val >= 107)
		return 125;
	else
		return omap4430_adc_to_temp[adc_val - 14];
}

static int omap_read_current_temp(struct omap_temp_sensor *temp_sensor)
{
	int adc, val;

	val = omap_temp_sensor_readl(temp_sensor, TEMP_SENSOR_CTRL_OFFSET);
	val |= OMAP4430_BGAP_TEMP_SENSOR_SOC;

	/* Start the Conversion */
	omap_temp_sensor_writel(temp_sensor, val, TEMP_SENSOR_CTRL_OFFSET);
	/* Wait for end of Conversion
	 * Conversion time is about 11-14 32K cycles (~0.5us)
	 * After some testing, for some reason EOCZ bit (8) is always low
	 * even when no conversion ongoing. Don't check it then ...
	 */
	usleep_range(1000, 1100);

	val = omap_temp_sensor_readl(temp_sensor, TEMP_SENSOR_CTRL_OFFSET);
	val &= ~OMAP4430_BGAP_TEMP_SENSOR_SOC;
	/* Stop the Conversion */
	omap_temp_sensor_writel(temp_sensor, val, TEMP_SENSOR_CTRL_OFFSET);

	adc = omap_temp_sensor_readl(temp_sensor, TEMP_SENSOR_CTRL_OFFSET);

	adc &= OMAP4430_BGAP_TEMP_SENSOR_DTEMP_MASK;

	if (!temp_sensor->is_efuse_valid)
		pr_err_once("%s: Invalid EFUSE, Non-trimmed BGAP,"
			    "Temp not accurate\n", __func__);

	if (adc < 0 || adc > 128) {
		pr_err("%s:Invalid adc code reported by the sensor %d",
			__func__, adc);
		return -EINVAL;
	}

	return adc_to_temp_conversion(adc);
}

/*
 * Control CPU by controlling DUTY_CYCLE
 */
void update_policy(void)
{
	/*Set Nitro Percentage*/
	set_duty_nitro_percentage(temp_section[curr_section].nitro_percentage);
	/*Set Nitro Interval */
	set_duty_nitro_interval(temp_section[curr_section].nitro_interval);
	/*Set Nitro Rate*/
	set_duty_nitro_rate(temp_section[curr_section].nitro_rate);
	/*Set Cooling Rate*/
	set_duty_cooling_rate(temp_section[curr_section].cooling_rate);
	/*Enable OR Disable duty cycle*/
	set_duty_cycle_enable(temp_section[curr_section].duty_enabled);
	pr_info("[Thermal] Current Policy :%d: enabled = %ld"	\
			" Nitro_percentage=%ld " \
			"Nitro_interval = %ld "	\
			"Nitro_rate = %ld Cooling_rate = %ld\n",	\
			curr_section,	\
			temp_section[curr_section].duty_enabled, \
			temp_section[curr_section].nitro_percentage, \
			temp_section[curr_section].nitro_interval, \
			temp_section[curr_section].nitro_rate, \
			temp_section[curr_section].cooling_rate \
			);
}

/*
 * Function polls for current temperature in each POLL_DELAY_MS
 * And takes action according to configuration defined in temperature section
 * "temp_section" in omap4430_temp_sensor.h file.
 */

static void temp_poll_work_fn(struct work_struct *work)
{
	int curr;
	int i, new_section = 0;
	struct omap_temp_sensor *temp_sensor =
				container_of(work, struct omap_temp_sensor,
					     temp_poll_work.work);
	curr = omap_read_current_temp(temp_sensor);
	temp_sensor->current_temp = curr;

	pr_debug("[Thermal] Current Temperature :: %d\n", curr);

	for (i = 0; i < ARRAY_SIZE(temp_section); i++) {
		if (curr < temp_section[i].temp) {
			new_section = i;
			break;
		}
	}

	/*If current temperature section is not same as current, update policy*/
	if (new_section != curr_section) {
		curr_section = new_section;
		update_policy();
	}

	/*Reschedule func*/
	schedule_delayed_work(&temp_sensor->temp_poll_work,
			msecs_to_jiffies(POLL_DELAY_MS));
}

static void omap_enable_continuous_mode(struct omap_temp_sensor *temp_sensor)
{
	u32 val;

	val = omap_temp_sensor_readl(temp_sensor, TEMP_SENSOR_CTRL_OFFSET);

	val = val | OMAP4430_BGAP_TEMP_SENSOR_CONTCONV;

	omap_temp_sensor_writel(temp_sensor, val, TEMP_SENSOR_CTRL_OFFSET);
}

static ssize_t omap_temp_show_current(struct device *dev,
				struct device_attribute *devattr,
				char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", omap_read_current_temp(temp_sensor));
}

static DEVICE_ATTR(temperature, S_IRUGO, omap_temp_show_current, NULL);
static struct attribute *omap_temp_sensor_attributes[] = {
	&dev_attr_temperature.attr,
	NULL
};

static const struct attribute_group omap_temp_sensor_group = {
	.attrs = omap_temp_sensor_attributes,
};

#define omap_section_fops(num, node)	\
static ssize_t omap_section_show_##node##num(struct device *dev,	\
				struct device_attribute *devattr,	\
				char *buf)	\
{	\
	return sprintf(buf, "%d\n", (int)temp_section[num - 1].node);	\
}	\
static ssize_t omap_section_store_##node##num(struct device *dev,	\
		struct device_attribute *attr,	\
		const char *buf, size_t size)	\
{	\
	long newval;	\
	if (!strict_strtol(buf, 10, &newval))	\
		temp_section[num - 1].node = (unsigned long)newval;	\
	return size;	\
}	\
static DEVICE_ATTR(node##num, 0666, omap_section_show_##node##num,	\
		omap_section_store_##node##num);	\


#define omap_temp_section_sysfs(num)	\
static ssize_t omap_section_show_temp##num(struct device *dev,	\
				struct device_attribute *devattr,	\
				char *buf)	\
{	\
	return sprintf(buf, "%d\n", temp_section[num - 1].temp);	\
}	\
static ssize_t omap_section_store_temp##num(struct device *dev,	\
		struct device_attribute *attr,	\
		const char *buf, size_t size)	\
{	\
	long newtemp;	\
	if (!strict_strtol(buf, 10, &newtemp))	\
		temp_section[num - 1].temp = (int)newtemp;	\
	return size;	\
}	\
static DEVICE_ATTR(temp##num, 0666, omap_section_show_temp##num,	\
		omap_section_store_temp##num);	\
omap_section_fops(num, duty_enabled);	\
omap_section_fops(num, nitro_percentage);	\
omap_section_fops(num, nitro_interval);	\
omap_section_fops(num, nitro_rate);	\
omap_section_fops(num, cooling_rate);	\
static struct attribute *omap_temp_section##num ## _attributes[] = {	\
	&dev_attr_temp##num.attr,	\
	&dev_attr_duty_enabled##num.attr,	\
	&dev_attr_nitro_percentage##num.attr,	\
	&dev_attr_nitro_interval##num.attr,	\
	&dev_attr_nitro_rate##num.attr,	\
	&dev_attr_cooling_rate##num.attr,	\
	NULL	\
};	\
static const struct attribute_group	\
	omap_temp_section##num ## _group = {	\
	.attrs = omap_temp_section##num ## _attributes,	\
	.name = "section" #num	\
};

omap_temp_section_sysfs(1);
omap_temp_section_sysfs(2);
omap_temp_section_sysfs(3);
omap_temp_section_sysfs(4);
omap_temp_section_sysfs(5);

static int omap_temp_sensor_enable(struct omap_temp_sensor *temp_sensor)
{
	u32 temp;
	u32 ret = 0;
	unsigned long clk_rate;

	unsigned long flags;

	spin_lock_irqsave(&temp_sensor->lock, flags);

	if (temp_sensor->clk_on) {
		pr_debug("%s: clock already on\n", __func__);
		goto out;
	}

	ret = pm_runtime_get_sync(&temp_sensor->pdev->dev);
	if (ret) {
		pr_err("%s:get sync failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	clk_set_rate(temp_sensor->clock, 1000000);
	clk_rate = clk_get_rate(temp_sensor->clock);
	temp_sensor->clk_rate = clk_rate;

	temp = omap_temp_sensor_readl(temp_sensor,
					TEMP_SENSOR_CTRL_OFFSET);
	temp &= ~OMAP4430_BGAP_TEMPSOFF;

	/* write BGAP_TEMPSOFF should be reset to 0 */
	omap_temp_sensor_writel(temp_sensor, temp,
				TEMP_SENSOR_CTRL_OFFSET);
	temp_sensor->clk_on = 1;

out:
	spin_unlock_irqrestore(&temp_sensor->lock, flags);
	return ret;
}


static int omap_temp_sensor_disable(struct omap_temp_sensor *temp_sensor)
{
	u32 temp;
	u32 ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&temp_sensor->lock, flags);

	if (!temp_sensor->clk_on) {
		pr_debug("%s: clock already off\n", __func__);
		goto out;
	}
	temp = omap_temp_sensor_readl(temp_sensor,
				TEMP_SENSOR_CTRL_OFFSET);
	temp |= OMAP4430_BGAP_TEMPSOFF;

	/* write BGAP_TEMPSOFF should be set to 1 before gating clock */
	omap_temp_sensor_writel(temp_sensor, temp,
				TEMP_SENSOR_CTRL_OFFSET);

	ret = pm_runtime_put_sync_suspend(&temp_sensor->pdev->dev);
	if (ret) {
		pr_err("%s:put sync failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}
	temp_sensor->clk_on = 0;

out:
	spin_unlock_irqrestore(&temp_sensor->lock, flags);
	return ret;
}

static int __devinit omap_temp_sensor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct omap_temp_sensor_pdata *pdata = pdev->dev.platform_data;
	struct omap_temp_sensor *temp_sensor;
	struct resource *mem;
	int ret = 0;

	if (!pdata) {
		dev_err(dev, "%s: platform data missing\n", __func__);
		return -EINVAL;
	}

	temp_sensor = kzalloc(sizeof(struct omap_temp_sensor), GFP_KERNEL);
	if (!temp_sensor)
		return -ENOMEM;

	spin_lock_init(&temp_sensor->lock);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(dev, "%s:no mem resource\n", __func__);
		dump_stack();
		ret = -EINVAL;
		goto plat_res_err;
	}

	temp_sensor->phy_base = pdata->offset;
	temp_sensor->pdev = pdev;
	temp_sensor->dev = dev;

	pm_runtime_enable(dev);
	pm_runtime_irq_safe(dev);

	/*
	 * check if the efuse has a non-zero value if not
	 * it is an untrimmed sample and the temperatures
	 * may not be accurate */
	if (omap_readl(OMAP4_CTRL_MODULE_CORE +
			OMAP4_CTRL_MODULE_CORE_STD_FUSE_OPP_BGAP))
		temp_sensor->is_efuse_valid = 1;

	temp_sensor->clock = clk_get(&temp_sensor->pdev->dev, "fck");
	if (IS_ERR(temp_sensor->clock)) {
		ret = PTR_ERR(temp_sensor->clock);
		pr_err("%s:Unable to get fclk: %d\n", __func__, ret);
		ret = -EINVAL;
		goto clk_get_err;
	}
	platform_set_drvdata(pdev, temp_sensor);

	ret = omap_temp_sensor_enable(temp_sensor);
	if (ret) {
		dev_err(dev, "%s:Cannot enable temp sensor\n", __func__);
		goto sensor_enable_err;
	}

	omap_enable_continuous_mode(temp_sensor);

	/* Wait till the first conversion is done wait for at least 1ms */
	mdelay(2);

	/* Read the temperature once due to hw issue*/
	omap_read_current_temp(temp_sensor);

	/* Initialize Polling work queue*/
	INIT_DELAYED_WORK(&temp_sensor->temp_poll_work,
			  temp_poll_work_fn);
	schedule_delayed_work(&temp_sensor->temp_poll_work,
			msecs_to_jiffies(POLL_DELAY_MS));

	ret = sysfs_create_group(&pdev->dev.kobj, &omap_temp_sensor_group);
	if (ret) {
		dev_err(&pdev->dev, "could not create sysfs files\n");
		goto sensor_enable_err;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &omap_temp_section1_group);
	if (ret)
		dev_err(&pdev->dev, "could not create section sysfs files\n");
	ret = sysfs_create_group(&pdev->dev.kobj, &omap_temp_section2_group);
	if (ret)
		dev_err(&pdev->dev, "could not create section sysfs files\n");
	ret = sysfs_create_group(&pdev->dev.kobj, &omap_temp_section3_group);
	if (ret)
		dev_err(&pdev->dev, "could not create section sysfs files\n");
	ret = sysfs_create_group(&pdev->dev.kobj, &omap_temp_section4_group);
	if (ret)
		dev_err(&pdev->dev, "could not create section sysfs files\n");
	ret = sysfs_create_group(&pdev->dev.kobj, &omap_temp_section5_group);
	if (ret)
		dev_err(&pdev->dev, "could not create section sysfs files\n");

	dev_info(dev, "%s probed", pdata->name);

	temp_sensor_pm = temp_sensor;

	return 0;

sensor_enable_err:
	clk_put(temp_sensor->clock);
clk_get_err:
	pm_runtime_disable(dev);
plat_res_err:
	kfree(temp_sensor);
	return ret;
}

static int __devexit omap_temp_sensor_remove(struct platform_device *pdev)
{
	struct omap_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&temp_sensor->temp_poll_work);
	omap_temp_sensor_disable(temp_sensor);
	clk_put(temp_sensor->clock);
	platform_set_drvdata(pdev, NULL);
	kfree(temp_sensor);

	return 0;
}

#ifdef CONFIG_PM
/*
 * Save/Restore the BG Sensor Configuration
 */
static void omap_temp_sensor_save_ctxt(struct omap_temp_sensor *temp_sensor)
{
	temp_sensor_context.temp_sensor_ctrl =
	    omap_temp_sensor_readl(temp_sensor, TEMP_SENSOR_CTRL_OFFSET);
}

static void omap_temp_sensor_restore_ctxt(struct omap_temp_sensor *temp_sensor)
{
	omap_temp_sensor_writel(temp_sensor,
				temp_sensor_context.temp_sensor_ctrl,
				TEMP_SENSOR_CTRL_OFFSET);
}

static int omap_temp_sensor_suspend(struct platform_device *pdev,
				    pm_message_t state)
{
	struct omap_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	/* Stop Polling */
	cancel_delayed_work_sync(&temp_sensor->temp_poll_work);
	omap_temp_sensor_disable(temp_sensor);

	return 0;
}

static int omap_temp_sensor_resume(struct platform_device *pdev)
{
	struct omap_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	omap_temp_sensor_enable(temp_sensor);

	/* Restart Polling */
	schedule_delayed_work(&temp_sensor->temp_poll_work,
			msecs_to_jiffies(POLL_DELAY_MS));

	return 0;
}

void omap_temp_sensor_idle(int idle_state)
{
	if (!temp_sensor_pm)
		return;

	if (idle_state)
		omap_temp_sensor_disable(temp_sensor_pm);
	else
		omap_temp_sensor_enable(temp_sensor_pm);
}

#else
omap_temp_sensor_suspend NULL
omap_temp_sensor_resume NULL

#endif /* CONFIG_PM */
static int omap_temp_sensor_runtime_suspend(struct device *dev)
{
	struct omap_temp_sensor *temp_sensor =
			platform_get_drvdata(to_platform_device(dev));

	omap_temp_sensor_save_ctxt(temp_sensor);
	return 0;
}

static int omap_temp_sensor_runtime_resume(struct device *dev)
{
	struct omap_temp_sensor *temp_sensor =
			platform_get_drvdata(to_platform_device(dev));
	if (omap_pm_was_context_lost(dev))
		omap_temp_sensor_restore_ctxt(temp_sensor);

	return 0;
}

static const struct dev_pm_ops omap_temp_sensor_dev_pm_ops = {
	.runtime_suspend = omap_temp_sensor_runtime_suspend,
	.runtime_resume = omap_temp_sensor_runtime_resume,
};

static struct platform_driver omap_temp_sensor_driver = {
	.probe = omap_temp_sensor_probe,
	.remove = omap_temp_sensor_remove,
	.suspend = omap_temp_sensor_suspend,
	.resume = omap_temp_sensor_resume,
	.driver = {
		.name = "omap_temp_sensor",
		.pm = &omap_temp_sensor_dev_pm_ops,
	},
};

int __init omap_temp_sensor_init(void)
{
	if (!cpu_is_omap443x() || !omap4_has_mpu_1_2ghz())
		return 0;

	return platform_driver_register(&omap_temp_sensor_driver);
}

static void __exit omap_temp_sensor_exit(void)
{
	platform_driver_unregister(&omap_temp_sensor_driver);
}

module_init(omap_temp_sensor_init);
module_exit(omap_temp_sensor_exit);

MODULE_DESCRIPTION("OMAP443X BANDGAP Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
