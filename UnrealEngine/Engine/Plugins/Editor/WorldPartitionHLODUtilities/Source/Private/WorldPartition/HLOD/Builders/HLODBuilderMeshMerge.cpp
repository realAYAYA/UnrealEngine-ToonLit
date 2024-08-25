// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/Builders/HLODBuilderMeshMerge.h"

#include "Algo/ForEach.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Components/StaticMeshComponent.h"

#include "IMaterialBakingModule.h"
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

	// Base mesh merge key, changing this will force a rebuild of all HLODs from this builder
	FString HLODBaseKey = "B8DB9CB1780C4EE1B80A36D9E205AA0F";
	Ar << HLODBaseKey;

	Ar << This.MeshMergeSettings;
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - MeshMergeSettings = %d"), Ar.GetCrc());

	if (MeshMergeSettings.bMergeMaterials)
	{
		IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
		uint32 MaterialBakingModuleCRC = Module.GetCRC();
		Ar << MaterialBakingModuleCRC;

		static const auto MeshMergeUtilitiesUVGenerationMethodCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("MeshMergeUtilities.UVGenerationMethod"));
		int32 MeshMergeUtilitiesUVGenerationMethod = (MeshMergeUtilitiesUVGenerationMethodCVar != nullptr) ? MeshMergeUtilitiesUVGenerationMethodCVar->GetInt() : 0;
		Ar << MeshMergeUtilitiesUVGenerationMethod;
	}

	uint32 Hash = Ar.GetCrc();

	if (HLODMaterial)
	{
		uint32 MaterialCRC = UHLODProxy::GetCRC(HLODMaterial);
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - Material = %d"), MaterialCRC);
		Hash = HashCombine(Hash, MaterialCRC);
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
	UMaterialInterface* HLODMaterial = MeshMergeSettings->HLODMaterial;

	// When using automatic texture sizing based on draw distance, use the MinVisibleDistance for this HLOD.
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

