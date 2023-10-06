// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <stdlib.h>

namespace AutoRTFM
{

class FContext;

void* MemcpyToNew(void* Dst, const void* Src, size_t Size, FContext* Context);
void* Memcpy(void* Dst, const void* Src, size_t Size, FContext* Context);
void* Memmove(void* Dst, const void* Src, size_t Size, FContext* Context);
void* Memset(void* Dst, int Value, size_t Size, FContext* Context);

} // namespace AutoRTFM

