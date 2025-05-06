#ifndef NGNTXT_SHAPING_INCLUDED
#define NGNTXT_SHAPING_INCLUDED

#include <ngntxt_font_face.hpp>

#include <cppext_memory.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <hb.h>

namespace ngntxt
{
    using shaping_font_face_ptr_t = boost::intrusive_ptr<hb_font_t>;

    using shaping_buffer_ptr_t =
        cppext::unique_ptr_with_static_deleter_t<hb_buffer_t,
            hb_buffer_destroy>;

    [[nodiscard]] shaping_font_face_ptr_t create_shaping_font_face(
        ngntxt::font_face_ptr_t face);

    [[nodiscard]] shaping_buffer_ptr_t create_shaping_buffer();
} // namespace ngntxt

void intrusive_ptr_add_ref(hb_font_t* p);

void intrusive_ptr_release(hb_font_t* p);

#endif
