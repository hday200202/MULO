#include "plugins/runner/plugin_runner.h"
#include <dlfcn.h>
#include <string>

namespace {
void* g_handle = nullptr;

using create_fn_t   = void* (*)(const Mdaw_PluginConfig*);
using destroy_fn_t  = void  (*)(void*);
using process_fn_t  = int   (*)(void*, const float*, float*, uint32_t);
using setp_fn_t     = int   (*)(void*, uint32_t, float);
using getp_fn_t     = float (*)(void*, uint32_t);
using getinfo_fn_t  = void  (*)(Mdaw_PluginInfo*);

create_fn_t   p_create  = nullptr;
destroy_fn_t  p_destroy = nullptr;
process_fn_t  p_process = nullptr;
setp_fn_t     p_setp    = nullptr;
getp_fn_t     p_getp    = nullptr;
getinfo_fn_t  p_info    = nullptr;

template <typename T>
bool LoadSym(T& out, void* h, const char* name) {
  void* s = dlsym(h, name);
  if (!s) return false;
  out = reinterpret_cast<T>(s);
  return true;
}
} // namespace

extern "C" {

int pr_load(const char* so_path) {
  if (g_handle) return 0; // already loaded
  g_handle = dlopen(so_path, RTLD_NOW);
  if (!g_handle) return -1;

  if (!LoadSym(p_create,  g_handle, "mdaw_plugin_create") ||
      !LoadSym(p_destroy, g_handle, "mdaw_plugin_destroy") ||
      !LoadSym(p_process, g_handle, "mdaw_plugin_process") ||
      !LoadSym(p_setp,    g_handle, "mdaw_plugin_set_param") ||
      !LoadSym(p_getp,    g_handle, "mdaw_plugin_get_param") ||
      !LoadSym(p_info,    g_handle, "mdaw_plugin_get_info")) {
    dlclose(g_handle); g_handle = nullptr;
    return -2;
  }
  return 0;
}

int pr_unload() {
  if (!g_handle) return 0;
  dlclose(g_handle);
  g_handle = nullptr;
  p_create = nullptr; p_destroy = nullptr; p_process = nullptr;
  p_setp = nullptr; p_getp = nullptr; p_info = nullptr;
  return 0;
}

void* pr_create(const Mdaw_PluginConfig* cfg) {
  return p_create ? p_create(cfg) : nullptr;
}
void  pr_destroy(void* h) {
  if (p_destroy) p_destroy(h);
}
int   pr_process(void* h, const float* in_i, float* out_i, uint32_t frames) {
  return p_process ? p_process(h, in_i, out_i, frames) : -1;
}
int   pr_set_param(void* h, uint32_t idx, float v) {
  return p_setp ? p_setp(h, idx, v) : -1;
}
float pr_get_param(void* h, uint32_t idx) {
  return p_getp ? p_getp(h, idx) : 0.0f;
}
void  pr_get_info(Mdaw_PluginInfo* out) {
  if (p_info) p_info(out);
}
}
