// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12Submission.h"

// Standard Windows implementation, used to mark the type as 'final'.
struct FD3D12Payload final : public FD3D12PayloadBase
{
	FD3D12Payload(FD3D12Device* Device, ED3D12QueueType QueueType)
		: FD3D12PayloadBase(Device, QueueType)
	{
	}
};
