// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetUtils/CreateStaticMeshUtil.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

#include "PhysicsEngine/BodySetup.h"

#include "MeshDescription.h"
#include "DynamicMeshToMeshDescription.h"

using namespace UE::AssetUtils;

using namespace UE::Geometry;


UE::AssetUtils::ECreateStaticMeshResult UE::AssetUtils::CreateStaticMeshAsset(
	FStaticMeshAssetOptions& Options,
	FStaticMeshResults& ResultsOut)
{
	FString NewObjectName = FPackageName::GetLongPackageAssetName(Options.NewAssetPath);

	UPackage* UsePackage;
	if (Options.UsePackage != nullptr)
	{
		UsePackage = Options.UsePackage;
	}
	else
	{
		UsePackage = CreatePackage(*Options.NewAssetPath);
	}
	if (ensure(UsePackage != nullptr) == false)
	{
		return ECreateStaticMeshResult::InvalidPackage;
	}

	// create new UStaticMesh object
	EObjectFlags UseFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
	UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(UsePackage, FName(*NewObjectName), UseFlags);
	if (ensure(NewStaticMesh != nullptr) == false)
	{
		return ECreateStaticMeshResult::UnknownError;
	}

	// initialize the MeshDescription SourceModel LODs
	int32 UseNumSourceModels = FMath::Max(1, Options.NumSourceModels);
	NewStaticMesh->SetNumSourceModels(UseNumSourceModels);
	for (int32 k = 0; k < UseNumSourceModels; ++k)
	{
		FMeshBuildSettings& BuildSettings = NewStaticMesh->GetSourceModel(k).BuildSettings;

		BuildSettings.bRecomputeNormals = Options.bEnableRecomputeNormals;
		BuildSettings.bRecomputeTangents = Options.bEnableRecomputeTangents;
		BuildSettings.bGenerateLightmapUVs = Options.bGenerateLightmapUVs;

		if (!Options.bAllowDistanceField)
		{
			BuildSettings.DistanceFieldResolutionScale = 0.0f;
		}

		NewStaticMesh->CreateMeshDescription(k);
	}

	// create physics body and configure appropriately
	if (Options.bCreatePhysicsBody)
	{
		NewStaticMesh->CreateBodySetup();
		NewStaticMesh->GetBodySetup()->CollisionTraceFlag = Options.CollisionType;
	}

	// add a material slot. Must always have one material slot.
	int32 UseNumMaterialSlots = FMath::Max(1, Options.NumMaterialSlots);
	for (int MatIdx = 0; MatIdx < UseNumMaterialSlots; MatIdx++)
	{
		NewStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	}

	// set materials if the count matches
	if (Options.AssetMaterials.Num() == UseNumMaterialSlots)
	{
		for (int MatIdx = 0; MatIdx < UseNumMaterialSlots; MatIdx++)
		{
			NewStaticMesh->SetMaterial(MatIdx, Options.AssetMaterials[MatIdx]);
		}
	}

	// determine maximum number of sections across all mesh LODs
	int32 MaxNumSections = 0;

	// if options included SourceModel meshes, copy them over
	if (Options.SourceMeshes.MoveMeshDescriptions.Num() > 0)
	{
		if (ensure(Options.SourceMeshes.MoveMeshDescriptions.Num() == UseNumSourceModels))
		{
			for (int32 k = 0; k < UseNumSourceModels; ++k)
			{
				FMeshDescription* Mesh = NewStaticMesh->GetMeshDescription(k);
				*Mesh = MoveTemp(*Options.SourceMeshes.MoveMeshDescriptions[k]);
				MaxNumSections = FMath::Max(MaxNumSections, Mesh->PolygonGroups().Num());
				NewStaticMesh->CommitMeshDescription(k);
			}
		}
	}
	else if (Options.SourceMeshes.MeshDescriptions.Num() > 0)
	{
		if (ensure(Options.SourceMeshes.MeshDescriptions.Num() == UseNumSourceModels))
		{
			for (int32 k = 0; k < UseNumSourceModels; ++k)
			{
				FMeshDescription* Mesh = NewStaticMesh->GetMeshDescription(k);
				*Mesh = *Options.SourceMeshes.MeshDescriptions[k];
				MaxNumSections = FMath::Max(MaxNumSections, Mesh->PolygonGroups().Num());
				NewStaticMesh->CommitMeshDescription(k);
			}
		}
	}
	else if (Options.SourceMeshes.DynamicMeshes.Num() > 0)
	{
		if (ensure(Options.SourceMeshes.DynamicMeshes.Num() == UseNumSourceModels))
		{
			for (int32 k = 0; k < UseNumSourceModels; ++k)
			{
				FMeshDescription* Mesh = NewStaticMesh->GetMeshDescription(k);
				FDynamicMeshToMeshDescription Converter;
				Converter.Convert(Options.SourceMeshes.DynamicMeshes[k], *Mesh, !Options.bEnableRecomputeTangents);
				MaxNumSections = FMath::Max(MaxNumSections, Mesh->PolygonGroups().Num());
				NewStaticMesh->CommitMeshDescription(k);
			}
		}
	}

	// Ensure that we have a Material Slot for each Section that exists on the mesh.
	// This is not technically correct as the Sections on the MeshDescription (ie PolygonGroups)
	// may reference any MaterialSlot (via the PolygonGroup::ImportedMaterialSlotName attribute),
	// so it is valid for there to be fewer Materials than Sections. *However* if a Section does
	// not have an ImportedMaterialSlotName, then the Section Index is used as Material Slot Index,
	// and if that Material Slot Index does not exist, we are in a bit of an undefined state, for
	// example the Section will not appear in the Static Mesh Editor, and various other code that
	// does not properly handle Sections/Slots will be broken. 
	// So, some of these extra Slots may be unused, but this is not a catastrophe. Note that the
	// extra Slots will end up with no Material assigned.
	int32 CurNumValidSections = UseNumMaterialSlots;
	while (CurNumValidSections < MaxNumSections)
	{
		NewStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
		CurNumValidSections++;
	}

	// Nanite options
	NewStaticMesh->NaniteSettings.bEnabled = false;
	if (Options.bGenerateNaniteEnabledMesh)
	{
		NewStaticMesh->NaniteSettings = Options.NaniteSettings;
	}

	// Ray tracing
	NewStaticMesh->bSupportRayTracing = Options.bSupportRayTracing;

	// Distance field
	NewStaticMesh->bGenerateMeshDistanceField = Options.bAllowDistanceField;

	[[maybe_unused]] bool MarkedDirty = NewStaticMesh->MarkPackageDirty();
	if (Options.bDeferPostEditChange == false)
	{
		NewStaticMesh->PostEditChange();
	}

	ResultsOut.StaticMesh = NewStaticMesh;
	return ECreateStaticMeshResult::Ok;
}



