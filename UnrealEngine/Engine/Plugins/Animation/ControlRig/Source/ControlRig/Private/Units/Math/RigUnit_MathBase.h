// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_MathBase.generated.h"

USTRUCT(meta=(Abstract, NodeColor = "0.05 0.25 0.05"))
struct CONTROLRIG_API FRigUnit_MathBase : public FRigUnit
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, NodeColor = "0.05 0.25 0.05"))
struct CONTROLRIG_API FRigUnit_MathMutableBase : public FRigUnitMutable
{
	GENERATED_BODY()
};
