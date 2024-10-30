#include <vkglsl_guard.hpp>

#include <glslang/Public/ShaderLang.h>

vkglsl::guard_t::guard_t() { glslang::InitializeProcess(); }

vkglsl::guard_t::~guard_t() { glslang::FinalizeProcess(); }
