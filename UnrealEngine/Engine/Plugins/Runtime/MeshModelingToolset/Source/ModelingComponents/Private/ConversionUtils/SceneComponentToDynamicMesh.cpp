// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversionUtils/SceneComponentToDynamicMesh.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "UObject/Package.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "StaticMeshComponentLODInfo.h"
#include "ConversionUtils/VolumeToDynamicMesh.h"
#include "ConversionUtils/SkinnedMeshToDynamicMesh.h"
#include "ConversionUtils/SplineComponentDeformDynamicMesh.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMeshEditor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Physics/ComponentCollisionUtil.h"
#include "PlanarCut.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshLODResourcesToDynamicMesh.h"
#include "StaticMeshOperations.h"

#define LOCTEXT_NAMESPACE "ModelingComponents_SceneComponentToDynamicMesh"

namespace UE 
{
namespace Conversion
{

bool CanConvertSceneComponentToDynamicMesh(USceneComponent* Component)
{
	if (!Component)
	{
		return false;
	}
	else if (const USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
	{
#if WITH_EDITOR
		const USkinnedAsset* SkinnedAsset = (!SkinnedMeshComponent->IsUnreachable() && SkinnedMeshComponent->IsValidLowLevel()) ? SkinnedMeshComponent->GetSkinnedAsset() : nullptr;
		return SkinnedAsset && !SkinnedAsset->GetOutermost()->bIsCookedForEditor;
#else
		return true;
#endif
	}
	else if (Cast<USplineMeshComponent>(Component))
	{
		return true;
	}
	else if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
#if WITH_EDITOR
		const UStaticMesh* StaticMesh = (!StaticMeshComponent->IsUnreachable() && StaticMeshComponent->IsValidLowLevel()) ? StaticMeshComponent->GetStaticMesh() : nullptr;
		return StaticMesh && !StaticMesh->GetOutermost()->bIsCookedForEditor;
#else
		return true;
#endif
	}
	else if (Cast<UDynamicMeshComponent>(Component))
	{
		return true;
	}
	else if (Cast<UBrushComponent>(Component))
	{
		return true;
	}
	else if (UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(Component))
	{
#if WITH_EDITOR
		const UGeometryCollection* GeometryCollectionAsset = (!GeometryCollectionComponent->IsUnreachable() && GeometryCollectionComponent->IsValidLowLevel()) ? GeometryCollectionComponent->GetRestCollection() : nullptr;
		return GeometryCollectionAsset && !GeometryCollectionAsset->GetOutermost()->bIsCookedForEditor;
#else
		return true;
#endif
	}
	return false;
}

// Conversion helpers
namespace Private::ConversionHelper
{
	// Static mesh conversion functions (from geometry script MeshAssetFunctions.cpp)
	// TODO: these static mesh conversion helpers should be pulled out to their own StaticMeshToDynamicMesh converter method

	struct FStaticMeshConversionOptions
	{
		// Whether to apply Build Settings during the mesh copy.
		bool bApplyBuildSettings = true;

		// Whether to request tangents on the copied mesh. If tangents are not requested, tangent-related build settings will also be ignored.
		bool bRequestTangents = true;

		// Whether to ignore the 'remove degenerates' option from Build Settings. Note: Only applies if 'Apply Build Settings' is enabled.
		bool bIgnoreRemoveDegenerates = true;

		// Whether to scale the copied mesh by the Build Setting's 'Build Scale'. Note: This is considered separately from the 'Apply Build Settings' option.
		bool bUseBuildScale = true;

		// Whether to request the vertex colors of the component instancing the static mesh, rather than the static mesh asset
		bool bRequestInstanceVertexColors = true;
	};

	static bool CopyMeshFromStaticMesh_SourceData(
		UStaticMesh* FromStaticMeshAsset,
		FStaticMeshConversionOptions AssetOptions,
		EMeshLODType LODType,
		int32 LODIndex,
		FDynamicMesh3& OutMesh,
		FText& OutErrorMessage
	)
	{
		using namespace ::UE::Geometry;

		bool bSuccess = false;
		OutMesh.Clear();

		if (!FromStaticMeshAsset)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMeshSource_NullMesh", "Static Mesh is null");
			return false;
		}

		if (LODType != EMeshLODType::MaxAvailable && LODType != EMeshLODType::SourceModel && LODType != EMeshLODType::HiResSourceModel)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMesh_LODNotAvailable", "Requested LOD Type is not available");
			return false;
		}

#if WITH_EDITOR
		if (LODType == EMeshLODType::HiResSourceModel && FromStaticMeshAsset->IsHiResMeshDescriptionValid() == false)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMesh_HiResLODNotAvailable", "HiResSourceModel LOD Type is not available");
			return false;
		}

		const FMeshDescription* SourceMesh = nullptr;
		const FMeshBuildSettings* BuildSettings = nullptr;

		if ((LODType == EMeshLODType::HiResSourceModel) ||
			(LODType == EMeshLODType::MaxAvailable && FromStaticMeshAsset->IsHiResMeshDescriptionValid()))
		{
			SourceMesh = FromStaticMeshAsset->GetHiResMeshDescription();
			const FStaticMeshSourceModel& SourceModel = FromStaticMeshAsset->GetHiResSourceModel();
			BuildSettings = &SourceModel.BuildSettings;
		}
		else
		{
			int32 UseLODIndex = FMath::Clamp(LODIndex, 0, FromStaticMeshAsset->GetNumSourceModels() - 1);
			SourceMesh = FromStaticMeshAsset->GetMeshDescription(UseLODIndex);
			const FStaticMeshSourceModel& SourceModel = FromStaticMeshAsset->GetSourceModel(UseLODIndex);
			BuildSettings = &SourceModel.BuildSettings;
		}

		if (SourceMesh == nullptr)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMesh_SourceLODIsNull", "Requested SourceModel LOD is null, only RenderData Mesh is available");
			return false;
		}

		bool bHasDirtyBuildSettings = BuildSettings->bRecomputeNormals
			|| (BuildSettings->bRecomputeTangents && AssetOptions.bRequestTangents);
		bool bNeedsBuildScale = AssetOptions.bUseBuildScale && BuildSettings && !BuildSettings->BuildScale3D.Equals(FVector::OneVector);
		bool bNeedsOtherBuildSettings = AssetOptions.bApplyBuildSettings && bHasDirtyBuildSettings;

		FMeshDescription LocalSourceMeshCopy;
		if (bNeedsBuildScale || bNeedsOtherBuildSettings)
		{
			LocalSourceMeshCopy = *SourceMesh;

			FStaticMeshAttributes Attributes(LocalSourceMeshCopy);

			if (bNeedsBuildScale)
			{
				FTransform BuildScaleTransform = FTransform::Identity;
				BuildScaleTransform.SetScale3D(BuildSettings->BuildScale3D);
				FStaticMeshOperations::ApplyTransform(LocalSourceMeshCopy, BuildScaleTransform, true /*use correct normal transforms*/);
			}

			if (bNeedsOtherBuildSettings)
			{
				if (!Attributes.GetTriangleNormals().IsValid() || !Attributes.GetTriangleTangents().IsValid())
				{
					// If these attributes don't exist, create them and compute their values for each triangle
					FStaticMeshOperations::ComputeTriangleTangentsAndNormals(LocalSourceMeshCopy);
				}

				EComputeNTBsFlags ComputeNTBsOptions = EComputeNTBsFlags::BlendOverlappingNormals;
				ComputeNTBsOptions |= BuildSettings->bRecomputeNormals ? EComputeNTBsFlags::Normals : EComputeNTBsFlags::None;
				if (AssetOptions.bRequestTangents)
				{
					ComputeNTBsOptions |= BuildSettings->bRecomputeTangents ? EComputeNTBsFlags::Tangents : EComputeNTBsFlags::None;
					ComputeNTBsOptions |= BuildSettings->bUseMikkTSpace ? EComputeNTBsFlags::UseMikkTSpace : EComputeNTBsFlags::None;
				}
				ComputeNTBsOptions |= BuildSettings->bComputeWeightedNormals ? EComputeNTBsFlags::WeightedNTBs : EComputeNTBsFlags::None;
				if (AssetOptions.bIgnoreRemoveDegenerates == false)
				{
					ComputeNTBsOptions |= BuildSettings->bRemoveDegenerates ? EComputeNTBsFlags::IgnoreDegenerateTriangles : EComputeNTBsFlags::None;
				}

				FStaticMeshOperations::ComputeTangentsAndNormals(LocalSourceMeshCopy, ComputeNTBsOptions);
			}

			SourceMesh = &LocalSourceMeshCopy;
		}

		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(SourceMesh, OutMesh, AssetOptions.bRequestTangents);

		bSuccess = true;
#else
		OutErrorMessage = LOCTEXT("CopyMeshFromAsset_EditorOnly", "Source Models are not available at Runtime");
#endif

		return bSuccess;
	}



	static bool CopyMeshFromStaticMesh_RenderData(
		UStaticMesh* FromStaticMeshAsset,
		UStaticMeshComponent* StaticMeshComponent,
		FStaticMeshConversionOptions AssetOptions,
		EMeshLODType LODType,
		int32 LODIndex,
		FDynamicMesh3& OutMesh,
		FText& OutErrorMessage
	)
	{
		using namespace ::UE::Geometry;

		OutMesh.Clear();

		if (LODType != EMeshLODType::MaxAvailable && LODType != EMeshLODType::RenderData)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMeshRender_LODNotAvailable", "Requested LOD Type is not available");
			return false;
		}

#if !WITH_EDITOR
		if (FromStaticMeshAsset->bAllowCPUAccess == false)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMesh_CPUAccess", "StaticMesh bAllowCPUAccess must be set to true to read mesh data at Runtime");
			return false;
		}
#endif

		int32 UseLODIndex = FMath::Clamp(LODIndex, 0, FromStaticMeshAsset->GetNumLODs() - 1);

		const FStaticMeshLODResources* LODResources = nullptr;
		if (FStaticMeshRenderData* RenderData = FromStaticMeshAsset->GetRenderData())
		{
			LODResources = &RenderData->LODResources[UseLODIndex];
		}
		if (LODResources == nullptr)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMesh_NoLODResources", "LOD Data is not available");
			return false;
		}

		FStaticMeshLODResourcesToDynamicMesh::ConversionOptions ConvertOptions;
#if WITH_EDITOR
		if (AssetOptions.bUseBuildScale)
		{
			// respect BuildScale build setting
			const FMeshBuildSettings& LODBuildSettings = FromStaticMeshAsset->GetSourceModel(UseLODIndex).BuildSettings;
			ConvertOptions.BuildScale = (FVector3d)LODBuildSettings.BuildScale3D;
		}
#else
		if (!AssetOptions.bUseBuildScale)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMesh_BuildScaleAlreadyBaked", "Requested mesh without BuildScale, but BuildScale is already baked into the RenderData.");
			return false;
		}
#endif

		FStaticMeshLODResourcesToDynamicMesh Converter;
		if (AssetOptions.bRequestInstanceVertexColors && StaticMeshComponent && StaticMeshComponent->LODData.IsValidIndex(UseLODIndex))
		{
			FStaticMeshComponentLODInfo* InstanceMeshLODInfo = &StaticMeshComponent->LODData[UseLODIndex];
			const bool bValidInstanceData = InstanceMeshLODInfo
				&& InstanceMeshLODInfo->OverrideVertexColors
				&& InstanceMeshLODInfo->OverrideVertexColors->GetAllowCPUAccess()
				&& InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices() == LODResources->GetNumVertices();
			Converter.Convert(LODResources, ConvertOptions, OutMesh, bValidInstanceData,
				[InstanceMeshLODInfo](int32 LODVID)
				{
					return InstanceMeshLODInfo->OverrideVertexColors->VertexColor(LODVID);
				});
		}
		else
		{
			Converter.Convert(LODResources, ConvertOptions, OutMesh);
		}

		return true;
	}

	static bool CopyMeshFromStaticMesh(
		UStaticMesh* FromStaticMeshAsset,
		UStaticMeshComponent* StaticMeshComponent,
		FStaticMeshConversionOptions AssetOptions,
		EMeshLODType LODType,
		int32 LODIndex,
		bool bUseClosestLOD,
		FDynamicMesh3& OutMesh,
		FText& OutErrorMessage
	)
	{
		if (!FromStaticMeshAsset)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromStaticMeshRender_NullMesh", "Static Mesh is null");
			return false;
		}

		if (bUseClosestLOD)
		{
			// attempt to detect if an unavailable LOD was requested, and if so re-map to an available one
			if (LODType == EMeshLODType::MaxAvailable || LODType == EMeshLODType::HiResSourceModel)
			{
				LODIndex = 0;
			}
#if WITH_EDITOR
			if (LODType == EMeshLODType::MaxAvailable)
			{
				LODType = EMeshLODType::HiResSourceModel;
			}
			if (LODType == EMeshLODType::HiResSourceModel && !FromStaticMeshAsset->IsHiResMeshDescriptionValid())
			{
				LODType = EMeshLODType::SourceModel;
			}
			if (LODType == EMeshLODType::SourceModel)
			{
				LODIndex = FMath::Clamp(LODIndex, 0, FromStaticMeshAsset->GetNumSourceModels() - 1);
				if (!FromStaticMeshAsset->GetSourceModel(LODIndex).IsSourceModelInitialized())
				{
					LODType = EMeshLODType::RenderData;
				}
			}
			if (LODType == EMeshLODType::RenderData)
			{
				LODIndex = FMath::Clamp(LODIndex, 0, FromStaticMeshAsset->GetNumLODs() - 1);
			}
#else
			LODType = EMeshLODType::RenderData;
			LODIndex = FMath::Clamp(LODIndex, 0, FromStaticMeshAsset->GetNumLODs() - 1);
#endif
		}

		if (LODType == EMeshLODType::RenderData)
		{
			return CopyMeshFromStaticMesh_RenderData(FromStaticMeshAsset, StaticMeshComponent, AssetOptions, LODType, LODIndex, OutMesh, OutErrorMessage);
		}
		else
		{
			return CopyMeshFromStaticMesh_SourceData(FromStaticMeshAsset, AssetOptions, LODType, LODIndex, OutMesh, OutErrorMessage);
		}
	}
}


bool SceneComponentToDynamicMesh(USceneComponent* Component, const FToMeshOptions& Options, bool bTransformToWorld, 
	Geometry::FDynamicMesh3& OutMesh, FTransform& OutLocalToWorld, FText& OutErrorMessage,
	TArray<UMaterialInterface*>* OutComponentMaterials, TArray<UMaterialInterface*>* OutAssetMaterials)
{
	using namespace ::UE::Geometry;

	bool bSuccess = false;
	OutMesh.Clear();

	if (!Component)
	{
		OutErrorMessage = LOCTEXT("CopyMeshFromComponent_NullComponent", "Scene Component is null");
		return false;
	}
	OutLocalToWorld = Component->GetComponentTransform();

	auto GetPrimitiveComponentMaterials = [](UPrimitiveComponent* PrimComp, TArray<UMaterialInterface*>& Materials)
	{
		int32 NumMaterials = PrimComp->GetNumMaterials();
		Materials.SetNum(NumMaterials);
		for (int32 k = 0; k < NumMaterials; ++k)
		{
			Materials[k] = PrimComp->GetMaterial(k);
		}
	};

	// if Component Materials were requested, try to get them generically off the primitive component
	// Note: Currently all supported types happen to be primitive components as well; will need to update if this changes
	if (OutComponentMaterials)
	{
		OutComponentMaterials->Empty();
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
		{
			GetPrimitiveComponentMaterials(PrimComp, *OutComponentMaterials);
		}
	}


	if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
	{
		const int32 NumLODs = SkinnedMeshComponent->GetNumLODs();
		int32 RequestedLOD = Options.LODType == EMeshLODType::MaxAvailable ? 0 : Options.LODIndex;
		if (Options.bUseClosestLOD)
		{
			RequestedLOD = FMath::Clamp(RequestedLOD, 0, NumLODs - 1);
		}
		if (RequestedLOD < 0 || RequestedLOD > NumLODs - 1)
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromComponent_MissingSkinnedMeshComponentLOD", "SkinnedMeshComponent requested LOD does not exist");
		}
		else
		{
			USkinnedAsset* SkinnedAsset = SkinnedMeshComponent->GetSkinnedAsset();
			if (SkinnedAsset)
			{
				SkinnedMeshComponentToDynamicMesh(*SkinnedMeshComponent, OutMesh, RequestedLOD, Options.bWantTangents);
				OutMesh.DiscardTriangleGroups();

				if (OutAssetMaterials)
				{
					const TArray<FSkeletalMaterial>& Materials = SkinnedAsset->GetMaterials();
					OutAssetMaterials->SetNum(Materials.Num());
					for (int32 k = 0; k < Materials.Num(); ++k)
					{
						(*OutAssetMaterials)[k] = Materials[k].MaterialInterface;
					}
				}
				bSuccess = true;
			}
			else
			{
				OutErrorMessage = LOCTEXT("CopyMeshFromComponent_MissingSkinnedAsset", "SkinnedMeshComponent has a null SkinnedAsset");
			}
		}

	}
	else if (USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(Component))
	{
		UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
		if (StaticMesh)
		{
			Private::ConversionHelper::FStaticMeshConversionOptions AssetOptions;
			AssetOptions.bApplyBuildSettings = (Options.bWantNormals || Options.bWantTangents);
			AssetOptions.bRequestTangents = Options.bWantTangents;
			AssetOptions.bRequestInstanceVertexColors = Options.bWantInstanceColors;
			bSuccess = Private::ConversionHelper::CopyMeshFromStaticMesh(
				StaticMesh, SplineMeshComponent, AssetOptions, Options.LODType, Options.LODIndex, Options.bUseClosestLOD, OutMesh, OutErrorMessage);

			// deform the dynamic mesh and its tangent space with the spline
			if (bSuccess)
			{
				const bool bUpdateTangentSpace = Options.bWantTangents;
				SplineDeformDynamicMesh(*SplineMeshComponent, OutMesh, bUpdateTangentSpace);

				if (OutAssetMaterials)
				{
					int32 NumMaterials = StaticMesh->GetStaticMaterials().Num();
					OutAssetMaterials->SetNum(NumMaterials);
					for (int32 k = 0; k < NumMaterials; ++k)
					{
						(*OutAssetMaterials)[k] = StaticMesh->GetMaterial(k);
					}
				}
			}
		}
		else
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromSplineMeshComponent_MissingStaticMesh", "SplineMeshComponent has a null StaticMesh");
		}
	}
	else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh)
		{
			Private::ConversionHelper::FStaticMeshConversionOptions AssetOptions;
			AssetOptions.bApplyBuildSettings = (Options.bWantNormals || Options.bWantTangents);
			AssetOptions.bRequestTangents = Options.bWantTangents;
			AssetOptions.bRequestInstanceVertexColors = Options.bWantInstanceColors;
			bSuccess = Private::ConversionHelper::CopyMeshFromStaticMesh(
				StaticMesh, StaticMeshComponent, AssetOptions, Options.LODType, Options.LODIndex, Options.bUseClosestLOD, OutMesh, OutErrorMessage);

			// if we have an ISMC, append instances
			if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent))
			{
				FDynamicMesh3 InstancedMesh = MoveTemp(OutMesh);
				OutMesh.Clear();

				FDynamicMesh3 AccumMesh;
				AccumMesh.EnableMatchingAttributes(InstancedMesh);
				FDynamicMeshEditor Editor(&AccumMesh);
				FMeshIndexMappings Mappings;

				int32 NumInstances = ISMComponent->GetInstanceCount();
				for (int32 InstanceIdx = 0; InstanceIdx < NumInstances; ++InstanceIdx)
				{
					if (ISMComponent->IsValidInstance(InstanceIdx))
					{
						FTransform InstanceTransform;
						ISMComponent->GetInstanceTransform(InstanceIdx, InstanceTransform, /*bWorldSpace=*/false);
						FTransformSRT3d XForm(InstanceTransform);

						Mappings.Reset();
						Editor.AppendMesh(&InstancedMesh, Mappings,
							[&](int, const FVector3d& Position) { return XForm.TransformPosition(Position); },
							[&](int, const FVector3d& Normal) { return XForm.TransformNormal(Normal); });
					}
				}

				OutMesh = MoveTemp(AccumMesh);
			}

			if (OutAssetMaterials)
			{
				int32 NumMaterials = StaticMesh->GetStaticMaterials().Num();
				OutAssetMaterials->SetNum(NumMaterials);
				for (int32 k = 0; k < NumMaterials; ++k)
				{
					(*OutAssetMaterials)[k] = StaticMesh->GetMaterial(k);
				}
			}
		}
		else
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromComponent_MissingStaticMesh", "StaticMeshComponent has a null StaticMesh");
		}
	}
	else if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
	{
		UDynamicMesh* CopyDynamicMesh = DynamicMeshComponent->GetDynamicMesh();
		if (CopyDynamicMesh)
		{
			CopyDynamicMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
			{
				OutMesh = Mesh;
			});
			bSuccess = true;
		}
		else
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromComponent_MissingDynamicMesh", "DynamicMeshComponent has a null DynamicMesh");
		}
	}
	else if (UBrushComponent* BrushComponent = Cast<UBrushComponent>(Component))
	{
		FVolumeToMeshOptions VolOptions;
		VolOptions.bMergeVertices = true;
		VolOptions.bAutoRepairMesh = true;
		VolOptions.bOptimizeMesh = true;
		VolOptions.bSetGroups = true;

		OutMesh.EnableTriangleGroups();
		BrushComponentToDynamicMesh(BrushComponent, OutMesh, VolOptions);

		// compute normals for current polygroup topology
		OutMesh.EnableAttributes();
		if (Options.bWantNormals)
		{
			FDynamicMeshNormalOverlay* Normals = OutMesh.Attributes()->PrimaryNormals();
			FMeshNormals::InitializeOverlayTopologyFromFaceGroups(&OutMesh, Normals);
			FMeshNormals::QuickRecomputeOverlayNormals(OutMesh);
		}

		if (OutMesh.TriangleCount() > 0)
		{
			bSuccess = true;
		}
		else
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromComponent_InvalidBrushConversion", "BrushComponent conversion produced 0 triangles");
		}
	}
	else if (UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(Component))
	{
		if (const UGeometryCollection* RestCollection = GeometryCollectionComponent->GetRestCollection())
		{
			if (const FGeometryCollection* Collection = RestCollection->GetGeometryCollection().Get())
			{
				FTransform UnusedTransform;
				const TArray<FTransform3f>& DynamicTransforms = GeometryCollectionComponent->GetComponentSpaceTransforms3f();
				if (!DynamicTransforms.IsEmpty())
				{
					ConvertGeometryCollectionToDynamicMesh(OutMesh, UnusedTransform, false, *Collection, true, DynamicTransforms, false, Collection->TransformIndex.GetConstArray());
				}
				else
				{
					ConvertGeometryCollectionToDynamicMesh(OutMesh, UnusedTransform, false, *Collection, true, TArrayView<const FTransform3f>(Collection->Transform.GetConstArray()), true, Collection->TransformIndex.GetConstArray());
				}
				bSuccess = true;

				if (OutAssetMaterials)
				{
					//const TArray<TObjectPtr<UMaterialInterface>>& AssetMaterials = RestCollection->Materials;
					*OutAssetMaterials = RestCollection->Materials;
				}
			}
			else
			{
				OutErrorMessage = LOCTEXT("CopyMeshFromComponent_MissingCollectionData", "GeometryCollectionComponent has null Geometry Collection data");
			}
		}
		else
		{
			OutErrorMessage = LOCTEXT("CopyMeshFromComponent_MissingRestCollection", "GeometryCollectionComponent has null Rest Collection object");
		}
	}

	// transform mesh to world
	if (bSuccess && bTransformToWorld)
	{
		MeshTransforms::ApplyTransform(OutMesh, (FTransformSRT3d)OutLocalToWorld, true);
	}

	return bSuccess;
}

} // end namespace Conversion
} // end namespace UE


#undef LOCTEXT_NAMESPACE

