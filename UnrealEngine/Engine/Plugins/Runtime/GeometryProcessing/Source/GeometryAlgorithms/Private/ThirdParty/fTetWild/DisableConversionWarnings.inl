// Disable conversion warnings
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wimplicit-int-float-conversion")
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#endif
#if __has_warning("-Wimplicit-float-conversion")
#pragma clang diagnostic ignored "-Wimplicit-float-conversion"
#endif
#if __has_warning("-Wimplicit-int-conversion")
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif
#if __has_warning("-Wfloat-conversion")
#pragma clang diagnostic ignored "-Wfloat-conversion"
#endif
#endif
