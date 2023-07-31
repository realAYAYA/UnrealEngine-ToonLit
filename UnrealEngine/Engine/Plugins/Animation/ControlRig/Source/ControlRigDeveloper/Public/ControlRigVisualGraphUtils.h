// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VisualGraphUtils.h"
#include "Rigs/RigHierarchy.h"

struct CONTROLRIGDEVELOPER_API FControlRigVisualGraphUtils
{
	static FString DumpRigHierarchyToDotGraph(URigHierarchy* InHierarchy, const FName& InEventName = NAME_None);
};
