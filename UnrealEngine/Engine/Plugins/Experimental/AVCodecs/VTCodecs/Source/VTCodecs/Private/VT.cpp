// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Thread.h"
#include "RHI.h"

REGISTER_TYPEID(FVT);

FVT::FVT()
{
}

bool FVT::IsValid() const
{
	return bHasCompatibleGPU;
}
