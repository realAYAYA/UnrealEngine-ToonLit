// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "HierarchicalLODType.generated.h"

UENUM()	
enum class EHierarchicalLODActionType : uint8
{
	InvalidAction,
	CreateCluster,
	AddActorToCluster,
	MoveActorToCluster,
	RemoveActorFromCluster,
	MergeClusters,
	ChildCluster,

	MAX
};
