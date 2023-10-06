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

struct FScopedSceneLock_Chaos
{
	ENGINE_API FScopedSceneLock_Chaos(FPhysicsActorHandle const* InActorHandle, EPhysicsInterfaceScopedLockType InLockType);
	ENGINE_API FScopedSceneLock_Chaos(FPhysicsActorHandle const* InActorHandleA, FPhysicsActorHandle const* InActorHandleB, EPhysicsInterfaceScopedLockType InLockType);
	ENGINE_API FScopedSceneLock_Chaos(FPhysicsConstraintHandle const* InConstraintHandle, EPhysicsInterfaceScopedLockType InLockType);
	ENGINE_API FScopedSceneLock_Chaos(USkeletalMeshComponent* InSkelMeshComp, EPhysicsInterfaceScopedLockType InLockType);
	ENGINE_API FScopedSceneLock_Chaos(Chaos::FPhysicsObjectHandle InObjectA, Chaos::FPhysicsObjectHandle InObjectB, EPhysicsInterfaceScopedLockType InLockType);
	ENGINE_API FScopedSceneLock_Chaos(FChaosScene* InScene, EPhysicsInterfaceScopedLockType InLockType);
	ENGINE_API ~FScopedSceneLock_Chaos();

	FScopedSceneLock_Chaos(FScopedSceneLock_Chaos& Other) = delete;
	FScopedSceneLock_Chaos& operator=(FScopedSceneLock_Chaos& Other) = delete;

	ENGINE_API FScopedSceneLock_Chaos(FScopedSceneLock_Chaos&& Other);
	ENGINE_API FScopedSceneLock_Chaos& operator=(FScopedSceneLock_Chaos&& Other);

	ENGINE_API void Release();

private:

	ENGINE_API void LockScene();
	ENGINE_API void UnlockScene();

	ENGINE_API FChaosScene* GetSceneForActor(FPhysicsActorHandle const* InActorHandle);
	ENGINE_API FChaosScene* GetSceneForActor(FPhysicsConstraintHandle const* InConstraintHandle);

	bool bHasLock = false;
	Chaos::FPBDRigidsSolver* Solver;
	EPhysicsInterfaceScopedLockType LockType;
};
