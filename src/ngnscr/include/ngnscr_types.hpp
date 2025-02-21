#ifndef NGNSCR_TYPES_INCLUDED
#define NGNSCR_TYPES_INCLUDED

#include <cppext_memory.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>

class asIScriptContext;
class asIScriptObject;

namespace ngnscr
{
    using script_object_ptr_t = boost::intrusive_ptr<asIScriptObject>;

    using script_context_ptr_t =
        cppext::unique_ptr_with_deleter_t<asIScriptContext>;
} // namespace ngnscr

void intrusive_ptr_add_ref(asIScriptObject const* p);

void intrusive_ptr_release(asIScriptObject const* p);

#endif // !NGNSCR_TYPES_INCLUDED
