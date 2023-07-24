// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NvmlWrapperPublic.h"

class EXPORTLIB NvmlEncoder
{
public:
	static bool IsEncoderSessionAvailable(const uint32_t GpuIdx);
	static int32_t GetEncoderSessionCount(const uint32_t GpuIdx);
};