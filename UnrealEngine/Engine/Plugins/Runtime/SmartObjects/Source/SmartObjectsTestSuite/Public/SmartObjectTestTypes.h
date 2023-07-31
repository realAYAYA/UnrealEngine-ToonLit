// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectDefinition.h"
#include "SmartObjectTypes.h"
#include "SmartObjectTestTypes.generated.h"

/**
 * Concrete definition class for testing purposes
 */
UCLASS(HideDropdown)
class SMARTOBJECTSTESTSUITE_API USmartObjectTestBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
};

/**
 * Some user data to assign to a slot definition
 */
USTRUCT(meta=(Hidden))
struct FSmartObjectSlotTestDefinitionData: public FSmartObjectSlotDefinitionData
{
	GENERATED_BODY()

	float SomeSharedFloat= 0.f;
};

/**
 * Some user runtime data to assign to a slot instance
 */
USTRUCT(meta=(Hidden))
struct FSmartObjectSlotTestRuntimeData : public FSmartObjectSlotStateData
{
	GENERATED_BODY()

	float SomePerInstanceSharedFloat = 0.0f;
};