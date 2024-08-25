// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Views/SPropertyAnimatorCoreEditorPropertiesView.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"
#include "Widgets/TableRows/SPropertyAnimatorCoreEditorPropertiesViewTableRow.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorPropertiesView"

void SPropertyAnimatorCoreEditorPropertiesView::Construct(const FArguments& InArgs, TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> InEditPanel)
{
	EditPanelWeak = InEditPanel;

	check(InEditPanel.IsValid());

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot(0)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(PropertiesTree, STreeView<FPropertiesViewItemPtr>)
		   .TreeItemsSource(&PropertiesTreeSource)
		   .SelectionMode(ESelectionMode::None)
		   .ItemHeight(30.f)
		   .HeaderRow(
			   SNew(SHeaderRow)
			   + SHeaderRow::Column(SPropertyAnimatorCoreEditorEditPanel::HeaderPropertyColumnName)
			   .DefaultLabel(FText::FromName(SPropertyAnimatorCoreEditorEditPanel::HeaderPropertyColumnName))
			   .FillWidth(0.4f)
			   + SHeaderRow::Column(SPropertyAnimatorCoreEditorEditPanel::HeaderAnimatorColumnName)
			   .DefaultLabel(FText::FromName(SPropertyAnimatorCoreEditorEditPanel::HeaderAnimatorColumnName))
			   .FillWidth(0.5f)
			   + SHeaderRow::Column(SPropertyAnimatorCoreEditorEditPanel::HeaderActionColumnName)
			   .DefaultLabel(FText::GetEmpty())
			   .FillWidth(0.1f)
		   )
		   .OnGetChildren(this, &SPropertyAnimatorCoreEditorPropertiesView::OnGetChildren)
		   .OnGenerateRow(this, &SPropertyAnimatorCoreEditorPropertiesView::OnGenerateRow)
		]
		+ SOverlay::Slot(1)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
	        .Text(LOCTEXT("PropertiesListEmpty", "No linked properties found on this actor"))
	        .Visibility(this, &SPropertyAnimatorCoreEditorPropertiesView::GetPropertyTextVisibility)
		]
	];
}

void SPropertyAnimatorCoreEditorPropertiesView::Update()
{
	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = EditPanelWeak.Pin();

	if (!EditPanel.IsValid())
	{
		return;
	}

	AActor* ContextActor = EditPanel->GetOptions().GetContextActor();

	if (!ContextActor)
	{
		return;
	}

	// Collect all existing controllers on actor
	const UPropertyAnimatorCoreSubsystem* PropertyControllerSubsystem = UPropertyAnimatorCoreSubsystem::Get();

	if (!PropertyControllerSubsystem)
	{
		return;
	}

	TSet<UPropertyAnimatorCoreBase*> ExistingControllers = PropertyControllerSubsystem->GetExistingAnimators(ContextActor);

	// Collect all linked root properties and their controllers
	TMap<FPropertyAnimatorCoreData, TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>> LinkedPropertiesMap;
	for (UPropertyAnimatorCoreBase* Controller : ExistingControllers)
	{
		for (const FPropertyAnimatorCoreData& LinkedProperty : Controller->GetLinkedProperties())
		{
			TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>& Controllers = LinkedPropertiesMap.FindOrAdd(LinkedProperty);
			Controllers.Add(Controller);
		}
	}

	// Only get root member properties to build the tree
	TMap<FPropertyAnimatorCoreData, TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>> MemberPropertiesMap;
	for (const TPair<FPropertyAnimatorCoreData, TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>>& LinkedPropertyPair : LinkedPropertiesMap)
	{
		TOptional<FPropertyAnimatorCoreData> MemberProperty = LinkedPropertyPair.Key.GetRootParent();

		if (MemberProperty.IsSet())
		{
			TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>& Controllers = MemberPropertiesMap.FindOrAdd(MemberProperty.GetValue());
			Controllers.Append(LinkedPropertyPair.Value);
		}
	}

	// Add resolvable properties
	FPropertyAnimatorCoreData Context(ContextActor, nullptr, nullptr);
	TSet<FPropertyAnimatorCoreData> ResolvableProperties;
	PropertyControllerSubsystem->GetResolvableProperties(Context, ResolvableProperties);

	for (const FPropertyAnimatorCoreData& ResolvableProperty : ResolvableProperties)
	{
		TSet<UPropertyAnimatorCoreBase*> LinkedControllers = PropertyControllerSubsystem->GetPropertyLinkedAnimators(ResolvableProperty);
		if (!LinkedControllers.IsEmpty())
		{
			TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>& Controllers = MemberPropertiesMap.Add(ResolvableProperty);
			Algo::Transform(LinkedControllers, Controllers, [](UPropertyAnimatorCoreBase* InController)
			{
				return InController;
			});
		}
	}

	PropertiesTreeSource.Empty(MemberPropertiesMap.Num());
	for (const TPair<FPropertyAnimatorCoreData, TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>>& MemberPropertyPair : MemberPropertiesMap)
	{
		TSharedPtr<FPropertiesViewItem> NewItem = MakeShared<FPropertiesViewItem>();
		NewItem->Property = MemberPropertyPair.Key;
		NewItem->ControllersWeak = MemberPropertyPair.Value;
		PropertiesTreeSource.Add(NewItem);
	}

	if (PropertiesTree.IsValid())
	{
		PropertiesTree->RebuildList();

		// Auto expand
		for (const FPropertiesViewItemPtr& Item : PropertiesTreeSource)
		{
			PropertiesTree->SetItemExpansion(Item, true);
		}
	}
}

TSharedRef<ITableRow> SPropertyAnimatorCoreEditorPropertiesView::OnGenerateRow(FPropertiesViewItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SPropertyAnimatorCoreEditorPropertiesViewTableRow, InOwnerTable, SharedThis(this), InItem);
}

void SPropertyAnimatorCoreEditorPropertiesView::OnGetChildren(FPropertiesViewItemPtr InItem, TArray<FPropertiesViewItemPtr>& OutChildren) const
{
	if (!InItem.IsValid())
	{
		return;
	}

	// Children already loaded
	if (!InItem->Children.IsEmpty())
	{
		OutChildren = InItem->Children;
		return;
	}

	// Map properties and controllers linked
	TMap<FPropertyAnimatorCoreData, TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>> ChildrenPropertiesMap;

	// Show resolvable properties tree
	if (InItem->Property.IsResolvable())
	{
		if (UPropertyAnimatorCoreSubsystem* ControllerSubsystem = UPropertyAnimatorCoreSubsystem::Get())
		{
			TSet<UPropertyAnimatorCoreBase*> ExistingControllers = ControllerSubsystem->GetExistingAnimators(InItem->Property);
			TSet<UPropertyAnimatorCoreBase*> AvailableControllers = ControllerSubsystem->GetAvailableAnimators(&InItem->Property);

			constexpr bool bRecursiveSearch = true;

			for (UPropertyAnimatorCoreBase* Controller : ExistingControllers)
			{
				TSet<FPropertyAnimatorCoreData> SupportedProperties;
				Controller->GetPropertiesSupported(InItem->Property, SupportedProperties, bRecursiveSearch);

				// Add available properties to control with this controller
				for (FPropertyAnimatorCoreData& SupportedProperty : SupportedProperties)
				{
					// Get only direct child of PropertyData and not nested ones
					if (TOptional<FPropertyAnimatorCoreData> ChildProperty = SupportedProperty.GetChildOf(InItem->Property))
					{
						TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>& Controllers = ChildrenPropertiesMap.FindOrAdd(ChildProperty.GetValue());

						// Check if we are controlling that property
						if (Controller->IsPropertyLinked(ChildProperty.GetValue()))
						{
							Controllers.Add(Controller);
						}
					}
				}
			}

			for (UPropertyAnimatorCoreBase* Controller : AvailableControllers)
			{
				TSet<FPropertyAnimatorCoreData> SupportedProperties;
				Controller->GetPropertiesSupported(InItem->Property, SupportedProperties, bRecursiveSearch);

				// Add all supported properties from new controller
				for (FPropertyAnimatorCoreData& SupportedProperty : SupportedProperties)
				{
					// Get only direct child of PropertyData and not nested ones
					if (TOptional<FPropertyAnimatorCoreData> ChildProperty = SupportedProperty.GetChildOf(InItem->Property))
					{
						ChildrenPropertiesMap.FindOrAdd(ChildProperty.GetValue());
					}
				}
			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& ControllerWeak : InItem->ControllersWeak)
		{
			UPropertyAnimatorCoreBase* Controller = ControllerWeak.Get();

			if (!Controller)
			{
				continue;
			}

			for (FPropertyAnimatorCoreData& InnerProperty : Controller->GetInnerPropertiesLinked(InItem->Property))
			{
				TOptional<FPropertyAnimatorCoreData> DirectPropertyChild = InnerProperty.GetChildOf(InItem->Property);

				if (DirectPropertyChild.IsSet())
				{
					TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>& Controllers = ChildrenPropertiesMap.FindOrAdd(DirectPropertyChild.GetValue());
					Controllers.Add(Controller);
				}
			}
		}
	}

	for (const TPair<FPropertyAnimatorCoreData, TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>>& ChildrenPropertyPair : ChildrenPropertiesMap)
	{
		TSharedPtr<FPropertiesViewItem> NewItem = MakeShared<FPropertiesViewItem>();
		NewItem->Property = ChildrenPropertyPair.Key;
		NewItem->ControllersWeak = ChildrenPropertyPair.Value;
		NewItem->ParentWeak = InItem;
		InItem->Children.Add(NewItem);
	}

	OutChildren = InItem->Children;
}

EVisibility SPropertyAnimatorCoreEditorPropertiesView::GetPropertyTextVisibility() const
{
	return PropertiesTreeSource.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
