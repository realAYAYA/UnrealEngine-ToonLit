// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
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

	auto CopyIntoSelection = [this](const TSharedRef<FManagedArrayCollection> SelectionCollection, 
		const FName& SelectionGroupName, 
		const TSet<int32>& SourceIndices,
		TSet<int32>& DestSelectionSet)
	{
		const int32 NumElementsInGroup = SelectionCollection->NumElements(SelectionGroupName);
		bool bFoundAnyInvalidIndex = false;

		DestSelectionSet.Reset();

		for (const int32 Index : SourceIndices)
		{
			if (Index < 0 || Index >= NumElementsInGroup)
			{
				const FText LogErrorMessage = FText::Format(LOCTEXT("SelectionIndexOutOfBoundsDetails", "Selection index {0} not valid for group \"{1}\" with {2} elements"),
					Index,
					FText::FromName(SelectionGroupName),
					NumElementsInGroup);

				// Log all indices, but toast once
				UE_LOG(LogChaosClothAssetDataflowNodes, Warning, TEXT("%s"), *LogErrorMessage.ToString());
				bFoundAnyInvalidIndex = true;
			}
			else
			{
				DestSelectionSet.Add(Index);
			}
		}

		if (bFoundAnyInvalidIndex)
		{
			// Toast once
			const FText ToastErrorMessage = FText::Format(LOCTEXT("AnySelectionIndexOutOfBoundsDetails", "Found invalid selection indices for group \"{0}.\" See log for details"),
				FText::FromName(SelectionGroupName));
			FClothDataflowTools::LogAndToastWarning(*this, LOCTEXT("AnySelectionIndexOutOfBoundsHeadline", "Invalid selection"), ToastErrorMessage);
		}
	};


	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		if (Name.IsEmpty() || Group.Name.IsEmpty())
		{
			const FManagedArrayCollection& SelectionCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
			SetValue(Context, SelectionCollection, &Collection);
			return;
		}

		const FName SelectionName(*Name);
		const FName SelectionGroupName(*Group.Name);

		FManagedArrayCollection InSelectionCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> SelectionCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InSelectionCollection));

		FCollectionClothSelectionFacade SelectionFacade(SelectionCollection);
		SelectionFacade.DefineSchema();
		check(SelectionFacade.IsValid());

		TSet<int32>& SelectionSet = SelectionFacade.FindOrAddSelectionSet(SelectionName, SelectionGroupName);
		CopyIntoSelection(SelectionCollection, SelectionGroupName, Indices, SelectionSet);

		if (!SecondaryGroup.Name.IsEmpty() && !SecondaryIndices.IsEmpty())
		{
			const FName SecondarySelectionName(*Name);
			const FName SecondarySelectionGroupName = (*SecondaryGroup.Name);		
			TSet<int32>& SecondarySelectionSet = SelectionFacade.FindOrAddSelectionSecondarySet(SecondarySelectionName, SecondarySelectionGroupName);

			CopyIntoSelection(SelectionCollection, SecondarySelectionGroupName, SecondaryIndices, SecondarySelectionSet);
		}

		SetValue(Context, MoveTemp(*SelectionCollection), &Collection);
	}
	else if (Out->IsA<FString>(&Name))
	{
		SetValue(Context, Name, &Name);
	}
}

void FChaosClothAssetSelectionNode::OnSelected(Dataflow::FContext& Context)
{
	using namespace UE::Chaos::ClothAsset;

	// Re-evaluate the input collection
	FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));
	FCollectionClothFacade Cloth(ClothCollection);

	// Update the list of used group for the UI customization
	const TArray<FName> GroupNames = ClothCollection->GroupNames();
	CachedCollectionGroupNames.Reset(GroupNames.Num());
	for (const FName& GroupName : GroupNames)
	{
		if (Cloth.IsValidClothCollectionGroupName(GroupName))  // Restrict to the cloth facade groups
		{
			CachedCollectionGroupNames.Emplace(GroupName);
		}
	}
}

void FChaosClothAssetSelectionNode::OnDeselected()
{
	// Clean up, to avoid another toolkit picking up the wrong context evaluation
	CachedCollectionGroupNames.Reset();
}

void FChaosClothAssetSelectionNode::Serialize(FArchive& Ar)
{
	using namespace UE::Chaos::ClothAsset;

	// This is just for convenience and can be removed post 5.4 once the plugin loses its experimental status
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Ar.IsLoading() && Type_DEPRECATED != EChaosClothAssetSelectionType::Deprecated)
	{
		switch (Type_DEPRECATED)
		{
		case EChaosClothAssetSelectionType::SimVertex2D: Group.Name = ClothCollectionGroup::SimVertices2D.ToString(); break;
		case EChaosClothAssetSelectionType::SimVertex3D: Group.Name = ClothCollectionGroup::SimVertices3D.ToString(); break;
		case EChaosClothAssetSelectionType::RenderVertex: Group.Name = ClothCollectionGroup::RenderVertices.ToString(); break;
		case EChaosClothAssetSelectionType::SimFace: Group.Name = ClothCollectionGroup::SimFaces.ToString(); break;
		case EChaosClothAssetSelectionType::RenderFace: Group.Name = ClothCollectionGroup::RenderFaces.ToString(); break;
		default: checkNoEntry();
		}
		Type_DEPRECATED = EChaosClothAssetSelectionType::Deprecated;  // This is only for clarity since the Type property won't be saved from now on

		FClothDataflowTools::LogAndToastWarning(*this,
			LOCTEXT("DeprecatedSelectionType", "Outdated Dataflow asset."),
			LOCTEXT("DeprecatedSelectionDetails", "This node is out of date and contains deprecated data. The asset needs to be re-saved before it stops working at the next version update."));
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#undef LOCTEXT_NAMESPACE
