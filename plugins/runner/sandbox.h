#pragma once
#include <memory>
#include <string>
#include "sandboxed_api/sandbox.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"

class PluginSandbox : public ::sapi::Sandbox {
 public:
  explicit PluginSandbox(std::string plugin_dir_ro)
      : plugin_dir_ro_(std::move(plugin_dir_ro)) {}

 protected:
  std::unique_ptr<sandbox2::Policy> ModifyPolicy(
      sandbox2::PolicyBuilder* b) override {
    return b->AllowExit()
            ->AllowDynamicStartup()      // dyn loader/bootstrap
            ->AddDirectoryAt(plugin_dir_ro_, "/plugins", /*is_ro=*/true)
            ->AddTmpfs("/tmp", /*size_mb=*/16)
            ->BuildOrDie();
  }
 private:
  std::string plugin_dir_ro_;
};
