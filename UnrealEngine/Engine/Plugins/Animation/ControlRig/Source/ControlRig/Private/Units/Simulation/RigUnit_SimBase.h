// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_SimBase.generated.h"

USTRUCT(meta=(Abstract, Category = "Simulation", NodeColor = "0.25 0.05 0.05"))
struct CONTROLRIG_API FRigUnit_SimBase : public FRigUnit
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, Category = "Simulation", NodeColor = "0.25 0.05 0.05"))
struct CONTROLRIG_API FRigUnit_SimBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()
};
