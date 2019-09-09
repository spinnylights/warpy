#include <malloc.h>
#include <stdint.h>

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/patch/patch.h>

#include "warpy.h"

#define WARPY_URI "https://milky.flowers/programs/warpy"
#define WARPY__sample WARPY_URI "#sample"

enum port_indices {
	WARPY_IN,
	WARPY_OUT_L,
	WARPY_OUT_R,
	WARPY_START_POINT,
	WARPY_END_POINT,
	WARPY_REVERSE,
	WARPY_LOOP_TIMES,
	WARPY_SUSTAIN_SECTION,
	WARPY_TIE_SUSTAIN_END_TO_MAIN_END,
	WARPY_SUSTAIN_START_POINT,
	WARPY_SUSTAIN_END_POINT,
	WARPY_RELEASE_SECTION,
	WARPY_TIE_RELEASE_START_TO_MAIN_END,
	WARPY_RELEASE_START_POINT,
	WARPY_RELEASE_END_POINT,
	WARPY_RELEASE_LOOP_TIMES,
	WARPY_SPEED_ADJUST,
	WARPY_SPEED_CENTER,
	WARPY_SPEED_LOWER_SCALE,
	WARPY_SPEED_UPPER_SCALE,
	WARPY_PITCH_ADJUST,
	WARPY_PITCH_CENTER,
	WARPY_PITCH_LOWER_SCALE,
	WARPY_PITCH_UPPER_SCALE,
	WARPY_ATTACK_TIME,
	WARPY_ATTACK_SHAPE,
	WARPY_DECAY_TIME,
	WARPY_DECAY_SHAPE,
	WARPY_SUSTAIN_LEVEL,
	WARPY_RELEASE_TIME,
	WARPY_RELEASE_SHAPE,
	WARPY_GAIN
};

struct lv2 {
	struct warpy* warpy;

	struct {
		const LV2_Atom_Sequence* in;
		float*                   out_l;
		float*                   out_r;
		float*                   start_point;
		float*                   end_point;
		float*                   reverse;
		float*                   loop_times;
		float*                   sustain_section;
		float*                   tie_sustain_end_to_main_end;
		float*                   sustain_start_point;
		float*                   sustain_end_point;
		float*                   release_section;
		float*                   tie_release_start_to_main_end;
		float*                   release_start_point;
		float*                   release_end_point;
		float*                   release_loop_times;
		float*                   speed_adjust;
		float*                   speed_center;
		float*                   speed_lower_scale;
		float*                   speed_upper_scale;
		float*                   pitch_adjust;
		float*                   pitch_center;
		float*                   pitch_lower_scale;
		float*                   pitch_upper_scale;
		float*                   attack_time;
		float*                   attack_shape;
		float*                   decay_time;
		float*                   decay_shape;
		float*                   sustain_level;
		float*                   release_time;
		float*                   release_shape;
		float*                   gain;
	} ports;

	LV2_URID_Map* urid_map;
	LV2_Atom_Forge forge;
	struct {
		LV2_URID atom_urid;
		LV2_URID midi_event;
		LV2_URID patch_set;
		LV2_URID patch_set_property;
		LV2_URID patch_set_value;
		LV2_URID warpy_sample;
	} uris;
};

static void map_urids(struct lv2* lv2)
{
	lv2->uris.atom_urid =
	        lv2->urid_map->map(lv2->urid_map->handle, LV2_ATOM__URID);
	lv2->uris.midi_event =
	        lv2->urid_map->map(lv2->urid_map->handle, LV2_MIDI__MidiEvent);
	lv2->uris.patch_set =
	        lv2->urid_map->map(lv2->urid_map->handle, LV2_PATCH__Set);
	lv2->uris.patch_set_property =
	        lv2->urid_map->map(lv2->urid_map->handle, LV2_PATCH__property);
	lv2->uris.patch_set_value =
	        lv2->urid_map->map(lv2->urid_map->handle, LV2_PATCH__value);
	lv2->uris.warpy_sample =
	        lv2->urid_map->map(lv2->urid_map->handle, WARPY__sample);
}

static LV2_Handle instantiate(const LV2_Descriptor*     descriptor,
                              double                    rate,
                              const char*               bundle_path,
                              const LV2_Feature* const* features)
{

	struct lv2* lv2 = (struct lv2*)malloc(sizeof(struct lv2));

	struct warpy* warpy = create_warpy(rate);
	lv2->warpy = warpy;

	for (int i = 0; features[i]; i++)
		if (!strcmp(features[i]->URI, LV2_URID__map))
			lv2->urid_map = (LV2_URID_Map*)features[i]->data;

	lv2_atom_forge_init(&lv2->forge, lv2->urid_map);

	map_urids(lv2);

	return (LV2_Handle)lv2;
}

static void connect_port(LV2_Handle instance,
                         uint32_t   port,
                         void*      data)
{
	struct lv2* lv2 = (struct lv2*)instance;

	switch ((enum port_indices)port) {
		case WARPY_IN:
			lv2->ports.in = (const LV2_Atom_Sequence*)data;
			break;
		case WARPY_OUT_L:
			lv2->ports.out_l = (float*)data;
			break;
		case WARPY_OUT_R:
			lv2->ports.out_r = (float*)data;
			break;
		case WARPY_START_POINT:
			lv2->ports.start_point = (float*)data;
			break;
		case WARPY_END_POINT:
			lv2->ports.end_point = (float*)data;
			break;
		case WARPY_REVERSE:
			lv2->ports.reverse = (float*)data;
			break;
		case WARPY_LOOP_TIMES:
			lv2->ports.loop_times = (float*)data;
			break;
		case WARPY_SUSTAIN_SECTION:
			lv2->ports.sustain_section = (float*)data;
			break;
		case WARPY_TIE_SUSTAIN_END_TO_MAIN_END:
			lv2->ports.tie_sustain_end_to_main_end = (float*)data;
			break;
		case WARPY_SUSTAIN_START_POINT:
			lv2->ports.sustain_start_point = (float*)data;
			break;
		case WARPY_SUSTAIN_END_POINT:
			lv2->ports.sustain_end_point = (float*)data;
			break;
		case WARPY_RELEASE_SECTION:
			lv2->ports.release_section = (float*)data;
			break;
		case WARPY_TIE_RELEASE_START_TO_MAIN_END:
			lv2->ports.tie_release_start_to_main_end = (float*)data;
			break;
		case WARPY_RELEASE_START_POINT:
			lv2->ports.release_start_point = (float*)data;
			break;
		case WARPY_RELEASE_END_POINT:
			lv2->ports.release_end_point = (float*)data;
			break;
		case WARPY_RELEASE_LOOP_TIMES:
			lv2->ports.release_loop_times = (float*)data;
			break;
		case WARPY_SPEED_ADJUST:
			lv2->ports.speed_adjust = (float*)data;
			break;
		case WARPY_SPEED_CENTER:
			lv2->ports.speed_center = (float*)data;
			break;
		case WARPY_SPEED_LOWER_SCALE:
			lv2->ports.speed_lower_scale = (float*)data;
			break;
		case WARPY_SPEED_UPPER_SCALE:
			lv2->ports.speed_upper_scale = (float*)data;
			break;
		case WARPY_PITCH_ADJUST:
			lv2->ports.pitch_adjust = (float*)data;
			break;
		case WARPY_PITCH_CENTER:
			lv2->ports.pitch_center = (float*)data;
			break;
		case WARPY_PITCH_LOWER_SCALE:
			lv2->ports.pitch_lower_scale = (float*)data;
			break;
		case WARPY_PITCH_UPPER_SCALE:
			lv2->ports.pitch_upper_scale = (float*)data;
			break;
		case WARPY_ATTACK_TIME:
			lv2->ports.attack_time = (float*)data;
			break;
		case WARPY_ATTACK_SHAPE:
			lv2->ports.attack_shape = (float*)data;
			break;
		case WARPY_DECAY_TIME:
			lv2->ports.decay_time = (float*)data;
			break;
		case WARPY_DECAY_SHAPE:
			lv2->ports.decay_shape = (float*)data;
			break;
		case WARPY_SUSTAIN_LEVEL:
			lv2->ports.sustain_level = (float*)data;
			break;
		case WARPY_RELEASE_TIME:
			lv2->ports.release_time = (float*)data;
			break;
		case WARPY_RELEASE_SHAPE:
			lv2->ports.release_shape = (float*)data;
			break;
		case WARPY_GAIN:
			lv2->ports.gain = (float*)data;
			break;
	}
}

static void activate(LV2_Handle instance)
{
	struct lv2* lv2 = (struct lv2*)instance;
	start_warpy(lv2->warpy);
}

static void update_control_ports(struct lv2* lv2)
{
	update_start_and_end_points(lv2->warpy,
	                            *(lv2->ports.start_point),
	                            *(lv2->ports.end_point),
	                            get_main_bounds(lv2->warpy));
	update_gain(lv2->warpy,   *(lv2->ports.gain));
	update_reverse(lv2->warpy, *(lv2->ports.reverse));
	update_loop_times(lv2->warpy, *(lv2->ports.loop_times));

	update_sustain_section(lv2->warpy, *(lv2->ports.sustain_section));
	update_tie_sustain_end_to_main_end(lv2->warpy,
	                             *(lv2->ports.tie_sustain_end_to_main_end));
	update_start_and_end_points(lv2->warpy,
	                            *(lv2->ports.sustain_start_point),
	                            *(lv2->ports.sustain_end_point),
	                            get_sustain_bounds(lv2->warpy));

	update_release_section(lv2->warpy, *(lv2->ports.release_section));
	update_tie_release_start_to_main_end(lv2->warpy,
	                           *(lv2->ports.tie_release_start_to_main_end));
	update_start_and_end_points(lv2->warpy,
	                            *(lv2->ports.release_start_point),
	                            *(lv2->ports.release_end_point),
	                            get_release_bounds(lv2->warpy));
	update_release_loop_times(lv2->warpy, *(lv2->ports.release_loop_times));

	struct envelope env;
	env.attack_time   = *(lv2->ports.attack_time);
	env.attack_shape  = *(lv2->ports.attack_shape);
	env.decay_time    = *(lv2->ports.decay_time);
	env.decay_shape   = *(lv2->ports.decay_shape);
	env.sustain_level = *(lv2->ports.sustain_level);
	env.release_time  = *(lv2->ports.release_time);
	env.release_shape = *(lv2->ports.release_shape);
	update_envelope(lv2->warpy, env);

	struct vocoder_settings speed_settings;
	speed_settings.type        = VOC_SPEED;
	speed_settings.adjust      = *(lv2->ports.speed_adjust);
	speed_settings.center      = *(lv2->ports.speed_center);
	speed_settings.lower_scale = *(lv2->ports.speed_lower_scale);
	speed_settings.upper_scale = *(lv2->ports.speed_upper_scale);
	update_vocoder_settings(lv2->warpy, speed_settings);

	struct vocoder_settings pitch_settings;
	pitch_settings.type        = VOC_PITCH;
	pitch_settings.adjust      = *(lv2->ports.pitch_adjust);
	pitch_settings.center      = *(lv2->ports.pitch_center);
	pitch_settings.lower_scale = *(lv2->ports.pitch_lower_scale);
	pitch_settings.upper_scale = *(lv2->ports.pitch_upper_scale);
	update_vocoder_settings(lv2->warpy, pitch_settings);
}

static void process_patch_set(struct lv2* lv2, const LV2_Atom_Object* obj)
{
	const LV2_Atom* property = NULL;
	const LV2_Atom* value    = NULL;
	lv2_atom_object_get(obj,
			    lv2->uris.patch_set_property, &property,
			    lv2->uris.patch_set_value,    &value,
			    0);

	if (!property) {
		fprintf(stderr, "Patch set message sent with no property\n");
		return;
	} else if (property->type != lv2->uris.atom_urid) {
		fprintf(stderr, "Patch set message property is not a URID\n");
		return;
	}

	const uint32_t key = ((const LV2_Atom_URID*)property)->body;
	if (key == lv2->uris.warpy_sample) {
		char* sample_path = LV2_ATOM_BODY(value);
		update_sample_path(lv2->warpy, sample_path);
	}
}
static void process_incoming_events(struct lv2* lv2)
{
	LV2_ATOM_SEQUENCE_FOREACH(lv2->ports.in, event) {
		if (event->body.type == lv2->uris.midi_event) {
			uint8_t* raw = (uint8_t*)LV2_ATOM_BODY(&event->body);
			uint64_t size = event->body.size;
			send_midi_message(lv2->warpy, raw, size);
		}
		else if (lv2_atom_forge_is_object_type
		        (&lv2->forge, event->body.type)) {
			const LV2_Atom_Object* obj =
			        (const LV2_Atom_Object*)&event->body;
			if (obj->body.otype == lv2->uris.patch_set)
				process_patch_set(lv2, obj);
		};
	}
}

static void run(LV2_Handle instance, uint32_t times)
{
	struct lv2* lv2 = (struct lv2*)instance;

	if (times == 0) {
		update_control_ports(lv2);
		return;
	}

	update_control_ports(lv2);
	process_incoming_events(lv2);

	for (int i = 0; i < times; i++) {
		struct audio_sample sample = gen_sample(lv2->warpy);
		float* out_l = lv2->ports.out_l;
		float* out_r = lv2->ports.out_r;
		out_l[i] = sample.left;
		out_r[i] = sample.right;
	}
}

static void deactivate(LV2_Handle instance)
{
	struct lv2* lv2 = (struct lv2*)instance;
	stop_warpy(lv2->warpy);
}

static void cleanup(LV2_Handle instance)
{
	struct lv2* lv2 = (struct lv2*)instance;
	destroy_warpy(lv2->warpy);
	free(lv2);
}

static const void* extension_data(const char *uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	WARPY_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	switch (index) {
		case 0:  return &descriptor;
		default: return NULL;
	}
}
