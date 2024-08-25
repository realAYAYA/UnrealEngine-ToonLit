// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "WorldConditions/SmartObjectWorldConditionSchema.h"
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
	UPROPERTY(EditAnywhere, config, Category = "SmartObject")
	ESmartObjectTagFilteringPolicy DefaultUserTagsFilteringPolicy = ESmartObjectTagFilteringPolicy::Override;

	/**
	 * Default merging policy to use for Activity Tags in newly created SmartObjectDefinitions.
	 * Indicates how Activity Tags from slots and parent object are combined to be evaluated by an Activity TagQuery from a find request.
	 */
	UPROPERTY(EditAnywhere, config, Category = "SmartObject")
	ESmartObjectTagMergingPolicy DefaultActivityTagsMergingPolicy = ESmartObjectTagMergingPolicy::Override;

	/** Base world condition class for all new Smart Object definitions. */
	UPROPERTY(EditAnywhere, config, Category = "SmartObject")
	TSubclassOf<USmartObjectWorldConditionSchema> DefaultWorldConditionSchemaClass = USmartObjectWorldConditionSchema::StaticClass();

	/**
	 * Indicates whether or not the pre-conditions should be excluded from serializing SmartObjectDefinitions for client builds.
	 * This can be useful for example if the preconditions are server only plugins.
	 * This allows to access the definition data on clients, while the logic is only handled on servers.
	 */
	UPROPERTY(EditAnywhere, config, Category = "SmartObject")
	bool bShouldExcludePreConditionsOnDedicatedClient = false;
};
