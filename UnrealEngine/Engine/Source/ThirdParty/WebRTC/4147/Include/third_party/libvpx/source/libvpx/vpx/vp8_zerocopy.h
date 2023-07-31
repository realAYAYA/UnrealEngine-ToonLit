#ifndef VPX_VPX_VP8_ZEROCOPY_H_
#define VPX_VPX_VP8_ZEROCOPY_H_

#include "./vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct vp8_zerocopy_param {
    uint32_t magic;
    uint32_t struct_size;
    void (*callback)(int32_t, void*);
    void* callback_arg;
    int16_t border[4];
    int8_t cpu_speed;
    uint8_t param_wipe;
} vp8_zerocopy_param;

const struct vp8_zerocopy_param* vp8_get_zerocopy_param(const void* y_data, int y_width, int y_stride);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VPX_VPX_VP8_ZEROCOPY_H_
