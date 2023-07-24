
#ifndef HWCPIPE_EXPORT_H
#define HWCPIPE_EXPORT_H

#ifdef HWCPIPE_STATIC_DEFINE
#  define HWCPIPE_EXPORT
#  define HWCPIPE_NO_EXPORT
#else
#  ifndef HWCPIPE_EXPORT
#    ifdef hwcpipe_EXPORTS
        /* We are building this library */
#      define HWCPIPE_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define HWCPIPE_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef HWCPIPE_NO_EXPORT
#    define HWCPIPE_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef HWCPIPE_DEPRECATED
#  define HWCPIPE_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef HWCPIPE_DEPRECATED_EXPORT
#  define HWCPIPE_DEPRECATED_EXPORT HWCPIPE_EXPORT HWCPIPE_DEPRECATED
#endif

#ifndef HWCPIPE_DEPRECATED_NO_EXPORT
#  define HWCPIPE_DEPRECATED_NO_EXPORT HWCPIPE_NO_EXPORT HWCPIPE_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef HWCPIPE_NO_DEPRECATED
#    define HWCPIPE_NO_DEPRECATED
#  endif
#endif

#endif
