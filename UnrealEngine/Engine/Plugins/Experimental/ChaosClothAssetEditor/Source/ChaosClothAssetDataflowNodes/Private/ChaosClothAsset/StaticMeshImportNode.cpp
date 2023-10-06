// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/StaticMeshImportNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "SkeletalMeshAttributes.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Materials/MaterialInterface.h"
#include "MeshUtilities.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StaticMeshImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetStaticMeshImportNode"

namespace UE::Chaos::ClothAsset::Private
{

void CopyBuildSettings(
	const FMeshBuildSettings& InStaticMeshBuildSettings,
	FSkeletalMeshBuildSettings& OutSkeletalMeshBuildSettings
)
{
	OutSkeletalMeshBuildSettings.bRecomputeNormals = InStaticMeshBuildSettings.bRecomputeNormals;
	OutSkeletalMeshBuildSettings.bRecomputeTangents = InStaticMeshBuildSettings.bRecomputeTangents;
	OutSkeletalMeshBuildSettings.bUseMikkTSpace = InStaticMeshBuildSettings.bUseMikkTSpace;
	OutSkeletalMeshBuildSettings.bComputeWeightedNormals = InStaticMeshBuildSettings.bComputeWeightedNormals;
	OutSkeletalMeshBuildSettings.bRemoveDegenerates = InStaticMeshBuildSettings.bRemoveDegenerates;
	OutSkeletalMeshBuildSettings.bUseHighPrecisionTangentBasis = InStaticMeshBuildSettings.bUseHighPrecisionTangentBasis;
	OutSkeletalMeshBuildSettings.bUseFullPrecisionUVs = InStaticMeshBuildSettings.bUseFullPrecisionUVs;
	OutSkeletalMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs = InStaticMeshBuildSettings.bUseBackwardsCompatibleF16TruncUVs;
	// The rest we leave at defaults.
}

bool BuildSkeletalMeshModelFromMeshDescription(const FMeshDescription* const InMeshDescription, const FMeshBuildSettings& InBuildSettings, FSkeletalMeshLODModel& SkeletalMeshModel)
{
	// This is following StaticToSkeletalMeshConverter.cpp::AddLODFromStaticMeshSourceModel
	FSkeletalMeshBuildSettings BuildSettings;
	CopyBuildSettings(InBuildSettings, BuildSettings);
	FMeshDescription SkeletalMeshGeometry = *InMeshDescription;
	FSkeletalMeshAttributes SkeletalMeshAttributes(SkeletalMeshGeometry);
	SkeletalMeshAttributes.Register();

	// Full binding to the root bone.
	constexpr int32 RootBoneIndex = 0;
	FSkinWeightsVertexAttributesRef SkinWeights = SkeletalMeshAttributes.GetVertexSkinWeights();
	UE::AnimationCore::FBoneWeight RootInfluence(RootBoneIndex, 1.0f);
	UE::AnimationCore::FBoneWeights RootBinding = UE::AnimationCore::FBoneWeights::Create({ RootInfluence });

	for (const FVertexID VertexID : SkeletalMeshGeometry.Vertices().GetElementIDs())
	{
		SkinWeights.Set(VertexID, RootBinding);
	}

	FSkeletalMeshImportData SkeletalMeshImportGeometry = FSkeletalMeshImportData::CreateFromMeshDescription(SkeletalMeshGeometry);
	// Data needed by BuildSkeletalMesh
	TArray<FVector3f> LODPoints;
	TArray<SkeletalMeshImportData::FMeshWedge> LODWedges;
	TArray<SkeletalMeshImportData::FMeshFace> LODFaces;
	TArray<SkeletalMeshImportData::FVertInfluence> LODInfluences;
	TArray<int32> LODPointToRawMap;
	SkeletalMeshImportGeometry.CopyLODImportData(LODPoints, LODWedges, LODFaces, LODInfluences, LODPointToRawMap);
	IMeshUtilities::MeshBuildOptions BuildOptions;
	BuildOptions.TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	BuildOptions.FillOptions(BuildSettings);

	static const FString SkeletalMeshName("ClothAssetStaticMeshImportConvert"); // This is only used by warning messages in the mesh builder.
	// Build a RefSkeleton with just a root bone. The BuildSkeletalMesh code expects you have a reference skeleton with at least one bone to work.
	FReferenceSkeleton RootBoneRefSkeleton;
	FReferenceSkeletonModifier SkeletonModifier(RootBoneRefSkeleton, nullptr);
	FMeshBoneInfo RootBoneInfo;
	RootBoneInfo.Name = FName("Root");
	SkeletonModifier.Add(RootBoneInfo, FTransform());
	RootBoneRefSkeleton.RebuildRefSkeleton(nullptr, true);

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	TArray<FText> WarningMessages;
	if (!MeshUtilities.BuildSkeletalMesh(SkeletalMeshModel, SkeletalMeshName, RootBoneRefSkeleton, LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap, BuildOptions, &WarningMessages))
	{
		for (const FText& Message : WarningMessages)
		{
			UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("%s"), *Message.ToString());
		}
		return false;
	}
	return true;
}

bool InitializeDataFromMeshDescription(
	const FMeshDescription* const InMeshDescription,
	const FMeshBuildSettings& InBuildSettings,
	const TArray<FStaticMaterial>& StaticMaterials,
	const TSharedRef<FManagedArrayCollection>& ClothCollection)
{
	FSkeletalMeshLODModel SkeletalMeshModel;
	if (BuildSkeletalMeshModelFromMeshDescription(InMeshDescription, InBuildSettings, SkeletalMeshModel))
	{
		check(SkeletalMeshModel.Sections.Num() == StaticMaterials.Num());
		for (int32 SectionIndex = 0; SectionIndex < SkeletalMeshModel.Sections.Num(); ++SectionIndex)
		{
			const FString RenderMaterialPathName = StaticMaterials[SectionIndex].MaterialInterface ? StaticMaterials[SectionIndex].MaterialInterface->GetPathName() : "";
			FClothDataflowTools::AddRenderPatternFromSkeletalMeshSection(ClothCollection, SkeletalMeshModel, SectionIndex, RenderMaterialPathName);
		}
		return true;
	}
	return false;
}

} // namespace UE::Chaos::ClothAsset::Private

FChaosClothAssetStaticMeshImportNode::FChaosClothAssetStaticMeshImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Collection);
}

void FChaosClothAssetStaticMeshImportNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;
	using namespace UE::Chaos::ClothAsset::Private;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		// Evaluate out collection
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
		FCollectionClothFacade ClothFacade(ClothCollection);
		ClothFacade.DefineSchema();

		if (StaticMesh && (bImportSimMesh || bImportRenderMesh))
		{
			const int32 NumLods = StaticMesh->GetNumSourceModels();
			if (LODIndex < NumLods)
			{
				const FMeshDescription* const MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
				check(MeshDescription);

				if (bImportSimMesh)
				{
					FMeshDescriptionToDynamicMesh Converter;
					Converter.bPrintDebugMessages = false;
					Converter.bEnableOutputGroups = false;
					Converter.bVIDsFromNonManifoldMeshDescriptionAttr = true;
					UE::Geometry::FDynamicMesh3 DynamicMesh;

					Converter.Convert(MeshDescription, DynamicMesh);
					constexpr bool bAppend = false;
					FClothGeometryTools::BuildSimMeshFromDynamicMesh(ClothCollection, DynamicMesh, UVChannel, UVScale, bAppend);
				}
				if (bImportRenderMesh)
				{
					// Add render data into a single pattern for now
					if (!InitializeDataFromMeshDescription(MeshDescription, StaticMesh->GetSourceModel(LODIndex).BuildSettings, StaticMesh->GetStaticMaterials(), ClothCollection))
					{
						FClothDataflowTools::LogAndToastWarning(*this,
							LOCTEXT("InvalidRenderMeshHeadline", "Invalid render mesh."),
							FText::Format(
								LOCTEXT("InvalidRenderMeshDetails", "The input static mesh {0} failed to convert to a valid render mesh."),
								FText::FromString(StaticMesh->GetName())));
					}
				}
				
				// Bind to root bone by default
				FClothGeometryTools::BindMeshToRootBone(ClothCollection, bImportSimMesh, bImportRenderMesh);
			}
			else
			{
				FClothDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("InvalidLODIndexHeadline", "Invalid LOD index."),
					FText::Format(LOCTEXT("InvalidLODIndexDetails",
						"{0} is not a valid LOD index for static mesh {1}.\n"
						"This static mesh has {2} LOD(s)."),
						FText::FromString(StaticMesh->GetName()),
						LODIndex,
						NumLods));
			}
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

#undef LOCTEXT_NAMESPACE
