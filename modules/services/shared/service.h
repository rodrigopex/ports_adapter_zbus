#ifndef MODULES_SERVICES_SHARED_SERVICE_H
#define MODULES_SERVICES_SHARED_SERVICE_H

#include <zephyr/kernel.h>

struct service {
	const char *name;
	struct {
		const struct zbus_channel *invoke;
		const struct zbus_channel *report;
	} channel;
	int (*init_fn)(const struct service *self);
	void *api;
	void *const data;
};

#define SERVICE_DEFINE(_name, _init_fn, _api, _data)                                               \
	const STRUCT_SECTION_ITERABLE(service, _name) = {                                          \
		.name = #_name,                                                                    \
		.channel =                                                                         \
			{                                                                          \
				.invoke = &CONCAT(chan_, _name, _invoke),                          \
				.report = &CONCAT(chan_, _name, _report),                          \
			},                                                                         \
		.init_fn = _init_fn,                                                               \
		.api = _api,                                                                       \
		.data = _data}

#endif /* MODULES_SERVICES_SHARED_SERVICE_H */
