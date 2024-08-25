// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/StaticMeshImportNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Engine/StaticMesh.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "SkeletalMeshAttributes.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StaticMeshImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetStaticMeshImportNode"

namespace UE::Chaos::ClothAsset::Private
{
	static bool InitializeDataFromMeshDescription(
		const UStaticMesh* const StaticMesh,
		const FMeshDescription* const InMeshDescription,
		const FMeshBuildSettings& InBuildSettings,
		const TArray<FStaticMaterial>& StaticMaterials,
		const TSharedRef<FManagedArrayCollection>& ClothCollection)
	{
		FSkeletalMeshLODModel SkeletalMeshModel;
		if (FClothDataflowTools::BuildSkeletalMeshModelFromMeshDescription(InMeshDescription, InBuildSettings, SkeletalMeshModel))
		{
			FStaticMeshConstAttributes MeshAttributes(*InMeshDescription);
			TPolygonGroupAttributesConstRef<FName> MaterialSlotNames = MeshAttributes.GetPolygonGroupMaterialSlotNames();
			for (int32 SectionIndex = 0; SectionIndex < SkeletalMeshModel.Sections.Num(); ++SectionIndex)
			{
				// Section MaterialIndex refers to the polygon group index. Look up which material this corresponds with.
				const FName& MaterialSlotName = MaterialSlotNames[SkeletalMeshModel.Sections[SectionIndex].MaterialIndex];
				const int32 MaterialIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName(MaterialSlotName);
				const FString RenderMaterialPathName = StaticMaterials.IsValidIndex(MaterialIndex)&& StaticMaterials[MaterialIndex].MaterialInterface ? StaticMaterials[MaterialIndex].MaterialInterface->GetPathName() : "";
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
					// Add render data (section = pattern)
					if (!InitializeDataFromMeshDescription(StaticMesh, MeshDescription, StaticMesh->GetSourceModel(LODIndex).BuildSettings, StaticMesh->GetStaticMaterials(), ClothCollection))
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
