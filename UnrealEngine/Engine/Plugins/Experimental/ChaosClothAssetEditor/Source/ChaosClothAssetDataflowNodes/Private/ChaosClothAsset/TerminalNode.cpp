// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/TerminalNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothLODTransitionDataCache.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Animation/Skeleton.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/Material.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TerminalNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetTerminalNode"

FChaosClothAssetTerminalNode::FChaosClothAssetTerminalNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowTerminalNode(InParam, InGuid)
{
	RegisterInputConnection(&CollectionLod0);
}

void FChaosClothAssetTerminalNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	if (UChaosClothAsset* ClothAsset = Cast<UChaosClothAsset>(Asset.Get()))
	{
		using namespace UE::Chaos::ClothAsset;
		const TArray<const FManagedArrayCollection*> CollectionLods = GetCollectionLods();

		// Reset the asset's collection
		TArray<TSharedRef<FManagedArrayCollection>>& ClothCollections = ClothAsset->GetClothCollections();
		ClothCollections.Reset(CollectionLods.Num());

		// Reset the asset's material list
		ClothAsset->GetMaterials().Reset();

		// Iterate through the LODs
		FString PhysicsAssetPathName;

		int32 LastValidLodIndex = INDEX_NONE;
		for (int32 LodIndex = 0; LodIndex < CollectionLods.Num(); ++LodIndex)
		{
			// New LOD
			TSharedRef<FManagedArrayCollection>& ClothCollection = ClothCollections.Emplace_GetRef(MakeShared<FManagedArrayCollection>());
			UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(ClothCollection);
			ClothFacade.DefineSchema();

			// Retrieve input LOD
			const FManagedArrayCollection& InCollectionLod = GetValue<FManagedArrayCollection>(Context, CollectionLods[LodIndex]);
			const TSharedRef<const FManagedArrayCollection> InClothCollection = MakeShared<const FManagedArrayCollection>(InCollectionLod);

			const FCollectionClothConstFacade InClothFacade(InClothCollection);
				// Check LOD validity
			if (InClothFacade.GetNumSimPatterns() &&
				InClothFacade.GetNumRenderPatterns() &&
				InClothFacade.GetNumSimVertices2D() &&
				InClothFacade.GetNumSimVertices3D() &&
				InClothFacade.GetNumSimFaces() &&
				InClothFacade.GetNumRenderVertices() &&
				InClothFacade.GetNumRenderFaces())
			{
				LastValidLodIndex = LodIndex;

				// Copy input LOD to current output LOD
				ClothFacade.Initialize(InClothFacade);
				FClothGeometryTools::CleanupAndCompactMesh(ClothCollection);

				// Add this LOD's materials to the asset
				const int32 NumLodMaterials = ClothFacade.GetNumRenderPatterns();

				TArray<FSkeletalMaterial>& Materials = ClothAsset->GetMaterials();
				Materials.Reserve(Materials.Num() + NumLodMaterials);

				const TConstArrayView<FString> LodRenderMaterialPathName = ClothFacade.GetRenderMaterialPathName();
				for (int32 LodMaterialIndex = 0; LodMaterialIndex < NumLodMaterials; ++LodMaterialIndex)
				{
					const FString& RenderMaterialPathName = LodRenderMaterialPathName[LodMaterialIndex];

					if (UMaterialInterface* const Material = LoadObject<UMaterialInterface>(ClothAsset, *RenderMaterialPathName, nullptr, LOAD_None, nullptr))
					{
						Materials.Emplace(Material, true, false, Material->GetFName());
					}
					else
					{
						Materials.Emplace();
					}
				}

				// Set properties
				constexpr bool bUpdateExistingProperties = false;
				Chaos::Softs::FCollectionPropertyMutableFacade(ClothCollection).Append(InClothCollection.ToSharedPtr(), bUpdateExistingProperties);

				// Set physics asset only with LOD 0 at the moment
				if (LodIndex == 0)
				{
					using namespace ::Chaos::Softs;
					PhysicsAssetPathName = InClothFacade.GetPhysicsAssetPathName();
				}
			}
			if (LodIndex != LastValidLodIndex)
			{
				if (LastValidLodIndex >= 0)
				{
					ClothFacade.Initialize(FCollectionClothConstFacade(ClothCollections[LastValidLodIndex]));

					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidInputLodNHeadline", "Invalid input LOD."),
						FText::Format(
							LOCTEXT("InvalidInputLodNDetails",
								"Invalid or empty input LOD for LOD {0}.\n"
								"Using the previous valid LOD {1} instead."),
							LodIndex,
							LastValidLodIndex));
				}
				else
				{
					FClothDataflowTools::LogAndToastWarning(*this,
						LOCTEXT("InvalidInputLod0Headline", "Invalid input LOD 0."),
						LOCTEXT("InvalidInputLod0Details",
							"Invalid or empty input LOD for LOD 0.\n"
							"LOD 0 cannot be empty in order to construct a valid Cloth Asset."));
					break; // Empty cloth asset
				}
			}
		}

		// Make sure that whatever happens there is always at least one empty LOD to avoid crashing the render data
		if (ClothCollections.Num() < 1)
		{
			TSharedRef<FManagedArrayCollection>& ClothCollection = ClothCollections.Emplace_GetRef(MakeShared<FManagedArrayCollection>());
			FCollectionClothFacade ClothFacade(ClothCollection);
			ClothFacade.DefineSchema();
		}

		// Set reference skeleton
		constexpr bool bRebuildModels = false;  // Avoid rebuilding the asset twice
		ClothAsset->UpdateSkeletonFromCollection(bRebuildModels);

		// Set physics asset (note: the cloth asset's physics asset is only replaced if a collection path name is found valid)
		UPhysicsAsset* const PhysicsAsset = !PhysicsAssetPathName.IsEmpty() ?
			LoadObject<UPhysicsAsset>(ClothAsset, *PhysicsAssetPathName, nullptr, LOAD_None, nullptr) :
			nullptr;
		ClothAsset->SetPhysicsAsset(PhysicsAsset);

		// Rebuild the asset static data
		ClothAsset->Build(&LODTransitionDataCache);
	}
}

Dataflow::FPin FChaosClothAssetTerminalNode::AddPin()
{
	auto AddInput = [this](const FManagedArrayCollection* Collection) -> Dataflow::FPin
		{
			RegisterInputConnection(Collection);
			const FDataflowInput* const Input = FindInput(Collection);
			return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
		};

	switch (NumLods)
	{
	case 1: ++NumLods; return AddInput(&CollectionLod1);
	case 2: ++NumLods; return AddInput(&CollectionLod2);
	case 3: ++NumLods; return AddInput(&CollectionLod3);
	case 4: ++NumLods; return AddInput(&CollectionLod4);
	case 5: ++NumLods; return AddInput(&CollectionLod5);
	default: break;
	}

	return Super::AddPin();
}

Dataflow::FPin FChaosClothAssetTerminalNode::RemovePin()
{
	auto RemoveInput = [this](const FManagedArrayCollection* Collection) -> Dataflow::FPin
		{
			const FDataflowInput* const Input = FindInput(Collection);
			check(Input);
			Dataflow::FPin Pin = { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
			UnregisterInputConnection(Collection);  // This will delete the input, so set the pin before that
			return Pin;
		};

	switch (NumLods - 1)
	{
	case 1: --NumLods; return RemoveInput(&CollectionLod1);
	case 2: --NumLods; return RemoveInput(&CollectionLod2);
	case 3: --NumLods; return RemoveInput(&CollectionLod3);
	case 4: --NumLods; return RemoveInput(&CollectionLod4);
	case 5: --NumLods; return RemoveInput(&CollectionLod5);
	default: break;
	}
	return Super::AddPin();
}

TArray<const FManagedArrayCollection*> FChaosClothAssetTerminalNode::GetCollectionLods() const
{
	TArray<const FManagedArrayCollection*> CollectionLods;
	CollectionLods.SetNumUninitialized(NumLods);

	for (int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
	{
		switch (LodIndex)
		{
		case 0: CollectionLods[LodIndex] = &CollectionLod0; break;
		case 1: CollectionLods[LodIndex] = &CollectionLod1; break;
		case 2: CollectionLods[LodIndex] = &CollectionLod2; break;
		case 3: CollectionLods[LodIndex] = &CollectionLod3; break;
		case 4: CollectionLods[LodIndex] = &CollectionLod4; break;
		case 5: CollectionLods[LodIndex] = &CollectionLod5; break;
		default: check(false); break;
		}
	}
	return CollectionLods;
}

void FChaosClothAssetTerminalNode::Serialize(FArchive& Ar)
{
	// because we add pins we need to make sure we restore them when loading
	// to make sure they can get properly reconnected

	if (Ar.IsLoading())
	{
		const int32 NumLodToAdd = (NumLods - 1);
		NumLods = 1; // need to reset back to default because add pin will increment it again 
		for (int32 LodIndex = 0; LodIndex < NumLodToAdd; ++LodIndex)
		{
			AddPin();
		}
		ensure(NumLodToAdd == (NumLods - 1));
	}
}

#undef LOCTEXT_NAMESPACE
