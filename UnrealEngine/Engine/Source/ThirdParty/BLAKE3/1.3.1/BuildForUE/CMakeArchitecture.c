// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(_M_X64) || defined(__amd64__) || defined(__x86_64__)
#undef x86_64
x86_64

#elif defined(_M_IX86) || defined(__i386__)
#undef x86
x86

#elif defined(__aarch64__) || defined(_M_ARM64)
#undef arm64
arm64

#elif defined(__has_builtin) && __has_builtin(__is_target_arch) && (__is_target_arch(arm64) || __is_target_arch(arm64e))
#undef arm64
arm64

#elif defined(__arm__) || defined(_M_ARM)
#undef arm
arm

#else
unknown

#endif
