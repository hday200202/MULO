#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t sample_rate, block_size, num_channels;
} Mdaw_PluginConfig;

typedef struct {
  const char* name;
  const char* vendor;
  uint32_t    version;
  uint32_t    num_params;
} Mdaw_PluginInfo;

// Lifecycle
void* mdaw_plugin_create(const Mdaw_PluginConfig* cfg);
void  mdaw_plugin_destroy(void* handle);

// Audio processing (interleaved)
int   mdaw_plugin_process(void* handle,
                          const float* in_interleaved,
                          float* out_interleaved,
                          uint32_t frames);

// Params + Info
int   mdaw_plugin_set_param(void* handle, uint32_t idx, float value);
float mdaw_plugin_get_param(void* handle, uint32_t idx);
void  mdaw_plugin_get_info(Mdaw_PluginInfo* out_info);

#ifdef __cplusplus
}
#endif
