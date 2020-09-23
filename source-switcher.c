#include <obs-module.h>
#include "source-switcher.h"

struct switcher_hotkey_info {
	obs_hotkey_id hotkey_id;
	obs_source_t *source;
};

struct switcher_info {
	obs_source_t *source;
	obs_source_t *current_source;
	DARRAY(obs_source_t *) sources;
	DARRAY(struct switcher_hotkey_info) hotkeys;
	size_t current_index;
	bool loop;
	uint64_t last_switch_time;

	bool time_switch;
	uint64_t time_switch_duration;
	int32_t time_switch_to;

	bool media_state_switch;
	int32_t media_switch_state;
	int32_t media_state_switch_to;

	obs_source_t *transition;
	bool transition_running;
	bool transition_resize;
	uint64_t transition_duration;
};

static const char *switcher_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("SourceSwitcher");
}

void switcher_source_rename(void *data, calldata_t *call_data)
{
	struct switcher_info *switcher = data;
	const char *new_name = calldata_string(call_data, "new_name");
	const char *prev_name = calldata_string(call_data, "prev_name");
	obs_data_t *settings = obs_source_get_settings(switcher->source);
	if (!settings || !new_name || !prev_name)
		return;
	obs_data_array_t *sources = obs_data_get_array(settings, S_SOURCES);
	if (sources) {
		size_t count = obs_data_array_count(sources);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(sources, i);
			const char *source_name =
				obs_data_get_string(item, "value");
			if (strcmp(source_name, prev_name) == 0) {
				obs_data_set_string(item, "value", new_name);
			}
			obs_data_release(item);
		}
		obs_data_array_release(sources);
	}
	obs_data_release(settings);
}

void switcher_index_changed(struct switcher_info *switcher)
{
	if (!switcher->sources.num)
		return;

	if (switcher->current_index >= switcher->sources.num) {
		switcher->current_index =
			switcher->loop ? 0 : switcher->sources.num - 1;
	}
	obs_source_t *dest = switcher->sources.array[switcher->current_index];
	if (switcher->current_source == dest)
		return;

	if (switcher->transition) {
		if (!switcher->transition_resize) {
			uint32_t cx = obs_source_get_width(dest);
			uint32_t cy = obs_source_get_height(dest);
			if (switcher->current_source) {
				const uint32_t cxa = obs_source_get_width(
					switcher->current_source);
				if (cxa > cx)
					cx = cxa;
				const uint32_t cya = obs_source_get_height(
					switcher->current_source);
				if (cya > cy)
					cy = cya;
			}
			obs_transition_set_size(switcher->transition, cx, cy);
		} else {
			obs_transition_set_size(
				switcher->transition,
				obs_source_get_width(switcher->current_source),
				obs_source_get_height(
					switcher->current_source));
		}
		obs_transition_set(switcher->transition,
				   switcher->current_source);
		obs_transition_start(switcher->transition,
				     OBS_TRANSITION_MODE_AUTO,
				     switcher->transition_duration, dest);
		obs_source_add_active_child(switcher->source,
					    switcher->transition);
		switcher->transition_running = true;
		uint32_t cx;
		uint32_t cy;
		obs_transition_get_size(switcher->transition, &cx, &cy);
		blog(LOG_INFO,
		     "[source-switcher: '%s'] transition to '%s' using '%s' for %i ms, %s {%i,%i}",
		     obs_source_get_name(switcher->source),
		     obs_source_get_name(dest),
		     obs_source_get_name(switcher->transition),
		     switcher->transition_duration,
		     switcher->transition_resize ? "resize" : "fixed size", cx,
		     cy);
	} else {
		blog(LOG_INFO, "[source-switcher: '%s'] switch to '%s'",
		     obs_source_get_name(switcher->source),
		     obs_source_get_name(dest));
	}
	if (switcher->current_source) {
		obs_source_release(switcher->current_source);
		obs_source_remove_active_child(switcher->source,
					       switcher->current_source);
	}
	switcher->current_source = dest;
	obs_source_addref(switcher->current_source);
	obs_source_add_active_child(switcher->source, switcher->current_source);
}

void switcher_switch_to(struct switcher_info *switcher, int32_t switch_to)
{
	switcher->last_switch_time = obs_get_video_frame_time();
	if (switch_to == SWITCH_NONE) {
		if (switcher->current_source) {
			obs_source_release(switcher->current_source);
			obs_source_remove_active_child(
				switcher->source, switcher->current_source);
			if (switcher->transition) {
				obs_transition_set_size(
					switcher->transition,
					obs_source_get_width(
						switcher->current_source),
					obs_source_get_height(
						switcher->current_source));
				obs_transition_set(switcher->transition,
						   switcher->current_source);
				obs_transition_start(
					switcher->transition,
					OBS_TRANSITION_MODE_AUTO,
					switcher->transition_duration, NULL);
				obs_source_add_active_child(
					switcher->source, switcher->transition);
				switcher->transition_running = true;
				blog(LOG_INFO,
				     "[source-switcher: '%s'] transition to none",
				     obs_source_get_name(switcher->source));
			} else {
				blog(LOG_INFO,
				     "[source-switcher: '%s'] switch to none",
				     obs_source_get_name(switcher->source));
			}
			switcher->current_source = NULL;
		}
		return;
	}
	if (switch_to == SWITCH_NEXT) {
		switcher->current_index++;
	} else if (switch_to == SWITCH_PREVIOUS) {
		if (!switcher->current_index) {
			if (switcher->loop && switcher->sources.num) {
				switcher->current_index =
					switcher->sources.num - 1;
			}
		} else {
			switcher->current_index--;
		}
	} else if (switch_to == SWITCH_RANDOM) {
		if (switcher->sources.num <= 1) {
			switcher->current_index = 0;
		} else {
			if (switcher->current_index < switcher->sources.num) {
				const size_t r = (size_t)rand() %
						 (switcher->sources.num - 1);
				if (r < switcher->current_index)
					switcher->current_index = r;
				else
					switcher->current_index = r + 1;
			} else {
				switcher->current_index =
					(size_t)rand() % switcher->sources.num;
			}
		}
	} else if (switch_to == SWITCH_FIRST) {
		switcher->current_index = 0;
	} else if (switch_to == SWITCH_LAST) {
		if (switcher->sources.num)
			switcher->current_index = switcher->sources.num - 1;
	}
	switcher_index_changed(switcher);
}

void switcher_none_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			  bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	switcher_switch_to(switcher, SWITCH_NONE);
}

void switcher_next_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			  bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	switcher_switch_to(switcher, SWITCH_NEXT);
}

void switcher_previous_hotkey(void *data, obs_hotkey_id id,
			      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	switcher_switch_to(switcher, SWITCH_PREVIOUS);
}

void switcher_random_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			    bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed || !switcher->sources.num)
		return;
	switcher_switch_to(switcher, SWITCH_RANDOM);
}

void switcher_shuffle_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			    bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed || !switcher->sources.num)
		return;
	obs_data_t * settings = obs_source_get_settings(switcher->source);
	if (!settings)
		return;
	obs_data_array_t *sources = obs_data_get_array(settings, S_SOURCES);
	if (sources) {
		const size_t count = obs_data_array_count(sources);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(sources, i);
			obs_data_array_erase(sources, i);
			obs_data_array_insert(sources, (size_t)rand() % count,
					      item);
			obs_data_release(item);
		}
	}
	obs_source_update(switcher->source, settings);
	obs_data_release(settings);
}

void switcher_first_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			   bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	switcher_switch_to(switcher, SWITCH_FIRST);
}

void switcher_last_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			  bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	switcher_switch_to(switcher, SWITCH_LAST);
}

void switcher_switch_source_hotkey(void *data, obs_hotkey_id id,
				   obs_hotkey_t *hotkey, bool pressed)
{
	if (!pressed)
		return;
	struct switcher_info *switcher = data;
	obs_source_t *source = NULL;
	for (size_t i = 0; i < switcher->hotkeys.num; i++) {
		if (switcher->hotkeys.array[i].hotkey_id == id)
			source = switcher->hotkeys.array[i].source;
	}
	if (!source)
		return;
	for (size_t i = 0; i < switcher->sources.num; i++) {
		if (switcher->sources.array[i] == source) {
			switcher->current_index = i;
			switcher_index_changed(switcher);
			break;
		}
	}
}

static void switcher_update(void *data, obs_data_t *settings)
{
	struct switcher_info *switcher = data;
	switcher->loop = obs_data_get_bool(settings, S_LOOP);
	obs_data_array_t *sources = obs_data_get_array(settings, S_SOURCES);
	if (sources) {
		for (size_t i = 0; i < switcher->sources.num; i++) {
			obs_source_release(switcher->sources.array[i]);
		}
		switcher->sources.num = 0;
		const size_t count = obs_data_array_count(sources);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(sources, i);
			const char *source_name =
				obs_data_get_string(item, "value");
			obs_source_t *source =
				obs_get_source_by_name(source_name);
			if (source) {
				da_push_back(switcher->sources, &source);
				bool found = false;
				for (size_t j = 0;
				     !found && j < switcher->hotkeys.num; j++) {
					if (source ==
					    switcher->hotkeys.array[j].source)
						found = true;
				}
				if (!found) {
					struct switcher_hotkey_info h;
					h.source = source;
					h.hotkey_id = obs_hotkey_register_source(
						switcher->source,
						obs_source_get_name(source),
						obs_source_get_name(source),
						switcher_switch_source_hotkey,
						switcher);
					da_push_back(switcher->hotkeys, &h);
				}
			}
			obs_data_release(item);
		}
		size_t i = 0;
		while (i < switcher->hotkeys.num) {
			bool found = false;
			for (size_t j = 0; !found && j < switcher->sources.num;
			     j++) {
				if (switcher->sources.array[j] ==
				    switcher->hotkeys.array[i].source)
					found = true;
			}
			if (found) {
				i++;
			} else {
				obs_hotkey_unregister(
					switcher->hotkeys.array[i].hotkey_id);
				da_erase(switcher->hotkeys, i);
			}
		}
		if (!switcher->sources.num) {
			switcher->current_index = 0;
			if (switcher->current_source) {
				obs_source_release(switcher->current_source);
				obs_source_remove_active_child(
					switcher->source,
					switcher->current_source);
				switcher->current_source = NULL;
			}
		} else {
			if (switcher->current_source) {
				for (size_t i = 0; i < switcher->sources.num;
				     i++) {
					if (switcher->current_source ==
					    switcher->sources.array[i]) {
						switcher->current_index = i;
						break;
					}
				}
			}
			switcher_index_changed(switcher);
		}
		obs_data_array_release(sources);
	}

	switcher->time_switch = obs_data_get_bool(settings, S_TIME_SWITCH);
	switcher->time_switch_duration =
		obs_data_get_int(settings, S_TIME_SWITCH_DURATION);
	switcher->time_switch_to = obs_data_get_int(settings, S_TIME_SWITCH_TO);

	switcher->media_state_switch =
		obs_data_get_bool(settings, S_MEDIA_STATE_SWITCH);
	switcher->media_switch_state =
		obs_data_get_int(settings, S_MEDIA_SWITCH_STATE);
	switcher->media_state_switch_to =
		obs_data_get_int(settings, S_MEDIA_STATE_SWITCH_TO);

	const char *transition_id = obs_data_get_string(settings, S_TRANSITION);
	if (!transition_id || !strlen(transition_id)) {
		obs_source_release(switcher->transition);
		switcher->transition = NULL;
	} else if (switcher->transition &&
		   strcmp(obs_source_get_id(switcher->transition),
			  transition_id) == 0) {
	} else {
		obs_source_release(switcher->transition);
		switcher->transition = obs_source_create_private(
			transition_id,
			obs_source_get_display_name(transition_id), settings);
	}
	if (switcher->transition) {
		obs_transition_set_alignment(
			switcher->transition,
			obs_data_get_int(settings, S_TRANSITION_ALIGNMENT));
		obs_transition_set_scale_type(
			switcher->transition,
			obs_data_get_int(settings, S_TRANSITION_SCALE));
		obs_source_update(switcher->transition, settings);
	}
	switcher->transition_duration =
		obs_data_get_int(settings, S_TRANSITION_DURATION);
	switcher->transition_resize =
		obs_data_get_bool(settings, S_TRANSITION_RESIZE);
}

static void *switcher_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	struct switcher_info *switcher = bzalloc(sizeof(struct switcher_info));
	switcher->source = source;
	da_init(switcher->sources);
	da_init(switcher->hotkeys);
	obs_hotkey_register_source(source, "none", obs_module_text("None"),
				   switcher_none_hotkey, switcher);
	obs_hotkey_register_source(source, "next", obs_module_text("Next"),
				   switcher_next_hotkey, switcher);
	obs_hotkey_register_source(source, "previous",
				   obs_module_text("Previous"),
				   switcher_previous_hotkey, switcher);
	obs_hotkey_register_source(source, "random", obs_module_text("Random"),
				   switcher_random_hotkey, switcher);
	obs_hotkey_register_source(source, "shuffle", obs_module_text("Shuffle"),
				   switcher_shuffle_hotkey, switcher);
	obs_hotkey_register_source(source, "first", obs_module_text("First"),
				   switcher_first_hotkey, switcher);
	obs_hotkey_register_source(source, "last", obs_module_text("Last"),
				   switcher_last_hotkey, switcher);
	signal_handler_connect(obs_get_signal_handler(), "source_rename",
			       switcher_source_rename, switcher);
	switcher_update(switcher, settings);
	return switcher;
}

static void switcher_destroy(void *data)
{
	struct switcher_info *switcher = data;
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename",
				  switcher_source_rename, switcher);
	if (switcher->current_source) {
		obs_source_release(switcher->current_source);
		obs_source_remove_active_child(switcher->source,
					       switcher->current_source);
		switcher->current_source = NULL;
	}
	for (size_t i = 0; i < switcher->sources.num; i++) {
		obs_source_release(switcher->sources.array[i]);
	}
	da_free(switcher->sources);
	da_free(switcher->hotkeys);
	obs_source_release(switcher->transition);
	bfree(switcher);
}

bool switcher_transition_active(obs_source_t *transition)
{
	if (!transition)
		return false;
	const float t = obs_transition_get_time(transition);
	return t >= 0.0f && t < 1.0f;
}

static void switcher_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct switcher_info *switcher = data;
	if (switcher_transition_active(switcher->transition)) {
		if (switcher->transition_resize) {
			obs_source_t *source_a = obs_transition_get_source(
				switcher->transition, OBS_TRANSITION_SOURCE_A);
			obs_source_t *source_b = obs_transition_get_source(
				switcher->transition, OBS_TRANSITION_SOURCE_B);
			uint32_t cxa = 0;
			uint32_t cya = 0;
			uint32_t cxb = 0;
			uint32_t cyb = 0;
			if (source_a) {
				cxa = obs_source_get_width(source_a);
				cya = obs_source_get_height(source_a);
			}
			if (source_b) {
				cxb = obs_source_get_width(source_b);
				cyb = obs_source_get_height(source_b);
			}
			const float t =
				obs_transition_get_time(switcher->transition);
			const uint32_t cx =
				(cxa && cxb)
					? (uint32_t)((1.0f - t) * (float)cxa +
						     t * (float)cxb)
					: cxa + cxb;
			const uint32_t cy =
				(cya && cyb)
					? (uint32_t)((1.0f - t) * (float)cya +
						     t * (float)cyb)
					: cya + cyb;
			obs_source_release(source_a);
			obs_source_release(source_b);
			obs_transition_set_size(switcher->transition, cx, cy);
		}
		obs_source_video_render(switcher->transition);
	} else {
		if (switcher->transition && switcher->transition_running) {
			const uint64_t t = obs_get_video_frame_time();
			if (t > switcher->last_switch_time &&
			    t - switcher->last_switch_time > 10000000UL)
				{ // wait 10 ms before start checking state
					switcher->transition_running = false;
					obs_source_remove_active_child(
						switcher->source,
						switcher->transition);
					obs_transition_force_stop(
						switcher->transition);
					obs_transition_clear(
						switcher->transition);
					if (switcher->current_source) {
						obs_source_video_render(
							switcher->current_source);
					}
			} else {
				obs_source_t *source =
					obs_transition_get_source(
						switcher->transition,
						OBS_TRANSITION_SOURCE_A);
				if (source) {
					obs_source_video_render(source);
					obs_source_release(source);
				}else {
					obs_source_video_render(
						switcher->transition);
				}
			}
		}else if (switcher->current_source) {
			obs_source_video_render(switcher->current_source);
		}
	}
}

static bool switcher_audio_render(void *data, uint64_t *ts_out,
				  struct obs_source_audio_mix *audio_output,
				  uint32_t mixers, size_t channels,
				  size_t sample_rate)
{
	UNUSED_PARAMETER(sample_rate);
	struct switcher_info *switcher = data;
	if (!switcher->current_source)
		return false;

	uint64_t timestamp = 0;

	if (!obs_source_audio_pending(switcher->current_source)) {
		timestamp = obs_source_get_audio_timestamp(
			switcher->current_source);

		struct obs_source_audio_mix child_audio;
		obs_source_get_audio_mix(switcher->current_source,
					 &child_audio);
		for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
			if ((mixers & (1 << mix)) == 0)
				continue;

			for (size_t ch = 0; ch < channels; ch++) {
				audio_output->output[mix].data[ch] =
					child_audio.output[mix].data[ch];
			}
		}
	}
	*ts_out = timestamp;
	return true;
}

void prop_list_add_switch_to(obs_property_t *p)
{
	obs_property_list_add_int(p, obs_module_text("None"), SWITCH_NONE);
	obs_property_list_add_int(p, obs_module_text("Next"), SWITCH_NEXT);
	obs_property_list_add_int(p, obs_module_text("Previous"),
				  SWITCH_PREVIOUS);
	obs_property_list_add_int(p, obs_module_text("First"), SWITCH_FIRST);
	obs_property_list_add_int(p, obs_module_text("Last"), SWITCH_LAST);
	obs_property_list_add_int(p, obs_module_text("Random"), SWITCH_RANDOM);
}

void prop_list_add_scales(obs_property_t *p)
{
	obs_property_list_add_int(p, obs_module_text("TransitionScale.MaxOnly"),
				  OBS_TRANSITION_SCALE_MAX_ONLY);
	obs_property_list_add_int(p, obs_module_text("TransitionScale.Aspect"),
				  OBS_TRANSITION_SCALE_ASPECT);
	obs_property_list_add_int(p, obs_module_text("TransitionScale.Stretch"),
				  OBS_TRANSITION_SCALE_STRETCH);
}

bool remove_prop(obs_properties_t *props, const char *name)
{
	obs_property_t *p = obs_properties_get(props, name);
	if (p) {
		obs_properties_remove_by_name(props, name);
		return true;
	}
	return false;
}
bool switcher_transition_changed(void *data, obs_properties_t *props,
				 obs_property_t *property, obs_data_t *settings)
{
	struct switcher_info *switcher = data;
	bool changed_props = false;
	const char *transition_id = obs_data_get_string(settings, S_TRANSITION);
	obs_properties_t *transition_group = obs_property_group_content(
		obs_properties_get(props, S_TRANSITION_GROUP));
	if (!transition_id || !strlen(transition_id)) {
		if (switcher->transition) {
			obs_source_release(switcher->transition);
			switcher->transition = NULL;
		}
		changed_props |=
			remove_prop(transition_group, S_TRANSITION_PROPERTIES);
		changed_props |=
			remove_prop(transition_group, S_TRANSITION_DURATION);
		changed_props |=
			remove_prop(transition_group, S_TRANSITION_SCALE);
		changed_props |=
			remove_prop(transition_group, S_TRANSITION_RESIZE);
		changed_props |=
			remove_prop(transition_group, S_TRANSITION_ALIGNMENT);
		return changed_props;
	}
	if (!switcher->transition ||
	    strcmp(obs_source_get_id(switcher->transition), transition_id) !=
		    0) {
		obs_source_release(switcher->transition);
		switcher->transition = obs_source_create_private(
			transition_id,
			obs_source_get_display_name(transition_id), settings);
		obs_transition_set_alignment(
			switcher->transition,
			obs_data_get_int(settings, S_TRANSITION_ALIGNMENT));
		obs_transition_set_scale_type(
			switcher->transition,
			obs_data_get_int(settings, S_TRANSITION_SCALE));
	}

	obs_property_t *p =
		obs_properties_get(transition_group, S_TRANSITION_DURATION);
	if (obs_transition_fixed(switcher->transition)) {
		changed_props |=
			remove_prop(transition_group, S_TRANSITION_DURATION);
	} else if (!p) {
		p = obs_properties_add_int(transition_group,
					   S_TRANSITION_DURATION,
					   obs_module_text("Duration"), 50,
					   10000, 100);
		obs_property_int_set_suffix(p, "ms");
		changed_props = true;
	}
	p = obs_properties_get(transition_group, S_TRANSITION_SCALE);
	if (!p) {

		p = obs_properties_add_list(
			transition_group, S_TRANSITION_SCALE,
			obs_module_text("TransitionScaleType"),
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
		prop_list_add_scales(p);
	}
	p = obs_properties_get(transition_group, S_TRANSITION_RESIZE);
	if (!p) {
		p = obs_properties_add_bool(transition_group,
					    S_TRANSITION_RESIZE,
					    obs_module_text("Resize"));
	}
	p = obs_properties_get(transition_group, S_TRANSITION_ALIGNMENT);
	if (!p) {
		p = obs_properties_add_list(transition_group,
					    S_TRANSITION_ALIGNMENT,
					    obs_module_text("Alignment"),
					    OBS_COMBO_TYPE_LIST,
					    OBS_COMBO_FORMAT_INT);
		obs_property_list_add_int(p, obs_module_text("TopLeft"),
					  OBS_ALIGN_TOP | OBS_ALIGN_LEFT);
		obs_property_list_add_int(p, obs_module_text("Top"),
					  OBS_ALIGN_TOP);
		obs_property_list_add_int(p, obs_module_text("TopRight"),
					  OBS_ALIGN_TOP | OBS_ALIGN_RIGHT);
		obs_property_list_add_int(p, obs_module_text("Left"),
					  OBS_ALIGN_LEFT);
		obs_property_list_add_int(p, obs_module_text("Center"),
					  OBS_ALIGN_CENTER);
		obs_property_list_add_int(p, obs_module_text("Right"),
					  OBS_ALIGN_RIGHT);
		obs_property_list_add_int(p, obs_module_text("BottomLeft"),
					  OBS_ALIGN_BOTTOM | OBS_ALIGN_LEFT);
		obs_property_list_add_int(p, obs_module_text("Bottom"),
					  OBS_ALIGN_BOTTOM);
		obs_property_list_add_int(p, obs_module_text("BottomRight"),
					  OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT);
	}

	if (obs_is_source_configurable(transition_id)) {
		obs_properties_remove_by_name(transition_group,
					      S_TRANSITION_PROPERTIES);
		obs_properties_add_group(
			transition_group, S_TRANSITION_PROPERTIES,
			obs_module_text("Properties"), OBS_GROUP_NORMAL,
			obs_source_properties(switcher->transition));
		return true;
	}
	changed_props |= remove_prop(transition_group, S_TRANSITION_PROPERTIES);
	return changed_props;
}

static obs_properties_t *switcher_properties(void *data)
{
	obs_property_t *p;
	obs_properties_t *ppts = obs_properties_create();
	obs_properties_add_editable_list(ppts, S_SOURCES,
					 obs_module_text("Sources"),
					 OBS_EDITABLE_LIST_TYPE_STRINGS, NULL,
					 NULL);
	obs_properties_add_bool(ppts, S_LOOP, obs_module_text("Loop"));

	obs_properties_t *tsppts = obs_properties_create();
	p = obs_properties_add_int(tsppts, S_TIME_SWITCH_DURATION,
				   obs_module_text("Duration"), 50, 1000000UL,
				   1000);
	obs_property_int_set_suffix(p, "ms");
	p = obs_properties_add_list(tsppts, S_TIME_SWITCH_TO,
				    obs_module_text("SwitchTo"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	prop_list_add_switch_to(p);
	obs_properties_add_group(ppts, S_TIME_SWITCH,
				 obs_module_text("TimeSwitch"),
				 OBS_GROUP_CHECKABLE, tsppts);

	obs_properties_t *mssppts = obs_properties_create();
	p = obs_properties_add_list(mssppts, S_MEDIA_SWITCH_STATE,
				    obs_module_text("MediaState"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("Playing"),
				  OBS_MEDIA_STATE_PLAYING);
	obs_property_list_add_int(p, obs_module_text("Opening"),
				  OBS_MEDIA_STATE_OPENING);
	obs_property_list_add_int(p, obs_module_text("Buffering"),
				  OBS_MEDIA_STATE_BUFFERING);
	obs_property_list_add_int(p, obs_module_text("Paused"),
				  OBS_MEDIA_STATE_PAUSED);
	obs_property_list_add_int(p, obs_module_text("Stopped"),
				  OBS_MEDIA_STATE_STOPPED);
	obs_property_list_add_int(p, obs_module_text("Ended"),
				  OBS_MEDIA_STATE_ENDED);
	obs_property_list_add_int(p, obs_module_text("Error"),
				  OBS_MEDIA_STATE_ERROR);
	obs_property_list_add_int(p, obs_module_text("NotPlaying"),
				  -OBS_MEDIA_STATE_PLAYING);
	obs_property_list_add_int(p, obs_module_text("NotOpening"),
				  -OBS_MEDIA_STATE_OPENING);
	obs_property_list_add_int(p, obs_module_text("NotBuffering"),
				  -OBS_MEDIA_STATE_BUFFERING);
	obs_property_list_add_int(p, obs_module_text("NotPaused"),
				  -OBS_MEDIA_STATE_PAUSED);
	obs_property_list_add_int(p, obs_module_text("NotStopped"),
				  -OBS_MEDIA_STATE_STOPPED);
	obs_property_list_add_int(p, obs_module_text("NotEnded"),
				  -OBS_MEDIA_STATE_ENDED);
	obs_property_list_add_int(p, obs_module_text("NotError"),
				  -OBS_MEDIA_STATE_ERROR);

	p = obs_properties_add_list(mssppts, S_MEDIA_STATE_SWITCH_TO,
				    obs_module_text("SwitchTo"),
				    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	prop_list_add_switch_to(p);
	obs_properties_add_group(ppts, S_MEDIA_STATE_SWITCH,
				 obs_module_text("MediaStateSwitch"),
				 OBS_GROUP_CHECKABLE, mssppts);

	obs_properties_t *transition_group = obs_properties_create();

	p = obs_properties_add_list(transition_group, S_TRANSITION,
				    obs_module_text("TransitionType"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

	obs_property_set_modified_callback2(p, switcher_transition_changed,
					    data);
	obs_property_list_add_string(p, obs_module_text("None"), "");
	size_t idx = 0;
	const char *id;
	while (obs_enum_transition_types(idx++, &id)) {
		const char *name = obs_source_get_display_name(id);
		obs_property_list_add_string(p, name, id);
	}
	obs_properties_add_group(ppts, S_TRANSITION_GROUP,
				 obs_module_text("Transition"),
				 OBS_GROUP_NORMAL, transition_group);

	return ppts;
}

void switcher_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_LOOP, true);

	obs_data_set_default_int(settings, S_TIME_SWITCH_DURATION, 5000);
	obs_data_set_default_int(settings, S_TIME_SWITCH_TO, SWITCH_NEXT);

	obs_data_set_default_int(settings, S_MEDIA_SWITCH_STATE,
				 OBS_MEDIA_STATE_STOPPED);
	obs_data_set_default_int(settings, S_MEDIA_STATE_SWITCH_TO,
				 SWITCH_NEXT);

	obs_data_set_default_int(settings, S_TRANSITION_DURATION, 1000);
	obs_data_set_default_int(settings, S_TRANSITION_SCALE,
				 OBS_TRANSITION_SCALE_ASPECT);
	obs_data_set_default_int(settings, S_TRANSITION_ALIGNMENT,
				 OBS_ALIGN_CENTER);
	obs_data_set_default_bool(settings, S_TRANSITION_RESIZE, true);
}

uint32_t switcher_get_width(void *data)
{
	struct switcher_info *switcher = data;
	if (switcher_transition_active(switcher->transition)) {
		if (switcher->transition_resize) {
			obs_source_t *source_a = obs_transition_get_source(
				switcher->transition, OBS_TRANSITION_SOURCE_A);
			obs_source_t *source_b = obs_transition_get_source(
				switcher->transition, OBS_TRANSITION_SOURCE_B);
			uint32_t cxa = 0;
			uint32_t cxb = 0;
			if (source_a) {
				cxa = obs_source_get_width(source_a);
			}
			if (source_b) {
				cxb = obs_source_get_width(source_b);
			}
			const float t =
				obs_transition_get_time(switcher->transition);
			const uint32_t cx =
				(cxa && cxb)
					? (uint32_t)((1.0f - t) * (float)cxa +
						     t * (float)cxb)
					: cxa + cxb;
			obs_source_release(source_a);
			obs_source_release(source_b);
			return cx;
		}
		return obs_source_get_width(switcher->transition);
	}
	if (switcher->current_source)
		return obs_source_get_width(switcher->current_source);
	return 0;
}

uint32_t switcher_get_height(void *data)
{
	struct switcher_info *switcher = data;
	if (switcher_transition_active(switcher->transition)) {
		if (switcher->transition_resize) {
			obs_source_t *source_a = obs_transition_get_source(
				switcher->transition, OBS_TRANSITION_SOURCE_A);
			obs_source_t *source_b = obs_transition_get_source(
				switcher->transition, OBS_TRANSITION_SOURCE_B);
			uint32_t cya = 0;
			uint32_t cyb = 0;
			if (source_a) {
				cya = obs_source_get_height(source_a);
			}
			if (source_b) {
				cyb = obs_source_get_height(source_b);
			}
			const float t =
				obs_transition_get_time(switcher->transition);
			const uint32_t cy =
				(cya && cyb)
					? (uint32_t)((1.0f - t) * (float)cya +
						     t * (float)cyb)
					: cya + cyb;
			obs_source_release(source_a);
			obs_source_release(source_b);
			return cy;
		}
		return obs_source_get_height(switcher->transition);
	}
	if (switcher->current_source)
		return obs_source_get_height(switcher->current_source);
	return 0;
}

static void switcher_enum_active_sources(void *data,
					 obs_source_enum_proc_t enum_callback,
					 void *param)
{
	struct switcher_info *switcher = data;
	if (switcher_transition_active(switcher->transition))
		enum_callback(switcher->source, switcher->transition, param);
	if (switcher->current_source)
		enum_callback(switcher->source, switcher->current_source,
			      param);
}

static void switcher_enum_all_sources(void *data,
				      obs_source_enum_proc_t enum_callback,
				      void *param)
{
	struct switcher_info *switcher = data;
	bool current_found = false;
	for (size_t i = 0; i < switcher->sources.num; i++) {
		if (switcher->sources.array[i] == switcher->current_source)
			current_found = true;
		enum_callback(switcher->source, switcher->sources.array[i],
			      param);
	}
	if (!current_found && switcher->current_source) {
		enum_callback(switcher->source, switcher->current_source,
			      param);
	}
	if (switcher->transition) {
		enum_callback(switcher->source, switcher->transition, param);
	}
}

void switcher_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct switcher_info *switcher = data;
	if (switcher->time_switch) {
		const uint64_t t = obs_get_video_frame_time();
		if (t > switcher->last_switch_time &&
		    t - switcher->last_switch_time >
			    switcher->time_switch_duration * 1000000UL) {
			switcher_switch_to(switcher, switcher->time_switch_to);
		}
	}
	if (switcher->media_state_switch && switcher->current_source) {
		const uint64_t t = obs_get_video_frame_time();
		const enum obs_media_state state =
			obs_source_media_get_state(switcher->current_source);
		if (state != OBS_MEDIA_STATE_NONE &&
		    (t < switcher->last_switch_time ||
		     t - switcher->last_switch_time >
			     10000000UL)) { // wait 10 ms before start checking state
			if (switcher->media_switch_state < 0) {
				if (-switcher->media_switch_state !=
				    (int32_t)state) {
					switcher_switch_to(
						switcher,
						switcher->media_state_switch_to);
				}
			} else if (switcher->media_switch_state ==
				   (int32_t)state) {
				switcher_switch_to(
					switcher,
					switcher->media_state_switch_to);
			} else if (state == OBS_MEDIA_STATE_PLAYING &&
				   switcher->media_switch_state ==
					    OBS_MEDIA_STATE_ENDED && switcher->transition && !switcher->transition_running) {
				const int64_t duration = obs_source_media_get_duration(
					switcher->current_source);
				if (duration) {
					const int64_t time = obs_source_media_get_time(
						switcher->current_source);
					if (time <= duration && duration - time < switcher->transition_duration) {
						switcher_switch_to(
							switcher,
							switcher->media_state_switch_to);
					}
				}
			}
		}
	}
}

void switch_save(void *data, obs_data_t *settings)
{
	struct switcher_info *switcher = data;
	if (switcher->current_source) {
		obs_data_set_int(settings, "current_index",
				 switcher->current_index);
	} else {
		obs_data_set_int(settings, "current_index", -1);
	}
}

void switcher_load(void *data, obs_data_t *settings)
{
	struct switcher_info *switcher = data;
	const long long index = obs_data_get_int(settings, "current_index");
	if (index >= 0) {
		switcher->current_index = index;
		switcher_update(data, settings);
	} else {
		switcher_update(data, settings);
		switcher_switch_to(switcher, SWITCH_NONE);
	}
}

struct obs_source_info source_switcher = {
	.id = "source_switcher",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_OUTPUT_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			OBS_SOURCE_COMPOSITE | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = switcher_get_name,
	.create = switcher_create,
	.destroy = switcher_destroy,
	.update = switcher_update,
	.video_render = switcher_video_render,
	.audio_render = switcher_audio_render,
	.get_properties = switcher_properties,
	.get_defaults = switcher_defaults,
	.get_width = switcher_get_width,
	.get_height = switcher_get_height,
	.enum_active_sources = switcher_enum_active_sources,
	.enum_all_sources = switcher_enum_all_sources,
	.video_tick = switcher_video_tick,
	.save = switch_save,
	.load = switcher_load,
	.icon_type = OBS_ICON_TYPE_SLIDESHOW,
};

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("source-switcher", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("SourceSwitcher");
}

bool obs_module_load(void)
{
	obs_register_source(&source_switcher);
	return true;
}
