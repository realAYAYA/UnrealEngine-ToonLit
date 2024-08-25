// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaShapeFactory.h"
#include "AvaShapeActor.h"
#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "DynamicMeshes/AvaShape3DDynMeshBase.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Tools/AvaShapesEditorShapeToolBase.h"

UAvaShapeFactory::UAvaShapeFactory()
{
	NewActorClass = AAvaShapeActor::StaticClass();
}

void UAvaShapeFactory::SetMeshClass(TSubclassOf<UAvaShapeDynamicMeshBase> InMeshClass)
{
	MeshClass = InMeshClass;
}

bool UAvaShapeFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!MeshClass)
	{
		return false;
	}

	if (UClass* AssetClass = AssetData.GetClass())
	{
		return AssetClass->IsChildOf<AAvaShapeActor>();
	}

	return false;
}

AActor* UAvaShapeFactory::GetDefaultActor(const FAssetData& AssetData)
{
	return Cast<AActor>(AAvaShapeActor::StaticClass()->GetDefaultObject());
}

AActor* UAvaShapeFactory::SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams)
{
	UWorld* World = InLevel->GetWorld();

	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.OverrideLevel = InLevel;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bNoFail = true;
	Params.ObjectFlags = RF_Transactional;

	AAvaShapeActor* ShapeActor = World->SpawnActor<AAvaShapeActor>(AAvaShapeActor::StaticClass(), InTransform, Params);

	UAvaShapeDynamicMeshBase* MeshBase = NewObject<UAvaShapeDynamicMeshBase>(ShapeActor, MeshClass.Get());

	if (UAvaShape2DDynMeshBase* Mesh2D = Cast<UAvaShape2DDynMeshBase>(MeshBase))
	{
		Mesh2D->SetSize2D({
			UAvaShapesEditorShapeToolBase::DefaultDim,
			UAvaShapesEditorShapeToolBase::DefaultDim
		});
	}
	else if (UAvaShape3DDynMeshBase* Mesh3D = Cast<UAvaShape3DDynMeshBase>(MeshBase))
	{
		Mesh3D->SetSize3D({
			UAvaShapesEditorShapeToolBase::DefaultDim,
			UAvaShapesEditorShapeToolBase::DefaultDim,
			UAvaShapesEditorShapeToolBase::DefaultDim
		});
	}

	const FAvaShapeParametricMaterial DefaultParametricMaterial;
	MeshBase->SetParametricMaterial(UAvaShapeDynamicMeshBase::MESH_INDEX_PRIMARY, DefaultParametricMaterial);

	ShapeActor->SetDynamicMesh(MeshBase);

	return ShapeActor;
}

void UAvaShapeFactory::PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	Super::PostPlaceAsset(InHandle, InPlacementInfo, InPlacementOptions);

	if (!InPlacementOptions.bIsCreatingPreviewElements && FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		if (MeshClass)
		{
			Attributes.Emplace(TEXT("MeshClass"), MeshClass->GetName());	
		}
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MotionDesign.PlaceShape"), Attributes);
	}
}
