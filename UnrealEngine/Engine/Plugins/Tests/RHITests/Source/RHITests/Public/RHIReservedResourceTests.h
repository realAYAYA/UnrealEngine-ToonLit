// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHITestsCommon.h"

class FRHIReservedResourceTests
{
public:

	static bool Test_ReservedResource_CreateTexture(FRHICommandListImmediate& RHICmdList);
	static bool Test_ReservedResource_CreateVolumeTexture(FRHICommandListImmediate& RHICmdList);
	static bool Test_ReservedResource_CreateBuffer(FRHICommandListImmediate& RHICmdList);
	static bool Test_ReservedResource_CommitBuffer(FRHICommandListImmediate& RHICmdList);
	static bool Test_ReservedResource_DecommitBuffer(FRHICommandListImmediate& RHICmdList);
};

