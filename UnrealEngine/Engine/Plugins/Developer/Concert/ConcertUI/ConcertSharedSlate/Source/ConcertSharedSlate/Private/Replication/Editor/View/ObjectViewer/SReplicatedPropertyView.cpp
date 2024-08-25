// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicatedPropertyView.h"

#include "Replication/Editor/Model/IReplicationStreamModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"

#include "Algo/AllOf.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicatedPropertyView"

namespace UE::ConcertSharedSlate
{
	void SReplicatedPropertyView::Construct(const FArguments& InArgs, TSharedRef<IPropertyTreeView> InPropertyTreeView, TSharedRef<IReplicationStreamModel> InPropertiesModel)
	{
		ReplicatedProperties = MoveTemp(InPropertyTreeView);
		PropertiesModel = MoveTemp(InPropertiesModel);
		
		GetSelectedRootObjectsDelegate = InArgs._GetSelectedRootObjects;
		check(GetSelectedRootObjectsDelegate.IsBound());
		
		ChildSlot
		[
			CreatePropertiesView(InArgs)
		];
	}
	
	void SReplicatedPropertyView::RefreshPropertyData()
	{
		TArray<FSoftObjectPath> SelectedObjects = GetObjectsSelectedForPropertyEditing();
		if (SelectedObjects.IsEmpty())
		{
			SetPropertyContent(EReplicatedPropertyContent::NoSelection);
			return;
		}

		// Technically, the classes just need to be compatible with each other... but it is easier to just allow the same class.
		const TOptional<FSoftClassPath> SharedClass = GetClassForPropertiesFromSelection(SelectedObjects);
		if (!SharedClass)
		{
			SetPropertyContent(EReplicatedPropertyContent::SelectionTooBig);
			return;
		}
		
		const FSoftClassPath Class = *SharedClass;
		if (!ensure(Class.IsValid()))
		{
			return;
		}
		
		// Build the set of properties that are shared by all of the selected objects
		TSet<FConcertPropertyChain> SharedProperties = PropertiesModel->GetAllProperties(SelectedObjects[0]);
		for (int32 i = 1; i < SelectedObjects.Num(); ++i)
		{
			SharedProperties = PropertiesModel->GetAllProperties(SelectedObjects[i]).Union(SharedProperties);
		}
		
		// If the objects have changed, the classes may share properties.
		// In that case, below we'd reuse the item pointer, which would cause the tree view to re-use the old row widgets.
		// However, we must regenerate all column widgets since they may be referencing the object the row was originally built for. So they'd display the state of the previous object still!
		// Example: Assign property combo-box in Multi-User All Clients view displays who has the property assigned.
		// Note: If the objects did not change, we definitely want to reuse item pointers since otherwise the user row selection is reset.
		const bool bCanReusePropertyData = PreviousSelectedObjects == SelectedObjects; // This SHOULD be an order independent compare but usually Num == 1, so whatever
		
		ReplicatedProperties->RefreshPropertyData(SharedProperties, Class, bCanReusePropertyData);
		
		SetPropertyContent(EReplicatedPropertyContent::Properties);
		PreviousSelectedObjects = MoveTemp(SelectedObjects);
	}

	TArray<FSoftObjectPath> SReplicatedPropertyView::GetObjectsSelectedForPropertyEditing() const
	{
		TArray<FSoftObjectPath> Result;
		Algo::Transform(GetSelectedRootObjectsDelegate.Execute(), Result, [](const TSharedPtr<FReplicatedObjectData>& ObjectData)
		{
			return ObjectData->GetObjectPath();
		});
		return Result;
	}

	TSharedRef<SWidget> SReplicatedPropertyView::CreatePropertiesView(const FArguments& InArgs)
	{
		return SAssignNew(PropertyContent, SWidgetSwitcher)
			// Make sure the slots are coherent with the order of EReplicatedPropertyContent!
			.WidgetIndex(static_cast<int32>(EReplicatedPropertyContent::NoSelection))
			
			// EReplicatedPropertyContent::Properties
			+SWidgetSwitcher::Slot()
			[
				ReplicatedProperties->GetWidget()
			]
			
			// EReplicatedPropertyContent::NoSelection
			+SWidgetSwitcher::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoPropertyEditedObjects", "Select an object to see selected properties"))
			]
			
			// EReplicatedPropertyContent::SelectionTooBig
			+SWidgetSwitcher::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectionTooBig", "Select objects of the same type type to see selected properties"))
			];
	}
	
	TOptional<FSoftClassPath> SReplicatedPropertyView::GetClassForPropertiesFromSelection(const TArray<FSoftObjectPath>& Objects) const
	{
		const FSoftClassPath Class = PropertiesModel->GetObjectClass(Objects[0]);
		const bool bAllHaveSameClass = Algo::AllOf(Objects, [this, Class](const FSoftObjectPath& Object)
		{
			return PropertiesModel->GetObjectClass(Object) == Class;
		});
		return bAllHaveSameClass ? Class : TOptional<FSoftClassPath>{};
	}

	void SReplicatedPropertyView::SetPropertyContent(EReplicatedPropertyContent Content) const
	{
		PropertyContent->SetActiveWidgetIndex(static_cast<int32>(Content));
	}
}

#undef LOCTEXT_NAMESPACE