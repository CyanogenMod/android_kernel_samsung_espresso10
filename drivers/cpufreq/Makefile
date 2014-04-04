# CPUfreq core
obj-$(CONFIG_CPU_FREQ)			+= cpufreq.o cpu-boost.o
# CPUfreq stats
obj-$(CONFIG_CPU_FREQ_STAT)             += cpufreq_stats.o

# CPUfreq governors
obj-$(CONFIG_CPU_FREQ_GOV_PERFORMANCE)	+= cpufreq_performance.o
obj-$(CONFIG_CPU_FREQ_GOV_POWERSAVE)	+= cpufreq_powersave.o
obj-$(CONFIG_CPU_FREQ_GOV_USERSPACE)	+= cpufreq_userspace.o
obj-$(CONFIG_CPU_FREQ_GOV_ONDEMAND)	+= cpufreq_ondemand.o
obj-$(CONFIG_CPU_FREQ_GOV_CONSERVATIVE)	+= cpufreq_conservative.o
obj-$(CONFIG_CPU_FREQ_GOV_INTERACTIVE)	+= cpufreq_interactive.o
obj-$(CONFIG_CPU_FREQ_GOV_HOTPLUG)	+= cpufreq_hotplug.o

# CPUfreq cross-arch helpers
obj-$(CONFIG_CPU_FREQ_TABLE)		+= freq_table.o

# ARM SoC drivers
obj-$(CONFIG_UX500_SOC_DB8500)		+= db8500-cpufreq.o
