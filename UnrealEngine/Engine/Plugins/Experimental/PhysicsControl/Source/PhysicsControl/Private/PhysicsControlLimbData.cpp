// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlLimbData.h"

//======================================================================================================================
FPhysicsControlCharacterSetupData& FPhysicsControlCharacterSetupData::operator+=(
	const FPhysicsControlCharacterSetupData& Other)
{
	LimbSetupData += Other.LimbSetupData;
	DefaultWorldSpaceControlData = Other.DefaultWorldSpaceControlData;
	DefaultParentSpaceControlData = Other.DefaultParentSpaceControlData;
	DefaultBodyModifierData = Other.DefaultBodyModifierData;
	return *this;
}

