// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLODBuilderMeshMerge.h"

#include "Algo/ForEach.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Components/StaticMeshComponent.h"

#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODLayer.h"

#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"
#include "Modules/ModuleManager.h"

#include "Materials/Material.h"
#include "Engine/HLODProxy.h"
#include "Serialization/ArchiveCrc32.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODBuilderMeshMerge)


UHLODBuilderMeshMerge::UHLODBuilderMeshMerge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHLODBuilderMeshMergeSettings::UHLODBuilderMeshMergeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsTemplate())
	{
		HLODMaterial = GEngine->DefaultHLODFlattenMaterial;
	}
#endif
}

uint32 UHLODBuilderMeshMergeSettings::GetCRC() const
{
	UHLODBuilderMeshMergeSettings& This = *const_cast<UHLODBuilderMeshMergeSettings*>(this);

	FArchiveCrc32 Ar;

	Ar << This.MeshMergeSettings;
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - MeshMergeSettings = %d"), Ar.GetCrc());

	uint32 Hash = Ar.GetCrc();

	if (!HLODMaterial.IsNull())
	{
		UMaterialInterface* Material = HLODMaterial.LoadSynchronous();
		if (Material)
		{
			uint32 MaterialCRC = UHLODProxy::GetCRC(Material);
			UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - Material = %d"), MaterialCRC);
			Hash = HashCombine(Hash, MaterialCRC);
		}
	}

	return Hash;
}

TSubclassOf<UHLODBuilderSettings> UHLODBuilderMeshMerge::GetSettingsClass() const
{
	return UHLODBuilderMeshMergeSettings::StaticClass();
}

TArray<UActorComponent*> UHLODBuilderMeshMerge::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODBuilderMeshMerge::CreateComponents);

	TArray<UPrimitiveComponent*> SourcePrimitiveComponents = FilterComponents<UPrimitiveComponent>(InSourceComponents);

	TArray<UObject*> Assets;
	FVector MergedActorLocation;

	const UHLODBuilderMeshMergeSettings* MeshMergeSettings = CastChecked<UHLODBuilderMeshMergeSettings>(HLODBuilderSettings);
	FMeshMergingSettings UseSettings = MeshMergeSettings->MeshMergeSettings; // Make a copy as we may tweak some values
	UMaterialInterface* HLODMaterial = MeshMergeSettings->HLODMaterial.LoadSynchronous();

	// When using automatic textuse sizing based on draw distance, use the MinVisibleDistance for this HLOD.
	if (UseSettings.MaterialSettings.TextureSizingType == TextureSizingType_AutomaticFromMeshDrawDistance)
	{
		UseSettings.MaterialSettings.MeshMinDrawDistance = InHLODBuildContext.MinVisibleDistance;
	}

	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	MeshMergeUtilities.MergeComponentsToStaticMesh(SourcePrimitiveComponents, InHLODBuildContext.World, UseSettings, HLODMaterial, InHLODBuildContext.AssetsOuter->GetPackage(), InHLODBuildContext.AssetsBaseName, Assets, MergedActorLocation, 0.25f, false);

	UStaticMeshComponent* Component = nullptr;
	Algo::ForEach(Assets, [this, &Component, &MergedActorLocation](UObject* Asset)
	{
		Asset->ClearFlags(RF_Public | RF_Standalone);

		if (Cast<UStaticMesh>(Asset))
		{
			Component = NewObject<UStaticMeshComponent>();
			Component->SetStaticMesh(static_cast<UStaticMesh*>(Asset));
			Component->SetWorldLocation(MergedActorLocation);
		}
	});

	return { Component };
}

