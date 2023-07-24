// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Physics/Experimental/ChaosScopedSceneLock.h"
#include "PhysicsObjectPhysicsCoreInterface.h"
#include "Containers/ArrayView.h"
#include <type_traits>

template<EPhysicsInterfaceScopedLockType LockType>
class ENGINE_API FLockedPhysicsObjectExternalInterface
{
public:
	using InterfaceType = std::conditional_t<LockType == EPhysicsInterfaceScopedLockType::Read, Chaos::FReadPhysicsObjectInterface_External, Chaos::FWritePhysicsObjectInterface_External>;

	FLockedPhysicsObjectExternalInterface(FChaosScene* Scene, InterfaceType&& InInterface)
		: Lock(Scene, LockType)
		, Interface(InInterface)
	{
	}

	FLockedPhysicsObjectExternalInterface(FLockedPhysicsObjectExternalInterface& Other) = delete;
	FLockedPhysicsObjectExternalInterface& operator=(FLockedPhysicsObjectExternalInterface& Other) = delete;

	FLockedPhysicsObjectExternalInterface(FLockedPhysicsObjectExternalInterface&& Other) = default;
	FLockedPhysicsObjectExternalInterface& operator=(FLockedPhysicsObjectExternalInterface&& Other) = default;

	InterfaceType& GetInterface() { return Interface; }
	InterfaceType* operator->() { return &Interface; }

	// The FORCEINLINE gets around some linker issues when trying to call Destroy.
	FORCEINLINE void Release()
	{
		Lock.Release();
	}

private:
	FScopedSceneLock_Chaos Lock;
	InterfaceType Interface;
};

using FLockedReadPhysicsObjectExternalInterface = FLockedPhysicsObjectExternalInterface<EPhysicsInterfaceScopedLockType::Read>;
using FLockedWritePhysicsObjectExternalInterface = FLockedPhysicsObjectExternalInterface<EPhysicsInterfaceScopedLockType::Write>;

class ENGINE_API FPhysicsObjectExternalInterface : public Chaos::FPhysicsObjectInterface, public PhysicsObjectPhysicsCoreInterface
{
public:
	static FLockedReadPhysicsObjectExternalInterface LockRead(TArrayView<Chaos::FPhysicsObjectHandle> InObjects);
	static FLockedWritePhysicsObjectExternalInterface LockWrite(TArrayView<Chaos::FPhysicsObjectHandle> InObjects);

	static Chaos::FReadPhysicsObjectInterface_External GetRead_AssumesLocked();
	static Chaos::FWritePhysicsObjectInterface_External GetWrite_AssumesLocked();
};