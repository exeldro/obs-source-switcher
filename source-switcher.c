#include <obs-module.h>
#include "source-switcher.h"

struct switcher_info {
	obs_source_t *source;
	obs_source_t *current_source;
	DARRAY(obs_source_t *) sources;
	size_t current_index;
	bool loop;
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

void switcher_none_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			  bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed || !switcher->current_source)
		return;

	obs_source_release(switcher->current_source);
	obs_source_remove_active_child(switcher->source,
				       switcher->current_source);
	switcher->current_source = NULL;
}

void switcher_index_changed(struct switcher_info *switcher)
{
	if (!switcher->sources.num)
		return;

	if (switcher->current_index >= switcher->sources.num) {
		switcher->current_index =
			switcher->loop ? 0 : switcher->sources.num - 1;
	}
	if (switcher->current_source !=
	    switcher->sources.array[switcher->current_index]) {
		if (switcher->current_source) {
			obs_source_release(switcher->current_source);
			obs_source_remove_active_child(
				switcher->source, switcher->current_source);
		}
		switcher->current_source =
			switcher->sources.array[switcher->current_index];
		obs_source_addref(switcher->current_source);
		obs_source_add_active_child(switcher->source,
					    switcher->current_source);
	}
}

void switcher_next_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			  bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	switcher->current_index++;
	switcher_index_changed(switcher);
}

void switcher_previous_hotkey(void *data, obs_hotkey_id id,
			      obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed)
		return;
	if (!switcher->current_index) {
		if (switcher->loop && switcher->sources.num) {
			switcher->current_index = switcher->sources.num - 1;
		}
	} else {
		switcher->current_index--;
	}
	switcher_index_changed(switcher);
}

void switcher_random_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			    bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed || !switcher->sources.num)
		return;
	if (switcher->sources.num == 1) {
		switcher->current_index = 0;
	} else {
		if (switcher->current_index < switcher->sources.num) {
			const size_t r =
				(size_t)rand() % (switcher->sources.num - 1);
			if (r < switcher->current_index)
				switcher->current_index = r;
			else
				switcher->current_index = r + 1;
		} else {
			switcher->current_index =
				(size_t)rand() % switcher->sources.num;
		}
	}
	switcher_index_changed(switcher);
}

void switcher_first_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			   bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed || !switcher->sources.num)
		return;
	switcher->current_index = 0;
	switcher_index_changed(switcher);
}

void switcher_last_hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
			  bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct switcher_info *switcher = data;
	if (!pressed || !switcher->sources.num)
		return;
	switcher->current_index = switcher->sources.num - 1;
	switcher_index_changed(switcher);
}

static void *switcher_create(obs_data_t *settings, obs_source_t *source)
{
	struct switcher_info *switcher = bzalloc(sizeof(struct switcher_info));
	switcher->source = source;
	obs_hotkey_register_source(source, "none", obs_module_text("None"),
				   switcher_none_hotkey, switcher);
	obs_hotkey_register_source(source, "next", obs_module_text("Next"),
				   switcher_next_hotkey, switcher);
	obs_hotkey_register_source(source, "previous",
				   obs_module_text("Previous"),
				   switcher_previous_hotkey, switcher);
	obs_hotkey_register_source(source, "random", obs_module_text("Random"),
				   switcher_random_hotkey, switcher);
	obs_hotkey_register_source(source, "first", obs_module_text("First"),
				   switcher_first_hotkey, switcher);
	obs_hotkey_register_source(source, "last", obs_module_text("Last"),
				   switcher_last_hotkey, switcher);
	signal_handler_connect(obs_get_signal_handler(), "source_rename",
			       switcher_source_rename, switcher);
	obs_source_update(source, settings);
	return switcher;
}

static void switcher_destroy(void *data)
{
	struct switcher_info *switcher = data;
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
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename",
				  switcher_source_rename, switcher);
	bfree(switcher);
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
		size_t count = obs_data_array_count(sources);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *item = obs_data_array_item(sources, i);
			const char *source_name =
				obs_data_get_string(item, "value");
			obs_source_t *source =
				obs_get_source_by_name(source_name);
			if (source) {
				da_push_back(switcher->sources, &source);
			}
			obs_data_release(item);
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
			switcher_index_changed(switcher);
		}
		obs_data_array_release(sources);
	}
}

static void switcher_video_render(void *data, gs_effect_t *effect)
{
	struct switcher_info *switcher = data;
	if (switcher->current_source) {
		obs_source_video_render(switcher->current_source);
	}
}

static bool switcher_audio_render(void *data, uint64_t *ts_out,
				  struct obs_source_audio_mix *audio_output,
				  uint32_t mixers, size_t channels,
				  size_t sample_rate)
{
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

static obs_properties_t *switcher_properties(void *data)
{

	obs_properties_t *ppts = obs_properties_create();
	obs_properties_add_editable_list(ppts, S_SOURCES,
					 obs_module_text("Sources"),
					 OBS_EDITABLE_LIST_TYPE_STRINGS, NULL,
					 NULL);
	obs_properties_add_bool(ppts, S_LOOP, obs_module_text("Loop"));
	return ppts;
}

void switcher_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_LOOP, true);
}

uint32_t switcher_get_width(void *data)
{
	struct switcher_info *switcher = data;
	if (switcher->current_source)
		return obs_source_get_width(switcher->current_source);
	return 0;
}

uint32_t switcher_get_height(void *data)
{
	struct switcher_info *switcher = data;
	if (switcher->current_source)
		return obs_source_get_height(switcher->current_source);
	return 0;
}

static void switcher_enum_active_sources(void *data,
					 obs_source_enum_proc_t enum_callback,
					 void *param)
{
	struct switcher_info *switcher = data;
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
}

struct obs_source_info source_switcher = {
	.id = "source_switcher",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_OUTPUT_VIDEO | OBS_SOURCE_COMPOSITE,
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
