// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_DebugBase.generated.h"

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct CONTROLRIG_API FRigUnit_DebugBase : public FRigUnit
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract, Category="Debug", NodeColor = "0.83077 0.846873 0.049707"))
struct CONTROLRIG_API FRigUnit_DebugBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()
};
