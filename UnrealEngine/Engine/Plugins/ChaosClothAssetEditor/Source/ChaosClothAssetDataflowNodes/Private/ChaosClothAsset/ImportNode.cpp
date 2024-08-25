// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ImportNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/Material.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetImportNode"

FChaosClothAssetImportNode::FChaosClothAssetImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Collection);
}

void FChaosClothAssetImportNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;
	using namespace Chaos::Softs;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		if (ReimportAsset.bRefreshAsset)
		{
			ReimportAsset.bRefreshAsset = false;
		}

		bool bSetValue = false;
		if (ClothAsset)
		{
			// Copy the main cloth asset details to this dataflow's owner if any
			if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
			{
				if (const UChaosClothAsset* const OwnerClothAsset = Cast<UChaosClothAsset>(EngineContext->Owner))
				{
					if (OwnerClothAsset == ClothAsset)
					{
						// Can't create a loop
						bSetValue = false;
						FClothDataflowTools::LogAndToastWarning(*this,
							LOCTEXT("RecursiveAssetLoopHeadline", "Recursive asset loop."),
							LOCTEXT("RecursiveAssetLoopDetails", "The source asset cannot be the same as the terminal asset."));
					}
					else
					{
						bSetValue = true;
					}
				}
			}
			else
			{
				// No terminal asset, this is a stray dataflow and it is safe to set the value without fear of a loop
				bSetValue = true;
			}
		}

		// Create a new cloth collection with its LOD 0
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();
		FCollectionClothFacade ClothFacade(ClothCollection);
		ClothFacade.DefineSchema();

		// Copy the cloth asset to this node's output collection
		if (bSetValue)
		{
			const TArray<TSharedRef<const FManagedArrayCollection>>& InClothCollections = ClothAsset->GetClothCollections();
			if (ImportLod >= 0 && InClothCollections.Num() > ImportLod)
			{
				// Copy cloth
				const FCollectionClothConstFacade InClothFacade(InClothCollections[ImportLod]);
				ClothFacade.Initialize(InClothFacade);

				// Copy properties
				const FCollectionPropertyConstFacade InPropertyFacade(InClothCollections[ImportLod]);
				if (InPropertyFacade.IsValid())
				{
					FCollectionPropertyMutableFacade PropertyFacade(ClothCollection);
					PropertyFacade.DefineSchema();
					constexpr bool bUpdateExistingPropertiesFalse = false; // There are no existing properties.
					PropertyFacade.Append(InClothCollections[ImportLod].ToSharedPtr(), bUpdateExistingPropertiesFalse);
				}

				// Copy selections
				const FCollectionClothSelectionConstFacade InSelectionFacade(InClothCollections[ImportLod]);
				if (InSelectionFacade.IsValid())
				{
					FCollectionClothSelectionFacade SelectionFacade(ClothCollection);
					SelectionFacade.DefineSchema();
					constexpr bool bOverwriteExistingIfMismatchedTrue = true;
					SelectionFacade.Append(InSelectionFacade, bOverwriteExistingIfMismatchedTrue);
				}
			}
		}
		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
}
}

#undef LOCTEXT_NAMESPACE
