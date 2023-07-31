// Copyright Epic Games, Inc. All Rights Reserved.

#include "HLODBuilderMeshSimplify.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(HLODBuilderMeshSimplify)


UHLODBuilderMeshSimplify::UHLODBuilderMeshSimplify(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UHLODBuilderMeshSimplifySettings::UHLODBuilderMeshSimplifySettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsTemplate())
	{
		HLODMaterial = GEngine->DefaultHLODFlattenMaterial;
	}
#endif
}

uint32 UHLODBuilderMeshSimplifySettings::GetCRC() const
{
	UHLODBuilderMeshSimplifySettings& This = *const_cast<UHLODBuilderMeshSimplifySettings*>(this);

	FArchiveCrc32 Ar;

	Ar << This.MeshSimplifySettings;
	UE_LOG(LogHLODBuilder, VeryVerbose, TEXT(" - MeshSimplifySettings = %d"), Ar.GetCrc());

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

TSubclassOf<UHLODBuilderSettings> UHLODBuilderMeshSimplify::GetSettingsClass() const
{
	return UHLODBuilderMeshSimplifySettings::StaticClass();
}

TArray<UActorComponent*> UHLODBuilderMeshSimplify::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UHLODBuilderMeshSimplifySettings::Build);

	TArray<UStaticMeshComponent*> StaticMeshComponents = FilterComponents<UStaticMeshComponent>(InSourceComponents);

	const UHLODBuilderMeshSimplifySettings* MeshSimplifySettings = CastChecked<UHLODBuilderMeshSimplifySettings>(HLODBuilderSettings);
	FMeshProxySettings UseSettings = MeshSimplifySettings->MeshSimplifySettings; // Make a copy as we may tweak some values
	UMaterialInterface* HLODMaterial = MeshSimplifySettings->HLODMaterial.LoadSynchronous();

	// When using automatic textuse sizing based on draw distance, use the MinVisibleDistance for this HLOD.
	if (UseSettings.MaterialSettings.TextureSizingType == TextureSizingType_AutomaticFromMeshDrawDistance)
	{
		UseSettings.MaterialSettings.MeshMinDrawDistance = InHLODBuildContext.MinVisibleDistance;
	}

	// Generate a projection matrix.
	static const float ScreenX = 1920;
	static const float ScreenY = 1080;
	static const float HalfFOVRad = FMath::DegreesToRadians(45.0f);
	static const FMatrix ProjectionMatrix = FPerspectiveMatrix(HalfFOVRad, ScreenX, ScreenY, 0.01f);

	// Gather bounds of the input components
	auto GetComponentsBounds = [&]() -> FBoxSphereBounds
	{
		FBoxSphereBounds Bounds;
		bool bFirst = true;

		for (UActorComponent* Component : InSourceComponents)
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				FBoxSphereBounds ComponentBounds = SceneComponent->Bounds;
				Bounds = bFirst ? ComponentBounds : Bounds + ComponentBounds;
				bFirst = false;
			}
		}

		return Bounds;
	};

	float ScreenSizePercent = ComputeBoundsScreenSize(FVector::ZeroVector, GetComponentsBounds().SphereRadius, FVector(0.0f, 0.0f, InHLODBuildContext.MinVisibleDistance), ProjectionMatrix);
	UseSettings.ScreenSize = ScreenSizePercent * ScreenX;

	TArray<UObject*> Assets;
	FCreateProxyDelegate ProxyDelegate;
	ProxyDelegate.BindLambda([&Assets](const FGuid Guid, TArray<UObject*>& InAssetsCreated) { Assets = InAssetsCreated; });

	const IMeshMergeUtilities& MeshMergeUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
	MeshMergeUtilities.CreateProxyMesh(StaticMeshComponents, UseSettings, HLODMaterial, InHLODBuildContext.AssetsOuter->GetPackage(), InHLODBuildContext.AssetsBaseName, FGuid::NewGuid(), ProxyDelegate, true);


	TArray<UActorComponent*> Components;

	Algo::ForEach(Assets, [this, &Components](UObject* Asset)
	{
		Asset->ClearFlags(RF_Public | RF_Standalone);

		if (Cast<UStaticMesh>(Asset))
		{
			UStaticMeshComponent* SMComponent = NewObject<UStaticMeshComponent>();
			SMComponent->SetStaticMesh(static_cast<UStaticMesh*>(Asset));

			Components.Add(SMComponent);
		}
	});
	
	return Components;
}

