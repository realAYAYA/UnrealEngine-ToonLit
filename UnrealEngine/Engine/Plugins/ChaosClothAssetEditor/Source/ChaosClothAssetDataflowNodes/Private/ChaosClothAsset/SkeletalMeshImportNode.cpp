// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SkeletalMeshImportNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Animation/Skeleton.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkeletalMeshImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetSkeletalMeshImportNode"

FChaosClothAssetSkeletalMeshImportNode::FChaosClothAssetSkeletalMeshImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Collection);
}

void FChaosClothAssetSkeletalMeshImportNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
		FCollectionClothFacade ClothFacade(ClothCollection);
		ClothFacade.DefineSchema();

		if (SkeletalMesh)
		{
			const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
			const bool bIsValidLOD = ImportedModel && ImportedModel->LODModels.IsValidIndex(LODIndex);
			if (!bIsValidLOD)
			{
				FClothDataflowTools::LogAndToastWarning(*this, LOCTEXT("InvalidLODHeadline", "Invalid LOD."),
					FText::Format(
						LOCTEXT("InvalidLODDetails", "No valid LOD {0} found for skeletal mesh {1}."),
						LODIndex,
						FText::FromString(SkeletalMesh->GetName())));

				SetValue(Context, MoveTemp(*ClothCollection), &Collection);
				return;
			}

			const FSkeletalMeshLODModel &LODModel = ImportedModel->LODModels[LODIndex];
			const bool bIsValidSection = LODModel.Sections.IsValidIndex(SectionIndex);;
			if (!bIsValidSection)
			{
				FClothDataflowTools::LogAndToastWarning(*this, LOCTEXT("InvalidSectionHeadline", "Invalid section."),
					FText::Format(
						LOCTEXT("InvalidSectionDetails", "No valid section {0} found for skeletal mesh {1}."),
						SectionIndex,
						FText::FromString(SkeletalMesh->GetName())));

				SetValue(Context, MoveTemp(*ClothCollection), &Collection);
				return;
			}

			const FSkelMeshSection &Section = LODModel.Sections[SectionIndex];

			if (bImportSimMesh)
			{
				FClothDataflowTools::AddSimPatternsFromSkeletalMeshSection(ClothCollection, LODModel, SectionIndex, UVChannel, UVScale);
			}

			if (bImportRenderMesh)
			{
				const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
				check(SectionIndex < Materials.Num());
				const FString RenderMaterialPathName = Materials[SectionIndex].MaterialInterface ? Materials[SectionIndex].MaterialInterface->GetPathName() : "";
				FClothDataflowTools::AddRenderPatternFromSkeletalMeshSection(ClothCollection, LODModel, SectionIndex, RenderMaterialPathName);
			}

			if (const UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset())
			{
				ClothFacade.SetPhysicsAssetPathName(PhysicsAsset->GetPathName());
			}

			ClothFacade.SetSkeletalMeshPathName(SkeletalMesh->GetPathName());
		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

#undef LOCTEXT_NAMESPACE
