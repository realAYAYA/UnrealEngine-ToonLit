// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"

#include "Engine/Light.h"
#include "GameFramework/Actor.h"

#include "LightMixerObjectFilter.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class LIGHTMIXER_API ULightMixerObjectFilter : public UObjectMixerObjectFilter
{
	GENERATED_BODY()
public:
	
	virtual TSet<UClass*> GetObjectClassesToFilter() const override
	{
		return
		{
			ULightComponent::StaticClass()
		};
	}

	virtual TSet<TSubclassOf<AActor>> GetObjectClassesToPlace() const override
	{
		return
		{
			ALight::StaticClass()
		};
	}

	virtual TSet<FName> GetColumnsToShowByDefault() const override
	{
		return 
		{
			"Intensity", "LightColor", "LightingChannels", "AttenuationRadius"
		};
	}

	virtual TSet<FName> GetForceAddedColumns() const override
	{
		return {"LightColor", "LightingChannels"};
	}

	virtual bool ShouldIncludeUnsupportedProperties() const override
	{
		return false;
	}

	virtual EObjectMixerInheritanceInclusionOptions GetObjectMixerPropertyInheritanceInclusionOptions() const override
	{
		return EObjectMixerInheritanceInclusionOptions::IncludeAllParentsAndChildren;
	}

	virtual EObjectMixerInheritanceInclusionOptions GetObjectMixerPlacementClassInclusionOptions() const override
	{
		return EObjectMixerInheritanceInclusionOptions::IncludeAllChildren;
	}
};
