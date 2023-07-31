// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/MeshAssetFunctions.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"

#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "RenderingThread.h"
#include "PhysicsEngine/BodySetup.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"

#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "AssetUtils/StaticMeshMaterialUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshAssetFunctions)


#if WITH_EDITOR
#include "Editor.h"
#include "ScopedTransaction.h"
#endif

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_MeshAssetFunctions"




static UDynamicMesh* CopyMeshFromStaticMesh_SourceData(	
	UStaticMesh* FromStaticMeshAsset, 
	UDynamicMesh* ToDynamicMesh, 
	FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
	FGeometryScriptMeshReadLOD RequestedLOD,
	TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug
)
{
	if (RequestedLOD.LODType != EGeometryScriptLODType::MaxAvailable && RequestedLOD.LODType != EGeometryScriptLODType::SourceModel)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromStaticMesh_LODNotAvailable", "CopyMeshFromStaticMesh: Requested LOD Type is not available"));
		return ToDynamicMesh;
	}

#if WITH_EDITOR
	int32 UseLODIndex = FMath::Clamp(RequestedLOD.LODIndex, 0, FromStaticMeshAsset->GetNumSourceModels() - 1);

	const FMeshDescription* SourceMesh = FromStaticMeshAsset->GetMeshDescription(UseLODIndex);
	if (SourceMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromStaticMesh_SourceLODIsNull", "CopyMeshFromStaticMesh: Requested SourceModel LOD is null, only RenderData Mesh is available"));
		return ToDynamicMesh;
	}

	const FStaticMeshSourceModel& SourceModel = FromStaticMeshAsset->GetSourceModel(UseLODIndex);
	const FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;

	bool bHasDirtyBuildSettings = BuildSettings.bRecomputeNormals
		|| (BuildSettings.bRecomputeTangents && AssetOptions.bRequestTangents);

	FMeshDescription LocalSourceMeshCopy;
	if (AssetOptions.bApplyBuildSettings && bHasDirtyBuildSettings )
	{
		LocalSourceMeshCopy = *SourceMesh;

		FStaticMeshAttributes Attributes(LocalSourceMeshCopy);
		if (!Attributes.GetTriangleNormals().IsValid() || !Attributes.GetTriangleTangents().IsValid())
		{
			// If these attributes don't exist, create them and compute their values for each triangle
			FStaticMeshOperations::ComputeTriangleTangentsAndNormals(LocalSourceMeshCopy);
		}

		EComputeNTBsFlags ComputeNTBsOptions = EComputeNTBsFlags::BlendOverlappingNormals;
		ComputeNTBsOptions |= BuildSettings.bRecomputeNormals ? EComputeNTBsFlags::Normals : EComputeNTBsFlags::None;
		if (AssetOptions.bRequestTangents)
		{
			ComputeNTBsOptions |= BuildSettings.bRecomputeTangents ? EComputeNTBsFlags::Tangents : EComputeNTBsFlags::None;
			ComputeNTBsOptions |= BuildSettings.bUseMikkTSpace ? EComputeNTBsFlags::UseMikkTSpace : EComputeNTBsFlags::None;
		}
		ComputeNTBsOptions |= BuildSettings.bComputeWeightedNormals ? EComputeNTBsFlags::WeightedNTBs : EComputeNTBsFlags::None;
		if (AssetOptions.bIgnoreRemoveDegenerates == false)
		{
			ComputeNTBsOptions |= BuildSettings.bRemoveDegenerates ? EComputeNTBsFlags::IgnoreDegenerateTriangles : EComputeNTBsFlags::None;
		}

		FStaticMeshOperations::ComputeTangentsAndNormals(LocalSourceMeshCopy, ComputeNTBsOptions);

		SourceMesh = &LocalSourceMeshCopy;
	}

	FDynamicMesh3 NewMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(SourceMesh, NewMesh, AssetOptions.bRequestTangents);

	ToDynamicMesh->SetMesh(MoveTemp(NewMesh));

	Outcome = EGeometryScriptOutcomePins::Success;
#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromAsset_EditorOnly", "CopyMeshFromStaticMesh: Source Models are not available at Runtime"));
#endif

	return ToDynamicMesh;
}



static UDynamicMesh* CopyMeshFromStaticMesh_RenderData(	
	UStaticMesh* FromStaticMeshAsset, 
	UDynamicMesh* ToDynamicMesh, 
	FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
	FGeometryScriptMeshReadLOD RequestedLOD,
	TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug
)
{
	if (RequestedLOD.LODType != EGeometryScriptLODType::MaxAvailable && RequestedLOD.LODType != EGeometryScriptLODType::RenderData)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromStaticMesh_LODNotAvailable", "CopyMeshFromStaticMesh: Requested LOD Type is not available"));
		return ToDynamicMesh;
	}

#if !WITH_EDITOR
	if (FromStaticMeshAsset->bAllowCPUAccess == false)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromStaticMesh_CPUAccess", "CopyMeshFromStaticMesh: StaticMesh bAllowCPUAccess must be set to true to read mesh data at Runtime"));
		return ToDynamicMesh;
	}
#endif

	int32 UseLODIndex = FMath::Clamp(RequestedLOD.LODIndex, 0, FromStaticMeshAsset->GetNumLODs() - 1);

	const FStaticMeshLODResources* LODResources = nullptr;
	if (FStaticMeshRenderData* RenderData = FromStaticMeshAsset->GetRenderData())
	{
		LODResources = &RenderData->LODResources[UseLODIndex];
	}
	if (LODResources == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromStaticMesh_NoLODResources", "CopyMeshFromStaticMesh: LOD Data is not available"));
		return ToDynamicMesh;
	}

	FStaticMeshLODResourcesToDynamicMesh::ConversionOptions ConvertOptions;
#if WITH_EDITOR
	// respect BuildScale build setting
	const FMeshBuildSettings& LODBuildSettings = FromStaticMeshAsset->GetSourceModel(UseLODIndex).BuildSettings;
	ConvertOptions.BuildScale = (FVector3d)LODBuildSettings.BuildScale3D;
#endif

	FDynamicMesh3 NewMesh;
	FStaticMeshLODResourcesToDynamicMesh Converter;
	Converter.Convert(LODResources, ConvertOptions, NewMesh);

	ToDynamicMesh->SetMesh(MoveTemp(NewMesh));
	Outcome = EGeometryScriptOutcomePins::Success;
	return ToDynamicMesh;
}



UDynamicMesh*  UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
	UStaticMesh* FromStaticMeshAsset, 
	UDynamicMesh* ToDynamicMesh, 
	FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
	FGeometryScriptMeshReadLOD RequestedLOD,
	TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromAsset_InvalidInput1", "CopyMeshFromStaticMesh: FromStaticMeshAsset is Null"));
		return ToDynamicMesh;
	}
	if (ToDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromAsset_InvalidInput2", "CopyMeshFromStaticMesh: ToDynamicMesh is Null"));
		return ToDynamicMesh;
	}

#if WITH_EDITOR
	if (RequestedLOD.LODType == EGeometryScriptLODType::RenderData)
	{
		return CopyMeshFromStaticMesh_RenderData(FromStaticMeshAsset, ToDynamicMesh, AssetOptions, RequestedLOD, Outcome, Debug);
	}
	else
	{
		return CopyMeshFromStaticMesh_SourceData(FromStaticMeshAsset, ToDynamicMesh, AssetOptions, RequestedLOD, Outcome, Debug);
	}
#else
	return CopyMeshFromStaticMesh_RenderData(FromStaticMeshAsset, ToDynamicMesh, AssetOptions, RequestedLOD, Outcome, Debug);	
#endif


	return ToDynamicMesh;
}




UDynamicMesh*  UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(
	UDynamicMesh* FromDynamicMesh,
	UStaticMesh* ToStaticMeshAsset,
	FGeometryScriptCopyMeshToAssetOptions Options,
	FGeometryScriptMeshWriteLOD TargetLOD,
	TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_InvalidInput1", "CopyMeshToStaticMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}
	if (ToStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_InvalidInput2", "CopyMeshToStaticMesh: ToStaticMeshAsset is Null"));
		return FromDynamicMesh;
	}

#if WITH_EDITOR

	int32 UseLODIndex = FMath::Clamp(TargetLOD.LODIndex, 0, 32);

	if (Options.bReplaceMaterials && UseLODIndex != 0)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToStaticMesh_InvalidOptions1", "CopyMeshToStaticMesh: Can only Replace Materials when updating LOD0"));
		return FromDynamicMesh;
	}

	// Don't allow built-in engine assets to be modified. However we do allow assets in /Engine/Transient/ to be updated because
	// this is a location that temporary assets in the Transient package will be created in, and in some cases we want to use
	// script functions on such asset (Datasmith does this for example)
	if ( ToStaticMeshAsset->GetPathName().StartsWith(TEXT("/Engine/")) && ToStaticMeshAsset->GetPathName().StartsWith(TEXT("/Engine/Transient")) == false )
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_EngineAsset", "CopyMeshToStaticMesh: Cannot modify built-in Engine asset"));
		return FromDynamicMesh;
	}

	// flush any pending rendering commands, which might want to touch this StaticMesh while we are rebuilding it
	FlushRenderingCommands();

	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateStaticMesh", "Update Static Mesh"));
	}

	// make sure transactional flag is on for the Asset
	ToStaticMeshAsset->SetFlags(RF_Transactional);
	// mark as modified
	ToStaticMeshAsset->Modify();

	auto ConfigureBuildSettingsFromOptions = [](FStaticMeshSourceModel& SourceModel, FGeometryScriptCopyMeshToAssetOptions& Options)
												{
													FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;
													BuildSettings.bRecomputeNormals  = Options.bEnableRecomputeNormals;
													BuildSettings.bRecomputeTangents = Options.bEnableRecomputeTangents;
													BuildSettings.bRemoveDegenerates = Options.bEnableRemoveDegenerates;
												};

	if (TargetLOD.bWriteHiResSource)
	{
		// update model build settings
		ConfigureBuildSettingsFromOptions(ToStaticMeshAsset->GetHiResSourceModel(), Options);

		ToStaticMeshAsset->ModifyHiResMeshDescription();
		FMeshDescription* NewHiResMD = ToStaticMeshAsset->CreateHiResMeshDescription();

		if (!ensure(NewHiResMD != nullptr))
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_NullHiResMeshDescription", "CopyMeshToAsset: MeshDescription for HiRes is null?"));
			return FromDynamicMesh;
		}

		FConversionToMeshDescriptionOptions ConversionOptions;
		FDynamicMeshToMeshDescription Converter(ConversionOptions);
		FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
			{
				Converter.Convert(&ReadMesh, *NewHiResMD, !Options.bEnableRecomputeTangents);
			});


		ToStaticMeshAsset->CommitHiResMeshDescription();
	}
	else
	{ 

		if (ToStaticMeshAsset->GetNumSourceModels() < UseLODIndex+1)
		{
			ToStaticMeshAsset->SetNumSourceModels(UseLODIndex+1);
		}

		// update model build settings
		ConfigureBuildSettingsFromOptions(ToStaticMeshAsset->GetSourceModel(UseLODIndex), Options);

		FMeshDescription* MeshDescription = ToStaticMeshAsset->GetMeshDescription(UseLODIndex);
		if (MeshDescription == nullptr)
		{
			MeshDescription = ToStaticMeshAsset->CreateMeshDescription(UseLODIndex);
		}

		// mark mesh description for modify
		ToStaticMeshAsset->ModifyMeshDescription(UseLODIndex);

		if (!ensure(MeshDescription != nullptr))
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs,
				FText::Format(LOCTEXT("CopyMeshToAsset_NullMeshDescription", "CopyMeshToAsset: MeshDescription for LOD {0} is null?"), FText::AsNumber(UseLODIndex)));
			return FromDynamicMesh;
		}

		FConversionToMeshDescriptionOptions ConversionOptions;
		FDynamicMeshToMeshDescription Converter(ConversionOptions);
		FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
		{
			Converter.Convert(&ReadMesh, *MeshDescription, !Options.bEnableRecomputeTangents);
		});

		// Setting to prevent the standard static mesh reduction from running and replacing the render LOD.
		FStaticMeshSourceModel& ThisSourceModel = ToStaticMeshAsset->GetSourceModel(UseLODIndex);
		ThisSourceModel.ReductionSettings.PercentTriangles = 1.f;
		ThisSourceModel.ReductionSettings.PercentVertices = 1.f;

		if (Options.bApplyNaniteSettings)
		{
			ToStaticMeshAsset->NaniteSettings = Options.NewNaniteSettings;
		}

		if (Options.bReplaceMaterials)
		{
			bool bHaveSlotNames = (Options.NewMaterialSlotNames.Num() == Options.NewMaterials.Num());

			TArray<FStaticMaterial> NewMaterials;
			for (int32 k = 0; k < Options.NewMaterials.Num(); ++k)
			{
				FStaticMaterial NewMaterial;
				NewMaterial.MaterialInterface = Options.NewMaterials[k];
				FName UseSlotName = (bHaveSlotNames && Options.NewMaterialSlotNames[k] != NAME_None) ? Options.NewMaterialSlotNames[k] :
					UE::AssetUtils::GenerateNewMaterialSlotName(NewMaterials, NewMaterial.MaterialInterface, k);

				NewMaterial.MaterialSlotName = UseSlotName;
				NewMaterial.ImportedMaterialSlotName = UseSlotName;
				NewMaterial.UVChannelData = FMeshUVChannelInfo(1.f);		// this avoids an ensure in  UStaticMesh::GetUVChannelData
				NewMaterials.Add(NewMaterial);
			}

			ToStaticMeshAsset->SetStaticMaterials(NewMaterials);

			// Reset the section info map
			ToStaticMeshAsset->GetSectionInfoMap().Clear();
		}

		ToStaticMeshAsset->CommitMeshDescription(UseLODIndex);
	}

	if (Options.bDeferMeshPostEditChange == false)
	{
		ToStaticMeshAsset->PostEditChange();
	}

	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->EndTransaction();
	}

	Outcome = EGeometryScriptOutcomePins::Success;

#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToAsset_EditorOnly", "CopyMeshToStaticMesh: Not currently supported at Runtime"));
#endif

	return FromDynamicMesh;
}






void UGeometryScriptLibrary_StaticMeshFunctions::GetSectionMaterialListFromStaticMesh(
	UStaticMesh* FromStaticMeshAsset, 
	FGeometryScriptMeshReadLOD RequestedLOD,
	TArray<UMaterialInterface*>& MaterialList,
	TArray<int32>& MaterialIndex,
	TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_InvalidInput1", "GetSectionMaterialListFromStaticMesh: FromStaticMeshAsset is Null"));
		return;
	}
	if (RequestedLOD.LODType != EGeometryScriptLODType::MaxAvailable && RequestedLOD.LODType != EGeometryScriptLODType::SourceModel)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_LODNotAvailable", "GetSectionMaterialListFromStaticMesh: Requested LOD is not available"));
		return;
	}

	int32 UseLODIndex = FMath::Clamp(RequestedLOD.LODIndex, 0, FromStaticMeshAsset->GetNumLODs() - 1);

	MaterialList.Reset();
	MaterialIndex.Reset();
	if (UE::AssetUtils::GetStaticMeshLODMaterialListBySection(FromStaticMeshAsset, UseLODIndex, MaterialList, MaterialIndex) == false)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSectionMaterialListFromStaticMesh_QueryFailed", "GetSectionMaterialListFromStaticMesh: Could not fetch Material Set from Asset"));
		return;
	}

	Outcome = EGeometryScriptOutcomePins::Success;
}


UDynamicMesh* UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
		USkeletalMesh* FromSkeletalMeshAsset, 
		UDynamicMesh* ToDynamicMesh,
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
		FGeometryScriptMeshReadLOD RequestedLOD,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromSkeletalMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_InvalidInput1", "CopyMeshFromSkeletalMesh: FromSkeletalMeshAsset is Null"));
		return ToDynamicMesh;
	}
	if (ToDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_InvalidInput2", "CopyMeshFromSkeletalMesh: ToDynamicMesh is Null"));
		return ToDynamicMesh;
	}
	if (RequestedLOD.LODType != EGeometryScriptLODType::MaxAvailable && RequestedLOD.LODType != EGeometryScriptLODType::SourceModel)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_LODNotAvailable", "CopyMeshFromSkeletalMesh: Requested LOD is not available"));
		return ToDynamicMesh;
	}

	// TODO: Consolidate this code with SkeletalMeshToolTarget::GetMeshDescription(..) 
#if WITH_EDITOR
	const int32 UseLODIndex = FMath::Clamp(RequestedLOD.LODIndex, 0, FromSkeletalMeshAsset->GetLODNum() - 1);;
	
	FMeshDescription SourceMesh;

	// Check first if we have bulk data available and non-empty.
	if (FromSkeletalMeshAsset->IsLODImportedDataBuildAvailable(UseLODIndex) && !FromSkeletalMeshAsset->IsLODImportedDataEmpty(UseLODIndex))
	{
		FSkeletalMeshImportData SkeletalMeshImportData;
		FromSkeletalMeshAsset->LoadLODImportedData(UseLODIndex, SkeletalMeshImportData);
		SkeletalMeshImportData.GetMeshDescription(SourceMesh);
	}
	else
	{
		// Fall back on the LOD model directly if no bulk data exists. When we commit
		// the mesh description, we override using the bulk data. This can happen for older
		// skeletal meshes, from UE 4.24 and earlier.
		const FSkeletalMeshModel* SkeletalMeshModel = FromSkeletalMeshAsset->GetImportedModel();
		if (SkeletalMeshModel && SkeletalMeshModel->LODModels.IsValidIndex(UseLODIndex))
		{
			SkeletalMeshModel->LODModels[UseLODIndex].GetMeshDescription(SourceMesh, FromSkeletalMeshAsset);
		}			
	}

	FDynamicMesh3 NewMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(&SourceMesh, NewMesh, AssetOptions.bRequestTangents);
	
	ToDynamicMesh->SetMesh(MoveTemp(NewMesh));
	
	Outcome = EGeometryScriptOutcomePins::Success;
#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSkeletalMesh_EditorOnly", "CopyMeshFromSkeletalMesh: Not currently supported at Runtime"));
#endif
	
	return ToDynamicMesh;
}


UDynamicMesh* UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(
		UDynamicMesh* FromDynamicMesh, 
		USkeletalMesh* ToSkeletalMeshAsset,
		FGeometryScriptCopyMeshToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_InvalidInput1", "CopyMeshToSkeletalMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}
	if (ToSkeletalMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_InvalidInput2", "CopyMeshToSkeletalMesh: ToSkeletalMeshAsset is Null"));
		return FromDynamicMesh;
	}
	if (TargetLOD.bWriteHiResSource)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_Unsupported", "CopyMeshToSkeletalMesh: Writing HiResSource LOD is not yet supported"));
		return FromDynamicMesh;
	}

	// TODO: Consolidate this code with SkeletalMeshToolTarget::CommitMeshDescription
#if WITH_EDITOR
	if (ToSkeletalMeshAsset->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		const FText Error = FText::Format(LOCTEXT("CopyMeshToSkeletalMesh_BuiltInAsset", "CopyMeshToSkeletalMesh: Cannot modify built-in engine asset: {0}"), FText::FromString(*ToSkeletalMeshAsset->GetPathName()));
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, Error);
		return FromDynamicMesh;
	}

	// flush any pending rendering commands, which might touch a component while we are rebuilding it's mesh
	FlushRenderingCommands();

	if (Options.bEmitTransaction)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateSkeletalMesh", "Update Skeletal Mesh"));
	}

	// make sure transactional flag is on for this asset
	ToSkeletalMeshAsset->SetFlags(RF_Transactional);

	verify(ToSkeletalMeshAsset->Modify());

	FMeshDescription MeshDescription;
	FSkeletalMeshAttributes MeshAttributes(MeshDescription);
	MeshAttributes.Register();
	
	FConversionToMeshDescriptionOptions ConversionOptions;
	FDynamicMeshToMeshDescription Converter(ConversionOptions);
	FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		Converter.Convert(&ReadMesh, MeshDescription, !Options.bEnableRecomputeTangents);
	});

	FSkeletalMeshImportData SkeletalMeshImportData = 
		FSkeletalMeshImportData::CreateFromMeshDescription(MeshDescription);
	ToSkeletalMeshAsset->SaveLODImportedData(TargetLOD.LODIndex, SkeletalMeshImportData);

	// Make sure the mesh builder knows it's the latest variety, so that the render data gets
	// properly rebuilt.
	ToSkeletalMeshAsset->SetLODImportedDataVersions(TargetLOD.LODIndex, ESkeletalMeshGeoImportVersions::LatestVersion, ESkeletalMeshSkinningImportVersions::LatestVersion);
	ToSkeletalMeshAsset->SetUseLegacyMeshDerivedDataKey(false);

	// TODO: Options.bReplaceMaterials support?

	if (Options.bDeferMeshPostEditChange == false)
	{
		ToSkeletalMeshAsset->PostEditChange();
	}

	if (Options.bEmitTransaction)
	{
		GEditor->EndTransaction();
	}
	
	Outcome = EGeometryScriptOutcomePins::Success;
#else
	UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshToSkeletalMesh_EditorOnly", "CopyMeshToSkeletalMesh: Not currently supported at Runtime"));
#endif
	
	return FromDynamicMesh;
}



#undef LOCTEXT_NAMESPACE
