// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandBufferFence.h: Metal RHI Command Buffer Fence Definition.
=============================================================================*/

#pragma once

struct FMetalCommandBufferFence
{
	bool Wait(uint64 Millis);

	mtlpp::CommandBufferFence CommandBufferFence;
};
