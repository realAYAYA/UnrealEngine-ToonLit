// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "UObject/NameTypes.h"

class UAvaTransitionSubsystem;
enum class EAvaTransitionLayerCompareType : uint8;
struct FAvaTagHandle;
struct FAvaTagHandleContainer;
struct FAvaTransitionBehaviorInstance;
struct FAvaTransitionContext;
struct FAvaTransitionLayerComparator;

struct AVALANCHETRANSITION_API FAvaTransitionLayerUtils
{
	/** Gets all the Behavior Instances that match the Comparator */
	static TArray<const FAvaTransitionBehaviorInstance*> QueryBehaviorInstances(UAvaTransitionSubsystem& InTransitionSubsystem, const FAvaTransitionLayerComparator& InComparator);

	/** Builds a Comparator for the given Context (and optionally Layer), excluding the provided Transition Context (assumes it's the transition context calling this) */
	static FAvaTransitionLayerComparator BuildComparator(const FAvaTransitionContext& InTransitionContext, EAvaTransitionLayerCompareType InCompareType, const FAvaTagHandle& InSpecificLayer);

	/** Builds a Comparator for the given Context and specific layers, excluding the provided Transition Context (assumes it's the transition context calling this) */
	static FAvaTransitionLayerComparator BuildComparator(const FAvaTransitionContext& InTransitionContext, EAvaTransitionLayerCompareType InCompareType, const FAvaTagHandleContainer& InSpecificLayers);

	static FText GetLayerQueryText(EAvaTransitionLayerCompareType InLayerType, FName InSpecificLayerName);
};
