#include <ngntxt_shaping.hpp>

#include <hb-ft.h>

ngntxt::shaping_font_face_ptr_t ngntxt::create_shaping_font_face(
    font_face_ptr_t face)
{
    return hb_ft_font_create_referenced(*face);
}

ngntxt::shaping_buffer_ptr_t ngntxt::create_shaping_buffer()
{
    return ngntxt::shaping_buffer_ptr_t{hb_buffer_create()};
}

void intrusive_ptr_add_ref(hb_font_t* const p) { hb_font_reference(p); }

void intrusive_ptr_release(hb_font_t* const p) { hb_font_destroy(p); }
