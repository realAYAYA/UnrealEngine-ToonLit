// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/TerminalNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/ClothGeometryTools.h"
#include "ChaosClothAsset/ClothLODTransitionDataCache.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "Animation/Skeleton.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/Material.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TerminalNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetTerminalNode"

namespace UE::Chaos::ClothAsset::Private
{
	uint32 CalculateClothChecksum(const TArray<TSharedRef<FManagedArrayCollection>>& InClothCollections)
	{
		uint32 Checksum = 0;
		for (const TSharedRef<FManagedArrayCollection>& ClothCollection : InClothCollections)
		{
			constexpr bool bIncludeWeightMapsTrue = true; // Currently, editing weight maps is destructive
			FCollectionClothConstFacade Cloth(ClothCollection);
			if (Cloth.HasValidData())
			{
				Checksum = Cloth.CalculateTypeHash(bIncludeWeightMapsTrue, Checksum);

				const TArray<FName> GroupNames = ClothCollection->GroupNames();
				for (const FName& GroupName : GroupNames)
				{
					Checksum = Cloth.CalculateUserDefinedAttributesTypeHash<int32>(GroupName, Checksum);
					Checksum = Cloth.CalculateUserDefinedAttributesTypeHash<float>(GroupName, Checksum);
					Checksum = Cloth.CalculateUserDefinedAttributesTypeHash<FVector3f>(GroupName, Checksum);
				}
			}
			FCollectionClothSelectionConstFacade Selection(ClothCollection);
			if (Selection.IsValid())
			{
				// Just checksum the sets that are SimVertex3D and SimFace sets since those are the only ones we care about right now
				const TArray<FName> SelectionNames = Selection.GetNames();
				for (const FName& SelectionName : SelectionNames)
				{
					const FName SelectionGroup = Selection.GetSelectionGroup(SelectionName);
					if (SelectionGroup == ClothCollectionGroup::SimVertices3D ||
						SelectionGroup == ClothCollectionGroup::SimFaces)
					{
						const TArray<int32> SelectionAsArray = Selection.GetSelectionSet(SelectionName).Array();
						Checksum = HashCombineFast(Checksum, GetTypeHash(SelectionName));
						Checksum = GetArrayHash(SelectionAsArray.GetData(), SelectionAsArray.Num(), Checksum);
					}
				}
			}
		}
		return Checksum;
	}

	bool PropertyKeysAndSolverTypesMatch(const TArray<TSharedRef<FManagedArrayCollection>>& Collections0, const TArray<TSharedRef<FManagedArrayCollection>>& Collections1)
	{
		if (Collections0.Num() != Collections1.Num())
		{
			return false;
		}
		for (int32 LODIndex = 0; LODIndex < Collections0.Num(); ++LODIndex)
		{
			::Chaos::Softs::FCollectionPropertyConstFacade Property0(Collections0[LODIndex].ToSharedPtr());
			::Chaos::Softs::FCollectionPropertyConstFacade Property1(Collections1[LODIndex].ToSharedPtr());
			if (Property0.Num() != Property1.Num())
			{
				return false;
			}
			for (int32 PropertyIndex = 0; PropertyIndex < Property0.Num(); ++PropertyIndex)
			{
				if (Property0.GetKey(PropertyIndex) != Property1.GetKey(PropertyIndex))
				{
					return false;
				}
			}
		}
		return true;
	}
}

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

		if (RefreshAsset.bRefreshAsset)
		{
			bClothCollectionChecksumValid = false;
			RefreshAsset.bRefreshAsset = false;
		}

		TArray<TSharedRef<FManagedArrayCollection>> InClothCollections = GetCleanedCollectionLodValues(Context);
		TArray<TSharedRef<FManagedArrayCollection>>& ClothCollections = ClothAsset->GetClothCollections();

		const uint32 PreviousChecksum = ClothColllectionChecksum;
		const bool bPreviousChecksumsValid = bClothCollectionChecksumValid;
		ClothColllectionChecksum = Private::CalculateClothChecksum(InClothCollections);
		bClothCollectionChecksumValid = InClothCollections.Num() > 0;

		if (bPreviousChecksumsValid && PreviousChecksum == ClothColllectionChecksum && Private::PropertyKeysAndSolverTypesMatch(InClothCollections, ClothCollections))
		{
			// Cloth and property keys match. Just update property values.
			check(InClothCollections.Num() == ClothCollections.Num());
			check(ClothCollections.Num() > 0);
			for (int32 LODIndex = 0; LODIndex < InClothCollections.Num(); ++LODIndex)
			{
				Chaos::Softs::FCollectionPropertyFacade Properties(ClothCollections[LODIndex]);
				Properties.UpdateProperties(InClothCollections[LODIndex].ToSharedPtr());
			}

			// Asset must be resaved
			ClothAsset->MarkPackageDirty();

			return;
		}

		// Reset the asset's collection
		ClothCollections.Reset(InClothCollections.Num());

		// Reset the asset's material list
		ClothAsset->GetMaterials().Reset();

		// Iterate through the LODs
		FString PhysicsAssetPathName;

		for (int32 LodIndex = 0; LodIndex < InClothCollections.Num(); ++LodIndex)
		{
			// New LOD
			TSharedRef<FManagedArrayCollection>& ClothCollection = ClothCollections.Emplace_GetRef(MakeShared<FManagedArrayCollection>());
			UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(ClothCollection);
			ClothFacade.DefineSchema();

			const FCollectionClothConstFacade InClothFacade(InClothCollections[LodIndex]);
			if (!InClothFacade.HasValidData())
			{
				FClothDataflowTools::LogAndToastWarning(*this, LOCTEXT("InvalidLODHeadline", "Invalid LOD."),
					FText::Format(
						LOCTEXT("InvalidLODDetails", "LOD {0} has no valid data."),
						LodIndex));

				continue;
			}

			// Copy input LOD to current output LOD
			ClothFacade.Initialize(InClothFacade);

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
			Chaos::Softs::FCollectionPropertyMutableFacade(ClothCollection).Append(InClothCollections[LodIndex].ToSharedPtr(), bUpdateExistingProperties);

			// Set selections
			FCollectionClothSelectionFacade Selection(ClothCollection);
			FCollectionClothSelectionConstFacade InSelection(InClothCollections[LodIndex]);
			if (InSelection.IsValid())
			{
				Selection.DefineSchema();
				const TArray<FName> InSelectionNames = InSelection.GetNames();
				for (const FName& InSelectionName : InSelectionNames)
				{
					const FName SelectionGroup = InSelection.GetSelectionGroup(InSelectionName);
					if (SelectionGroup == ClothCollectionGroup::SimVertices3D || 
						SelectionGroup == ClothCollectionGroup::SimFaces)
					{
						Selection.FindOrAddSelectionSet(InSelectionName, SelectionGroup) = InSelection.GetSelectionSet(InSelectionName);
					}
				}
			}


			// Set physics asset only with LOD 0 at the moment
			if (LodIndex == 0)
			{
				using namespace ::Chaos::Softs;
				PhysicsAssetPathName = InClothFacade.GetPhysicsAssetPathName();
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

		// Asset must be resaved
		ClothAsset->MarkPackageDirty();
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

Dataflow::FPin FChaosClothAssetTerminalNode::GetPinToRemove() const
{
	auto PinToRemove = [this](const FManagedArrayCollection* Collection) -> Dataflow::FPin
		{
			const FDataflowInput* const Input = FindInput(Collection);
			check(Input);
			return { Dataflow::FPin::EDirection::INPUT, Input->GetType(), Input->GetName() };
		};

	switch (NumLods - 1)
	{
	case 1: return PinToRemove(&CollectionLod1);
	case 2: return PinToRemove(&CollectionLod2);
	case 3: return PinToRemove(&CollectionLod3);
	case 4: return PinToRemove(&CollectionLod4);
	case 5: return PinToRemove(&CollectionLod5);
	default: break;
	}
	return Super::GetPinToRemove();
}

void FChaosClothAssetTerminalNode::OnPinRemoved(const Dataflow::FPin& Pin)
{
	auto CheckPinRemoved = [this, &Pin](const FManagedArrayCollection* Collection)
	{
		check(Pin.Direction == Dataflow::FPin::EDirection::INPUT);
#if DO_CHECK
		const FDataflowInput* const Input = FindInput(Collection);
		check(Input);
		check(Input->GetName() == Pin.Name);
		check(Input->GetType() == Pin.Type);
#endif
	};

	switch (NumLods - 1)
	{
	case 1:
		CheckPinRemoved(&CollectionLod1);
		--NumLods; 
		break;
	case 2:
		CheckPinRemoved(&CollectionLod2);
		--NumLods;
		break;
	case 3:
		CheckPinRemoved(&CollectionLod3);
		--NumLods;
		break;
	case 4:
		CheckPinRemoved(&CollectionLod4);
		--NumLods;
		break;
	case 5:
		CheckPinRemoved(&CollectionLod5);
		--NumLods;
		break;
	default: 
		checkNoEntry();
		break;
	}

	return Super::OnPinRemoved(Pin);
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

TArray<TSharedRef<FManagedArrayCollection>> FChaosClothAssetTerminalNode::GetCleanedCollectionLodValues(Dataflow::FContext& Context) const
{
	using namespace UE::Chaos::ClothAsset;

	const TArray<const FManagedArrayCollection*> CollectionLods = GetCollectionLods();
	TArray<TSharedRef<FManagedArrayCollection>> CollectionLodValues;
	CollectionLodValues.Reserve(CollectionLods.Num());

	int32 LastValidLodIndex = INDEX_NONE;	
	for (int32 LodIndex = 0; LodIndex < CollectionLods.Num(); ++LodIndex)
	{
		TSharedRef<FManagedArrayCollection>& CollectionLodValue = CollectionLodValues.Emplace_GetRef(MakeShared<FManagedArrayCollection>(GetValue<FManagedArrayCollection>(Context, CollectionLods[LodIndex])));

		FCollectionClothFacade ClothFacade(CollectionLodValue);
		if (ClothFacade.HasValidData())
		{
			FClothGeometryTools::CleanupAndCompactMesh(CollectionLodValue);
			LastValidLodIndex = LodIndex;
		}
		else
		{
			if (LastValidLodIndex >= 0)
			{
				ClothFacade.DefineSchema();
				ClothFacade.Initialize(FCollectionClothConstFacade(CollectionLodValues[LastValidLodIndex]));
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
				break;
			}
		}
	}
	return CollectionLodValues;
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
