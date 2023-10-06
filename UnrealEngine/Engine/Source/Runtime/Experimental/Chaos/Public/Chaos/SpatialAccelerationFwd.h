// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Real.h"

namespace Chaos
{
	class FAccelerationStructureHandle;
	template <typename TPayload, typename T, int d>
	class ISpatialAcceleration;

	using IDefaultChaosSpatialAcceleration = ISpatialAcceleration<FAccelerationStructureHandle, FReal, 3>;
}