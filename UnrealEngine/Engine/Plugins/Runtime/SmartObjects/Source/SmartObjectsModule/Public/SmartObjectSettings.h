// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "Engine/DeveloperSettings.h"
#include "SmartObjectSettings.generated.h"

UCLASS(config = SmartObjects, defaultconfig, DisplayName = "SmartObject", AutoExpandCategories = "SmartObject")
class SMARTOBJECTSMODULE_API USmartObjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Default filtering policy to use for TagQueries applied on User Tags in newly created SmartObjectDefinitions.
	 * Indicates how TagQueries from slots and parent object will be processed against User Tags from a find request.
	 */
	UPROPERTY(EditAnywhere, config, Category = SmartObject)
	ESmartObjectTagFilteringPolicy DefaultUserTagsFilteringPolicy = ESmartObjectTagFilteringPolicy::Override;

	/**
	 * Default merging policy to use for Activity Tags in newly created SmartObjectDefinitions.
	 * Indicates how Activity Tags from slots and parent object are combined to be evaluated by an Activity TagQuery from a find request.
	 */
	UPROPERTY(EditAnywhere, config, Category = SmartObject)
	ESmartObjectTagMergingPolicy DefaultActivityTagsMergingPolicy = ESmartObjectTagMergingPolicy::Override;
};
