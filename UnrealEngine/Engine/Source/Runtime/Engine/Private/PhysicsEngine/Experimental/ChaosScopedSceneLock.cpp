// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/Experimental/ChaosScopedSceneLock.h"

#include "Chaos/PBDJointConstraintData.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "PBDRigidsSolver.h"

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(FPhysicsActorHandle const* InActorHandle, EPhysicsInterfaceScopedLockType InLockType)
	: LockType(InLockType)
{
	auto Scene = GetSceneForActor(InActorHandle);
	Solver = Scene ? Scene->GetSolver() : nullptr;
	LockScene();
}

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(FPhysicsActorHandle const* InActorHandleA, FPhysicsActorHandle const* InActorHandleB, EPhysicsInterfaceScopedLockType InLockType)
	: LockType(InLockType)
{
	FChaosScene* SceneA = GetSceneForActor(InActorHandleA);
	FChaosScene* SceneB = GetSceneForActor(InActorHandleB);
	FChaosScene* Scene = nullptr;

	if (SceneA == SceneB)
	{
		Scene = SceneA;
	}
	else if (!SceneA || !SceneB)
	{
		Scene = SceneA ? SceneA : SceneB;
	}
	else
	{
		UE_LOG(LogPhysics, Warning, TEXT("Attempted to aquire a physics scene lock for two paired actors that were not in the same scene. Skipping lock"));
	}

	Solver = Scene ? Scene->GetSolver() : nullptr;
	LockScene();
}

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(FPhysicsConstraintHandle const* InConstraintHandle, EPhysicsInterfaceScopedLockType InLockType)
	: Solver(nullptr)
	, LockType(InLockType)
{
	if (InConstraintHandle)
	{
		auto Scene = GetSceneForActor(InConstraintHandle);
		Solver = Scene ? Scene->GetSolver() : nullptr;
	}
#if CHAOS_CHECKED
	if (!Solver)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Failed to find Scene for constraint. Skipping lock"));
	}
#endif
	LockScene();
}

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(USkeletalMeshComponent* InSkelMeshComp, EPhysicsInterfaceScopedLockType InLockType)
	: LockType(InLockType)
{
	Solver = nullptr;

	if (InSkelMeshComp)
	{
		for (FBodyInstance* BI : InSkelMeshComp->Bodies)
		{
			auto Scene = GetSceneForActor(&BI->GetPhysicsActorHandle());
			if (Scene)
			{
				Solver = Scene->GetSolver();
				break;
			}
		}
	}

	LockScene();
}

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(Chaos::FPhysicsObjectHandle InObjectA, Chaos::FPhysicsObjectHandle InObjectB, EPhysicsInterfaceScopedLockType InLockType)
	: LockType(InLockType)
{
	FChaosScene* SceneA = static_cast<FChaosScene*>(FPhysicsObjectExternalInterface::GetScene({ &InObjectA, 1 }));
	FChaosScene* SceneB = static_cast<FChaosScene*>(FPhysicsObjectExternalInterface::GetScene({ &InObjectB, 1 }));
	FChaosScene* Scene = nullptr;

	if (SceneA == SceneB)
	{
		Scene = SceneA;
	}
	else if (!SceneA || !SceneB)
	{
		Scene = SceneA ? SceneA : SceneB;
	}
	else
	{
		UE_LOG(LogPhysics, Warning, TEXT("Attempted to aquire a physics scene lock for two paired physics objects that were not in the same scene. Skipping lock"));
	}

	Solver = Scene ? Scene->GetSolver() : nullptr;
	LockScene();
}

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(FChaosScene* InScene, EPhysicsInterfaceScopedLockType InLockType)
	: Solver(InScene ? InScene->GetSolver() : nullptr)
	, LockType(InLockType)
{
	LockScene();
}

FScopedSceneLock_Chaos::FScopedSceneLock_Chaos(FScopedSceneLock_Chaos&& Other)
{
	*this = MoveTemp(Other);
}

FScopedSceneLock_Chaos& FScopedSceneLock_Chaos::operator=(FScopedSceneLock_Chaos&& Other)
{
	Solver = Other.Solver;
	LockType = Other.LockType;
	bHasLock = Other.bHasLock;

	Other.bHasLock = false;
	Other.Solver = nullptr;
	return *this;
}

FScopedSceneLock_Chaos::~FScopedSceneLock_Chaos()
{
	Release();
}

void FScopedSceneLock_Chaos::Release()
{
	if (bHasLock)
	{
		UnlockScene();
	}
}

void FScopedSceneLock_Chaos::LockScene()
{
	if (!Solver)
	{
		return;
	}

	switch (LockType)
	{
	case EPhysicsInterfaceScopedLockType::Read:
		Solver->GetExternalDataLock_External().ReadLock();
		break;
	case EPhysicsInterfaceScopedLockType::Write:
		Solver->GetExternalDataLock_External().WriteLock();
		break;
	}

	bHasLock = true;
}

void FScopedSceneLock_Chaos::UnlockScene()
{
	if (!Solver)
	{
		return;
	}

	switch (LockType)
	{
	case EPhysicsInterfaceScopedLockType::Read:
		Solver->GetExternalDataLock_External().ReadUnlock();
		break;
	case EPhysicsInterfaceScopedLockType::Write:
		Solver->GetExternalDataLock_External().WriteUnlock();
		break;
	}

	bHasLock = false;
}

FChaosScene* FScopedSceneLock_Chaos::GetSceneForActor(FPhysicsActorHandle const* InActorHandle)
{
	if (InActorHandle)
	{
		return static_cast<FPhysScene*>(FChaosEngineInterface::GetCurrentScene(*InActorHandle));
	}

	return nullptr;
}

FChaosScene* FScopedSceneLock_Chaos::GetSceneForActor(FPhysicsConstraintHandle const* InConstraintHandle)
{
	if (InConstraintHandle && InConstraintHandle->IsValid() && InConstraintHandle->Constraint->IsType(Chaos::EConstraintType::JointConstraintType))
	{
		Chaos::FJointConstraint* Constraint = static_cast<Chaos::FJointConstraint*>(InConstraintHandle->Constraint);

		FConstraintInstanceBase* ConstraintInstance = (Constraint) ? FPhysicsUserData_Chaos::Get<FConstraintInstanceBase>(Constraint->GetUserData()) : nullptr;
		if (ConstraintInstance)
		{
			return ConstraintInstance->GetPhysicsScene();
		}
	}

	return nullptr;
}
