// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif
void* StbMalloc(size_t Size);
void* StbRealloc(void* Pointer, size_t Size);
void StbFree(void* Pointer);
char* StbStrDup(const char* String);
#ifdef __cplusplus
}
#endif

#define STB_COMMON_MALLOC(Size) StbMalloc(Size)
#define STB_COMMON_REALLOC(Pointer, Size) StbRealloc(Pointer, Size)
#define STB_COMMON_FREE(Pointer) StbFree(Pointer)
#define STB_COMMON_STRDUP(String) StbStrDup(String)
