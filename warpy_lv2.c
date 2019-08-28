#include <malloc.h>
#include <stdint.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

enum port_indices {
	WARPY_IN,
	WARPY_OUT_L,
	WARPY_OUT_R,
	WARPY_SPEED,
	WARPY_CENTER,
	WARPY_GAIN
};

struct ports {
	const LV2_Atom_Sequence* in_port;
	float*                   out_l_port;
	float*                   out_r_port;
	float*                   speed_port;
	float*                   center_port;
	float*                   gain_port;
};

struct warpy {
	struct ports* ports;
	char* sample_path;
	double rate;
};

static LV2_Handle instatiate(const LV2_Descriptor*     descriptor,
                             double                    rate,
                             const char*               bundle_path,
                             const LV2_Feature* const* features)
{
	struct warpy* warpy = (struct warpy*)malloc(sizeof(struct warpy));

	warpy->rate = rate;

	return (LV2_Handle)warpy;
}

static void connect_port(LV2_Handle instance,
                         uint32_t   port,
                         void*      data)
{
	struct warpy* warpy = (struct warpy*)instance;

	switch ((enum port_indices)port) {
		case WARPY_IN:
			warpy->ports->in_port = (const LV2_Atom_Sequence*)data;
			break;
		case WARPY_OUT_L:
			warpy->ports->out_l_port = (float*)data;
			break;
		case WARPY_OUT_R:
			warpy->ports->out_r_port = (float*)data;
			break;
		case WARPY_SPEED:
			warpy->ports->speed_port = (float*)data;
			break;
		case WARPY_CENTER:
			warpy->ports->center_port = (float*)data;
			break;
		case WARPY_GAIN:
			warpy->ports->gain_port = (float*)data;
			break;
	}
}

static void cleanup(LV2_Handle instance)
{
	free(instance);
}
