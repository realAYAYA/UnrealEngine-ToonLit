// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCommon.h"

namespace PCGFeatureSwitches
{

	TAutoConsoleVariable<bool> CVarCheckSamplerMemory{
		TEXT("pcg.CheckSamplerMemory"),
		true,
		TEXT("Checks expected memory size consumption prior to performing sampling operations")
	};

}