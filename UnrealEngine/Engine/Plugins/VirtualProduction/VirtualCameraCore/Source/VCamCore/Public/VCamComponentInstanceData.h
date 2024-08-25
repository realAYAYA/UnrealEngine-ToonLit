// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/SceneComponent.h"
#include "LiveLinkTypes.h"
#include "VCamComponentInstanceData.generated.h"

class UInputMappingContext;
class UVCamComponent;
class UVCamOutputProviderBase;

struct FModifierStackEntry;

/** Saves internal UVCamComponent state for Blueprint created components. */
USTRUCT()
struct VCAMCORE_API FVCamComponentInstanceData : public FSceneComponentInstanceData
{
	GENERATED_BODY()
	
	FVCamComponentInstanceData() = default;
	FVCamComponentInstanceData(const UVCamComponent* SourceComponent);
	
	virtual bool ContainsData() const override;
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	/** These output providers are renamed and re-outered to the component they're applied to. The owning Blueprint component makes sure to not reference the old instances in OnComponentDestroyed. */
	TArray<TObjectPtr<UVCamOutputProviderBase>> StolenOutputProviders;

	/** Simple copy for carrying over player remappings */
	TArray<TObjectPtr<UInputMappingContext>> AppliedInputContexts;
	
	/** The subject name would be lost when the component is reconstructed so keep track of it. */
	FLiveLinkSubjectName LiveLinkSubject;
};
