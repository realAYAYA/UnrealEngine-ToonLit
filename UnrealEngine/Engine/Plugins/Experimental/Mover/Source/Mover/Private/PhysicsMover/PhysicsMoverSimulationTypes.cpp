// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "HAL/IConsoleManager.h"

//////////////////////////////////////////////////////////////////////////
// Debug

FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

FAutoConsoleVariableRef CVarPhysicsDrivenMotionEnableMultithreading(TEXT("p.mover.physics.EnableMultithreading"),
	GPhysicsDrivenMotionDebugParams.EnableMultithreading, TEXT("Enable multi-threading of physics driven motion updates."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionDebugDrawFloorTest(TEXT("p.mover.physics.DebugDrawFloorQueries"),
	GPhysicsDrivenMotionDebugParams.DebugDrawGroundQueries, TEXT("Debug draw floor test queries."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionTeleportThreshold(TEXT("p.mover.physics.TeleportThreshold"),
	GPhysicsDrivenMotionDebugParams.TeleportThreshold, TEXT("Single frame movement threshold in cm that will trigger a teleport."));

FAutoConsoleVariableRef CVarPhysicsDrivenMotionMinStepUpDistance(TEXT("p.mover.physics.MinStepUpDistance"),
	GPhysicsDrivenMotionDebugParams.MinStepUpDistance, TEXT("Minimum distance that will be considered a step up."));

//////////////////////////////////////////////////////////////////////////
// FMovementSettingsInputs

FMoverDataStructBase* FMovementSettingsInputs::Clone() const
{
	FMovementSettingsInputs* CopyPtr = new FMovementSettingsInputs(*this);
	return CopyPtr;
}

bool FMovementSettingsInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << MaxSpeed;
	Ar << Acceleration;

	bOutSuccess = true;
	return true;
}


void FMovementSettingsInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("MaxSpeed=%.2f\n", MaxSpeed);
	Out.Appendf("Acceleration=%.2f\n", Acceleration);
}