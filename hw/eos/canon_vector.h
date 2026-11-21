/* Canon tagged vector resources used by Zico's vector-object command. */
static void eos_draw_canon_vector(uint32_t object, uint32_t destination)
{
    uint8_t descriptor[0xC0];
    if (object < 0x10000 || object > 0x3FFFFF40) return;
    eos_mem_read(object, descriptor, sizeof(descriptor));
    uint32_t resource = ldl_le_p(descriptor + 0x10);
    uint32_t header[3];
    if (resource < 0xFC000000) return;
    eos_mem_read(resource, header, sizeof(header));
    unsigned int length = le32_to_cpu(header[1]);
    if ((le32_to_cpu(header[0]) & 65535) != 0x9999 || length < 0x48 ||
        length > 0x100000 || (uint64_t)resource + length > 0x100000000ULL) return;
    uint8_t *data = g_malloc(length);
    eos_mem_read(resource, data, length);
    double width = ldl_le_p(data + 0x24) / 1048576.0;
    double height = ldl_le_p(data + 0x28) / 1048576.0;
    float cx, cy;
    uint32_t bits = ldl_le_p(descriptor + 0x4C);
    memcpy(&cx, &bits, 4);
    bits = ldl_le_p(descriptor + 0x50);
    memcpy(&cy, &bits, 4);
    unsigned int dw = lduw_le_p(descriptor + 0x54);
    unsigned int dh = lduw_le_p(descriptor + 0x56);
    if (!(width > 0 && height > 0 && dw > 0 && dh > 0 && dw <= 1920 && dh <= 1080 &&
          cx >= -1920 && cx <= 1920 && cy >= -1080 && cy <= 1080)) { g_free(data); return; }
    uint32_t *pixels = g_malloc(960 * 540 * 4);
    eos_mem_read(destination, pixels, 960 * 540 * 4);
    cairo_surface_t *surface = cairo_image_surface_create_for_data((void *)pixels, CAIRO_FORMAT_ARGB32, 960, 540, 960 * 4);
    cairo_t *cr = cairo_create(surface);
    cairo_rectangle(cr, ldl_le_p(descriptor + 0x9C), ldl_le_p(descriptor + 0xA0),
                    ldl_le_p(descriptor + 0xA4), ldl_le_p(descriptor + 0xA8));
    cairo_clip(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, dw / width, dh / height);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_path_t *paths[1024] = {0};
    uint32_t paints[256] = {0};
    unsigned int path_count = 0, paint_count = 0;
    unsigned int commands = 0, count = 0;
    for (unsigned int p = 0x48; p + 6 <= length; p += 2) {
        unsigned int tag = lduw_le_p(data + p), size = lduw_le_p(data + p + 2);
        if (size < 6 || size > length - p) continue;
        if (tag == 0xBBB1 && size == 8 && paint_count < ARRAY_SIZE(paints)) {
            paints[paint_count++] = ldl_le_p(data + p + 4);
        } else if (tag == 0xAAAC && size == 12) {
            unsigned int paint = lduw_le_p(data + p + 6);
            unsigned int path = ldl_le_p(data + p + 8);
            if (paint && paint <= paint_count && path && path <= path_count) {
                uint32_t color = paints[paint - 1];
                cairo_set_source_rgba(cr, ((color >> 8) & 255) / 255.0,
                                      ((color >> 16) & 255) / 255.0,
                                      ((color >> 24) & 255) / 255.0, (color & 255) / 255.0);
                cairo_append_path(cr, paths[path - 1]);
                cairo_fill(cr);
            }
        } else if (tag == 0xAAA0) {
            count = lduw_le_p(data + p + 4);
            commands = count <= size - 6 ? p + 6 : 0;
        } else if (tag == 0xAAA1 && commands) {
            unsigned int n = lduw_le_p(data + p + 4), index = 0;
            if (n > (size - 6) / 2) break;
            for (unsigned int j = 0; j < count; j++) {
                unsigned int op = data[commands + j], kind = op & ~1;
                unsigned int needed = kind == 0 ? 0 : kind == 6 || kind == 8 ? 1 : kind == 10 ? 4 : kind == 12 ? 6 : 2;
                if (needed > n - index || kind > 12) break;
                double v[6], x = 0, y = 0;
                if (cairo_has_current_point(cr)) cairo_get_current_point(cr, &x, &y);
                for (unsigned int k = 0; k < needed; k++) v[k] = (int16_t)lduw_le_p(data + p + 6 + 2 * index++) / 16.0;
                if (op & 1) for (unsigned int k = 0; k < needed; k++) v[k] += kind == 8 ? y : (k & 1) ? y : x;
                switch (kind) {
                    case 0: cairo_close_path(cr); break;
                    case 2: cairo_move_to(cr, v[0], v[1]); break;
                    case 4: cairo_line_to(cr, v[0], v[1]); break;
                    case 6: cairo_line_to(cr, v[0], y); break;
                    case 8: cairo_line_to(cr, x, v[0]); break;
                    case 10: cairo_curve_to(cr, x + (v[0]-x)*2/3, y + (v[1]-y)*2/3,
                                           v[2] + (v[0]-v[2])*2/3, v[3] + (v[1]-v[3])*2/3, v[2], v[3]); break;
                    case 12: cairo_curve_to(cr, v[0], v[1], v[2], v[3], v[4], v[5]); break;
                }
            }
            commands = 0;
            if (path_count < ARRAY_SIZE(paths))
                paths[path_count++] = cairo_copy_path(cr);
            cairo_new_path(cr);
        }
    }
    for (unsigned int i = 0; i < path_count; i++) cairo_path_destroy(paths[i]);
    cairo_surface_flush(surface);
    eos_mem_write(destination, pixels, 960 * 540 * 4);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_free(pixels);
    g_free(data);
}
