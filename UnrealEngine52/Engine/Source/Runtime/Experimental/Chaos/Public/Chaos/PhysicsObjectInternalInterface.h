// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/PhysicsObjectInterface.h"

namespace Chaos
{
	class CHAOS_API FPhysicsObjectInternalInterface : public Chaos::FPhysicsObjectInterface
	{
	public:
		static FReadPhysicsObjectInterface_Internal GetRead();
		static FWritePhysicsObjectInterface_Internal GetWrite();
	};
}