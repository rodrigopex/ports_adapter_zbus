#include "service.h"

#include <zephyr/kernel.h>

int services_init_fn(void)
{
	printk("Init services!\n");

	STRUCT_SECTION_FOREACH(service, instance) {
		printk("%p: %s initialing\n", instance, instance->name);
		if (instance->init_fn != NULL) {
			instance->init_fn(instance);
		}
	}

	return 0;
}

SYS_INIT(services_init_fn, APPLICATION, 99);
