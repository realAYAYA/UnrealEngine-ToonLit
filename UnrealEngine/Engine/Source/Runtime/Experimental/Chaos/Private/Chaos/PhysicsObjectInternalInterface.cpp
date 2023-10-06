// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PhysicsObjectInternalInterface.h"

namespace Chaos
{
	FReadPhysicsObjectInterface_Internal FPhysicsObjectInternalInterface::GetRead()
	{
		return CreateReadInterface<EThreadContext::Internal>();
	}

	FWritePhysicsObjectInterface_Internal FPhysicsObjectInternalInterface::GetWrite()
	{
		return CreateWriteInterface<EThreadContext::Internal>();
	}
}