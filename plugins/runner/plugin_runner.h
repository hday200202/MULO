#pragma once
#include <stdint.h>
#include "plugins/api/plugin_api.h"

extern "C" {
int   pr_load(const char* so_path);   // e.g., /plugins/foo.so
int   pr_unload();
void* pr_create(const Mdaw_PluginConfig* cfg);
void  pr_destroy(void* h);
int   pr_process(void* h, const float* in_i, float* out_i, uint32_t frames);
int   pr_set_param(void* h, uint32_t idx, float value);
float pr_get_param(void* h, uint32_t idx);
void  pr_get_info(Mdaw_PluginInfo* out);
}
