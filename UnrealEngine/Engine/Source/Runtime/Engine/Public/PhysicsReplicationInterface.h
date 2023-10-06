// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;
struct FRigidBodyState;

class IPhysicsReplication
{
public:
	virtual ~IPhysicsReplication() { }

	virtual void Tick(float DeltaSeconds) { }

	virtual void SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget, int32 ServerFrame) = 0;

	virtual void RemoveReplicatedTarget(UPrimitiveComponent* Component) = 0;
};
