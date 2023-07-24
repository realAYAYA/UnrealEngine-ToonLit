// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsInterfaceDeclaresCore.h"
#include "Chaos/PhysicsObject.h"

enum class EPhysicsInterfaceScopedLockType : uint8
{
	Read,
	Write
};

class USkeletalMeshComponent;
class FChaosScene;

namespace Chaos
{
	class FPBDRigidsSolver;
}

struct ENGINE_API FScopedSceneLock_Chaos
{
	FScopedSceneLock_Chaos(FPhysicsActorHandle const* InActorHandle, EPhysicsInterfaceScopedLockType InLockType);
	FScopedSceneLock_Chaos(FPhysicsActorHandle const* InActorHandleA, FPhysicsActorHandle const* InActorHandleB, EPhysicsInterfaceScopedLockType InLockType);
	FScopedSceneLock_Chaos(FPhysicsConstraintHandle const* InConstraintHandle, EPhysicsInterfaceScopedLockType InLockType);
	FScopedSceneLock_Chaos(USkeletalMeshComponent* InSkelMeshComp, EPhysicsInterfaceScopedLockType InLockType);
	FScopedSceneLock_Chaos(Chaos::FPhysicsObjectHandle InObjectA, Chaos::FPhysicsObjectHandle InObjectB, EPhysicsInterfaceScopedLockType InLockType);
	FScopedSceneLock_Chaos(FChaosScene* InScene, EPhysicsInterfaceScopedLockType InLockType);
	~FScopedSceneLock_Chaos();

	FScopedSceneLock_Chaos(FScopedSceneLock_Chaos& Other) = delete;
	FScopedSceneLock_Chaos& operator=(FScopedSceneLock_Chaos& Other) = delete;

	FScopedSceneLock_Chaos(FScopedSceneLock_Chaos&& Other);
	FScopedSceneLock_Chaos& operator=(FScopedSceneLock_Chaos&& Other);

	void Release();

private:

	void LockScene();
	void UnlockScene();

	FChaosScene* GetSceneForActor(FPhysicsActorHandle const* InActorHandle);
	FChaosScene* GetSceneForActor(FPhysicsConstraintHandle const* InConstraintHandle);

	bool bHasLock = false;
	Chaos::FPBDRigidsSolver* Solver;
	EPhysicsInterfaceScopedLockType LockType;
};