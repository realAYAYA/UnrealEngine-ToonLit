// Copyright Epic Games, Inc. All Rights Reserved.

#include "TP_VehicleAdvWheelFront.h"
#include "UObject/ConstructorHelpers.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UTP_VehicleAdvWheelFront::UTP_VehicleAdvWheelFront()
{
	WheelRadius = 39.f;
	WheelWidth = 35.0f;
	FrictionForceMultiplier = 3.0f;
	CorneringStiffness = 1200.0f;
	bAffectedByEngine = false;
	bAffectedByHandbrake = false;
	bAffectedBySteering = true;
	AxleType = EAxleType::Front;
	SpringRate = 250.0f;
	SpringPreload = 50.f;
	SuspensionDampingRatio = 0.5f;
	WheelLoadRatio = 0.5f;
	RollbarScaling = 0.15f;
	SuspensionMaxRaise = 10.0f;
	SuspensionMaxDrop = 10.0f;
	WheelLoadRatio = 0.5f;
	MaxSteerAngle = 40.f;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
