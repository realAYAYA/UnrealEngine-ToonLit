// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Framework/MultiBufferResource.h"
#include "Chaos/Framework/PhysicsProxy.h"
#include "Chaos/PBDSuspensionConstraints.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/ParticleHandle.h"
#include "PhysicsCoreTypes.h"
#include "Chaos/Defines.h"
#include "Chaos/PBDConstraintBaseData.h"

namespace Chaos
{
	class FSuspensionConstraint;

	class FPBDRigidsEvolutionGBF;

class CHAOS_API FSuspensionConstraintPhysicsProxy : public IPhysicsProxyBase
{
public:
	using Base = IPhysicsProxyBase;
	
	FSuspensionConstraintPhysicsProxy() = delete;
	FSuspensionConstraintPhysicsProxy(FSuspensionConstraint* InConstraint, FPBDSuspensionConstraintHandle* InHandle, UObject* InOwner = nullptr);

	bool IsInitialized() const { return bInitialized; }
	void SetInitialized() { bInitialized = true; }

	static FGeometryParticleHandle* GetParticleHandleFromProxy(IPhysicsProxyBase* ProxyBase);

	//
	//  Lifespan Management
	//

	void InitializeOnPhysicsThread(FPBDRigidsSolver* InSolver, FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData);

	// Merge to perform a remote sync
	void PushStateOnGameThread(FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData);

	void PushStateOnPhysicsThread(FPBDRigidsSolver* InSolver, const FDirtyPropertiesManager& Manager, int32 DataIdx, const FDirtyChaosProperties& RemoteData);
	// Merge to perform a remote sync - END

	void DestroyOnGameThread();
	void DestroyOnPhysicsThread(FPBDRigidsSolver* InSolver);

	void UpdateTargetOnPhysicsThread(FPBDRigidsSolver* InSolver, const FVector& TargetPos, const FVector& Normal, bool Enabled);


	//
	// Member Access
	//

	FPBDSuspensionConstraintHandle* GetHandle()
	{
		return Constraint_PT;
	}

	const FPBDSuspensionConstraintHandle* GetHandle() const
	{
		return Constraint_PT;
	}

	virtual void* GetHandleUnsafe() const override
	{
		return Constraint_PT;
	}

	void SetHandle(FPBDSuspensionConstraintHandle* InHandle)
	{
		Constraint_PT = InHandle;
	}

	FSuspensionConstraint* GetConstraint()
	{
		return Constraint_GT;
	}

	const FSuspensionConstraint* GetConstraint() const
	{
		return Constraint_GT;
	}
	
private:
	FSuspensionConstraint* Constraint_GT;
	FPBDSuspensionConstraintHandle* Constraint_PT;
	bool bInitialized;
};
}