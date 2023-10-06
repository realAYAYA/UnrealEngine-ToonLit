// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHITestsCommon.h"

class FRHIReadbackTests
{
public:
	static bool Test_BufferReadback(FRHICommandListImmediate& RHICmdList);
	static bool Test_TextureReadback(FRHICommandListImmediate& RHICmdList);
};
