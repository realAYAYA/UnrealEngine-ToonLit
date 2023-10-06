// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/PhysicsObjectInterface.h"

namespace Chaos
{
	class FPhysicsObjectInternalInterface : public Chaos::FPhysicsObjectInterface
	{
	public:
		static CHAOS_API FReadPhysicsObjectInterface_Internal GetRead();
		static CHAOS_API FWritePhysicsObjectInterface_Internal GetWrite();
	};
}
