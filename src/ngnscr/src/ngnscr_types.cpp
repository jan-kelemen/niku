#include <ngnscr_types.hpp>

#include <angelscript.h>

void intrusive_ptr_add_ref(asIScriptObject const* const p) { p->AddRef(); }

void intrusive_ptr_release(asIScriptObject const* const p) { p->Release(); }
