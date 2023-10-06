// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "Dataflow/DataflowInputOutput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SelectionNode)

#define LOCTEXT_NAMESPACE "FChaosClothAssetSelectionNode"

FChaosClothAssetSelectionNode::FChaosClothAssetSelectionNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterInputConnection(&Collection);
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&Name);
}

void FChaosClothAssetSelectionNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InSelectionCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> SelectionCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InSelectionCollection));
		FCollectionClothSelectionFacade SelectionFacade(SelectionCollection);
		SelectionFacade.DefineSchema();
		
		checkSlow(SelectionFacade.IsValid());

		const int32 PreviousNumSelections = SelectionFacade.GetNumSelections();
		const int32 NewSelectionIndex = SelectionFacade.FindOrAddSelection(Name);
		checkf(NewSelectionIndex != INDEX_NONE, TEXT("Failed to find or add a new Selection"));

		if (NewSelectionIndex != PreviousNumSelections)
		{
			FClothDataflowTools::LogAndToastWarning(*this,
				LOCTEXT("SelectionAlreadyExistsHeadline", "Selection already exists"),
				FText::Format(LOCTEXT("SelectionAlreadyExistsDetails", "A Selection with Name \"{0}\" already exists in the Collection. Its data will be overwritten."),
					FText::FromString(Name)));
		}

		const UEnum* const SelectionTypeEnum = StaticEnum<EChaosClothAssetSelectionType>();

		const bool bSuccess = SelectionFacade.SetSelection(NewSelectionIndex, SelectionTypeEnum->GetNameStringByValue(static_cast<int32>(Type)), Indices);
		checkf(bSuccess, TEXT("Failed to set value on a Selection"));

		SetValue(Context, MoveTemp(*SelectionCollection), &Collection);
		SetValue(Context, Name, &Name);
	}
}


#undef LOCTEXT_NAMESPACE
