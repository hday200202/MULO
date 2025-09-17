#include "plugins/api/plugin_api.h"

struct State { float gain = 1.0f; uint32_t ch=0; };

extern "C" {
void* mdaw_plugin_create(const Mdaw_PluginConfig* cfg) {
  auto* s = new State(); s->ch = cfg->num_channels; return s;
}
void  mdaw_plugin_destroy(void* h) { delete static_cast<State*>(h); }

int mdaw_plugin_process(void* h, const float* in_i, float* out_i, uint32_t frames) {
  auto* s = static_cast<State*>(h);
  const uint64_t n = uint64_t(frames) * s->ch;
  for (uint64_t i=0;i<n;++i) out_i[i] = in_i[i] * s->gain;
  return 0;
}
int mdaw_plugin_set_param(void* h, uint32_t idx, float v) {
    auto* s = static_cast<State*>(h);
    if (!s) return -1;

    switch (idx) {
        case 0: s->gain = v; return 0;
        default: return -1;
    }
}

float mdaw_plugin_get_param(void* h, uint32_t idx) {
  return (idx==0) ? static_cast<State*>(h)->gain : 0.0f;
}
void  mdaw_plugin_get_info(Mdaw_PluginInfo* out) {
  out->name="Gain"; out->vendor="MDAW"; out->version=1; out->num_params=1;
}
}
