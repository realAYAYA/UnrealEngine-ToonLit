// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DeprecatedDataLayerInstance.h"
#include "Misc/StringFormatArg.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/DataLayer/DataLayer.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DeprecatedDataLayerInstance)

UDeprecatedDataLayerInstance::UDeprecatedDataLayerInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	DataLayerType(EDataLayerType::Editor)
{

}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

#if WITH_EDITOR

FName UDeprecatedDataLayerInstance::MakeName()
{
	return FName(FString::Format(TEXT("DataLayer_{0}"), { FGuid::NewGuid().ToString() }));
}

void UDeprecatedDataLayerInstance::OnCreated()
{
	check(GetOuterAWorldDataLayers()->HasDeprecatedDataLayers());

	Modify(/*bAlwaysMarkDirty*/false);

	FDataLayerUtils::SetDataLayerShortName(this, TEXT("DataLayer"));

	DeprecatedDataLayerFName = TEXT("");

	DebugColor = FColor::MakeRandomSeededColor(GetTypeHash(GetDataLayerFName().ToString()));
}

void UDeprecatedDataLayerInstance::PostLoad()
{
	if (DebugColor == FColor::Black)
	{
		DebugColor = FColor::MakeRandomSeededColor(GetTypeHash(GetDataLayerFName().ToString()));
	}

	Super::PostLoad();
}

FName UDeprecatedDataLayerInstance::MakeName(const UDEPRECATED_DataLayer* DeprecatedDataLayer)
{
	return FName(FString::Format(TEXT("{0}_{1}"), { *DeprecatedDataLayer->GetDataLayerLabel().ToString(), FGuid::NewGuid().ToString() }));
}

void UDeprecatedDataLayerInstance::OnCreated(const UDEPRECATED_DataLayer* DeprecatedDataLayer)
{
	check(GetOuterAWorldDataLayers()->HasDeprecatedDataLayers());

	Modify(/*bAlwaysMarkDirty*/false);

	Label = DeprecatedDataLayer->GetDataLayerLabel();

	DeprecatedDataLayerFName = DeprecatedDataLayer->GetFName();

	DataLayerType = DeprecatedDataLayer->IsRuntime() ? EDataLayerType::Runtime : EDataLayerType::Editor;
	DebugColor = DeprecatedDataLayer->GetDebugColor();

	bIsVisible = DeprecatedDataLayer->bIsVisible;
	bIsInitiallyVisible = DeprecatedDataLayer->bIsInitiallyVisible;
	bIsInitiallyLoadedInEditor = DeprecatedDataLayer->bIsInitiallyLoadedInEditor;
	bIsLocked = DeprecatedDataLayer->bIsLocked;
	InitialRuntimeState = DeprecatedDataLayer->InitialRuntimeState;
}

FActorDataLayer UDeprecatedDataLayerInstance::GetActorDataLayer() const
{
	return FActorDataLayer(GetDataLayerFName());
}

bool UDeprecatedDataLayerInstance::PerformAddActor(AActor* InActor) const
{
	return InActor->AddDataLayer(GetActorDataLayer());
}

bool UDeprecatedDataLayerInstance::PerformRemoveActor(AActor* InActor) const
{
	return InActor->RemoveDataLayer(GetActorDataLayer());
}

bool UDeprecatedDataLayerInstance::RelabelDataLayer(FName InDataLayerLabel)
{
	return FDataLayerUtils::SetDataLayerShortName(this, InDataLayerLabel.ToString());
}

#endif // WITH_EDITOR

PRAGMA_ENABLE_DEPRECATION_WARNINGS
