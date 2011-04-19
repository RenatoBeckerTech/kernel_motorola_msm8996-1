#if defined(__i386__) || defined(__x86_64__)

#include "helpers/helpers.h"

int cpufreq_has_boost_support(unsigned int cpu, int *support, int *active,
			int *states)
{
	struct cpupower_cpu_info cpu_info;
	int ret;

	*support = *active = *states = 0;

	ret = get_cpu_info(0, &cpu_info);
	if (ret)
		return ret;

	if (cpupower_cpu_info.caps & CPUPOWER_CAP_AMD_CBP) {
		*support = 1;
		amd_pci_get_num_boost_states(active, states);
		if (ret <= 0)
			return ret;
		*support = 1;
	} else if (cpupower_cpu_info.vendor == X86_VENDOR_INTEL) {
		ret = msr_intel_has_boost_support(cpu);
		if (ret <= 0)
			return ret;
		*support = ret;
		ret = msr_intel_boost_is_active(cpu);
		if (ret <= 0)
			return ret;
		*active = ret;
	}
	return 0;
}
#endif /* #if defined(__i386__) || defined(__x86_64__) */
