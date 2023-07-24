// Copyright Epic Games, Inc. All Rights Reserved.
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"

FLockedReadPhysicsObjectExternalInterface FPhysicsObjectExternalInterface::LockRead(TArrayView<Chaos::FPhysicsObjectHandle> InObjects)
{
	return FLockedReadPhysicsObjectExternalInterface{ GetScene(InObjects), CreateReadInterface<Chaos::EThreadContext::External>() };
}

FLockedWritePhysicsObjectExternalInterface FPhysicsObjectExternalInterface::LockWrite(TArrayView<Chaos::FPhysicsObjectHandle> InObjects)
{
	return FLockedWritePhysicsObjectExternalInterface{ GetScene(InObjects), CreateWriteInterface<Chaos::EThreadContext::External>() };
}

Chaos::FReadPhysicsObjectInterface_External FPhysicsObjectExternalInterface::GetRead_AssumesLocked()
{
	return CreateReadInterface<Chaos::EThreadContext::External>();
}

Chaos::FWritePhysicsObjectInterface_External FPhysicsObjectExternalInterface::GetWrite_AssumesLocked()
{
	return CreateWriteInterface<Chaos::EThreadContext::External>();
}