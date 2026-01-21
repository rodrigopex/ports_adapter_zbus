#ifndef MODULES_SERVICES_SHARED_SERVICE_H
#define MODULES_SERVICES_SHARED_SERVICE_H

struct service {
	char *name;
	char *description;
	struct {
		const struct zbus_channel *invoke;
		const struct zbus_channel *report;
	} channel;
};

#define SERVICE_DEFINE(_name, _description, _invoke_chan, _report_chan)                            \
	static struct service _service_##_name = {                                                 \
		.name = _name,                                                                     \
		.description = _description,                                                       \
		.channel =                                                                         \
			{                                                                          \
				.invoke = _invoke_chan,                                            \
				.report = _report_chan,                                            \
			},                                                                         \
	}

#endif /* MODULES_SERVICES_SHARED_SERVICE_H */
