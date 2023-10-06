// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysXPublic.h"
#include "Containers/Union.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsInterfaceUtilsCore.h"
#include "PhysicsReplicationInterface.h"
#include "Misc/CoreMiscDefines.h"

class FPhysScene_PhysX;
struct FConstraintInstance;

// FILTER DATA

/** Utility for creating a filter data object for performing a query (trace) against the scene */
FCollisionFilterData CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex, const FCollisionResponseContainer& InCollisionResponseContainer, const struct FCollisionQueryParams& QueryParam, const struct FCollisionObjectQueryParams & ObjectParam, const bool bMultitrace);

struct FConstraintBrokenDelegateData
{
	FConstraintBrokenDelegateData(FConstraintInstance* ConstraintInstance);

	void DispatchOnBroken()
	{
		OnConstraintBrokenDelegate.ExecuteIfBound(ConstraintIndex);
	}

	FOnConstraintBroken OnConstraintBrokenDelegate;
	int32 ConstraintIndex;
};

class FPhysicsReplication;

/** Interface for the creation of customized physics replication.*/
class IPhysicsReplicationFactory
{
public:

	// NOTE: Once the old Create/Destroy methods are deprecated, remove the default implementation!
	ENGINE_API virtual TUniquePtr<IPhysicsReplication> CreatePhysicsReplication(FPhysScene* OwningPhysScene);

	UE_DEPRECATED(5.3, "Use CreatePhysicsReplication (which gives a replication interface) instead")
	virtual FPhysicsReplication* Create(FPhysScene* OwningPhysScene) { return nullptr; }
	UE_DEPRECATED(5.3, "No longer used - PhysScene owns the physics replication ptr once it is created. If you need a custom deleter, override CreatePhysicsReplication and specify the deleter in TUniquePtr.")
	virtual void Destroy(FPhysicsReplication* PhysicsReplication) { }
};
