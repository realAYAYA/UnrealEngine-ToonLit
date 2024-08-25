// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamComponentInstanceData.h"

#include "VCamComponent.h"
#include "Output/VCamOutputProviderBase.h"

FVCamComponentInstanceData::FVCamComponentInstanceData(const UVCamComponent* SourceComponent)
	: FSceneComponentInstanceData(SourceComponent)
{
	// Only Blueprint components would deactivate their output providers when the construction script is re-run.
	// Hence, we only need to apply instance cache for Blueprint created components.
	const bool bIsBlueprintCreatedComponent = SourceComponent->CreationMethod == EComponentCreationMethod::SimpleConstructionScript
		|| SourceComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript;
	if (bIsBlueprintCreatedComponent)
	{
		SourceComponent->GetAllOutputProviders(MutableView(StolenOutputProviders));
		AppliedInputContexts = SourceComponent->AppliedInputContexts;
		LiveLinkSubject = SourceComponent->GetLiveLinkSubobject();
	}
}

bool FVCamComponentInstanceData::ContainsData() const
{
	return !StolenOutputProviders.IsEmpty() || !AppliedInputContexts.IsEmpty();
}

void FVCamComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	Super::ApplyToComponent(Component, CacheApplyPhase);
	Cast<UVCamComponent>(Component)->ApplyComponentInstanceData(*this, CacheApplyPhase);
}

void FVCamComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObjects(StolenOutputProviders);
}
