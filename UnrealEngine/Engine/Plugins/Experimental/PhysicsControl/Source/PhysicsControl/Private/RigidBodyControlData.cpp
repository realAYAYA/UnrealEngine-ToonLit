// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigidBodyControlData.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsActorHandle.h"

//======================================================================================================================
FRigidBodyControlRecord::FRigidBodyControlRecord(
	const FPhysicsControl& InControl, ImmediatePhysics::FJointHandle* InJointHandle)
	: Control(InControl)
	, ControlData(Control.ControlData)
	, ChildBodyIndex(-1)
	, ParentBodyIndex(-1)
	, JointHandle(InJointHandle)
{
}

//======================================================================================================================
void FRigidBodyControlRecord::ResetCurrent(bool bResetTarget)
{
	ControlData = Control.ControlData;
	ControlMultiplier = Control.ControlMultiplier;

	if (bResetTarget)
	{
		ControlTarget = FRigidBodyControlTarget();
	}
}

//======================================================================================================================
FVector FRigidBodyControlRecord::GetControlPoint(const ImmediatePhysics::FActorHandle* ChildActorHandle) const
{
	if (ControlData.bUseCustomControlPoint)
	{
		return ControlData.CustomControlPoint;
	}
	return ChildActorHandle->GetLocalCoMLocation();
}

//======================================================================================================================
FRigidBodyModifierRecord::FRigidBodyModifierRecord(
	const FPhysicsBodyModifier& InModifier, ImmediatePhysics::FActorHandle* InActorHandle)
	: Modifier(InModifier)
	, ModifierData(Modifier.ModifierData)
	, ActorHandle(InActorHandle)
{
}

//======================================================================================================================
void FRigidBodyModifierRecord::ResetCurrent()
{
	ModifierData = Modifier.ModifierData;
}

