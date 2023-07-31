#ifndef COMMON_OS_HANDLED_H_
#define COMMON_OS_HANDLED_H_

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <winsock2.h>

// Need to depend on our local one and not the systems
#include "third_party/musl/include/elf.h"

#define ssize_t uintptr_t
#define OSHandleInvalid INVALID_HANDLE_VALUE

#ifndef __WORDSIZE
#if defined(__i386__) || defined(_M_IX86) || defined(__ARM_EABI__)
#define __WORDSIZE 32
#elif defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
#define __WORDSIZE 64
#else
#error "Unsupported arch"
#endif
#endif

// Assume little endian
#define __LITTLE_ENDIAN 1
#define __BIG_ENDIAN 0
#define __BYTE_ORDER __LITTLE_ENDIAN

// Turned it off to not have to port these bits
#define NO_STABS_SUPPORT 1

typedef HANDLE OSHandle;
#else

#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define OSHandleInvalid -1
typedef int OSHandle;
#endif

extern int GetOSPageSize();
extern int64_t GetOSHandleSize(OSHandle os_handle);
extern void CloseOSHandle(OSHandle os_handle);
extern size_t ReadOSHandle(const OSHandle os_handle, void* buf, size_t size, size_t nmemb = 1, size_t offset = 0);
extern void* CreateOSMapping(void* addr, int64_t length, int prot, int flags, OSHandle os_handle, size_t offset);
extern void CloseOSMapping(void* addr, size_t length = 0);

#endif
