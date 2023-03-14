// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXPixelMappingDetailCustomization_FixtureGroup.h"

#include "DMXPixelMapping.h"
#include "DMXPixelMappingLayoutSettings.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingFixtureGroupItemComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "DragDrop/DMXPixelMappingDragDropOp.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Widgets/SDMXPixelMappingFixturePatchDetailRow.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "IPropertyUtilities.h"
#include "ScopedTransaction.h"
#include "Layout/Visibility.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Views/SListView.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingDetailCustomization_FixtureGroup"

void FDMXPixelMappingDetailCustomization_FixtureGroup::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	PropertyUtilities = InDetailLayout.GetPropertyUtilities();

	// Hide the Layout Script property (shown in its own panel, see SDMXPixelMappingLayoutView)
	InDetailLayout.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupComponent, LayoutScript));

	// Handle size changes
	UpdateCachedScaleChildrenWithParent();
	SizeXHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeXPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	SizeXHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyPreChange));
	SizeXHandle->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyChanged));

	SizeYHandle = InDetailLayout.GetProperty(UDMXPixelMappingOutputComponent::GetSizeYPropertyName(), UDMXPixelMappingOutputComponent::StaticClass());
	SizeYHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyPreChange));
	SizeYHandle->SetOnPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyChanged));

	// Remember the group being edited
	WeakFixtureGroupComponent = GetSelectedFixtureGroupComponent(InDetailLayout);

	// Listen to the library being changed in the group component
	DMXLibraryHandle = InDetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXPixelMappingFixtureGroupComponent, DMXLibrary));
	DMXLibraryHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnLibraryChanged));
	DMXLibraryHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnLibraryChanged));

	// Listen to component changes
	UDMXPixelMappingBaseComponent::GetOnComponentAdded().AddSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnComponentAdded);
	UDMXPixelMappingBaseComponent::GetOnComponentRemoved().AddSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnComponentRemoved);

	// Add the library property
	IDetailCategoryBuilder& FixtureListCategoryBuilder = InDetailLayout.EditCategory("Fixture List", FText::GetEmpty(), ECategoryPriority::Important);
	FixtureListCategoryBuilder.AddProperty(DMXLibraryHandle);

	// Add an 'Add all Patches' button
	CreateAddAllPatchesButton(InDetailLayout);

	// Add Fixture Patches to the view
	CreateFixturePatchDetailRows(InDetailLayout);
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::CreateAddAllPatchesButton(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& FixtureListCategoryBuilder = InDetailLayout.EditCategory("Fixture List", FText::GetEmpty(), ECategoryPriority::Important);

	FixtureListCategoryBuilder.AddCustomRow(FText::GetEmpty())
		.WholeRowContent()
		[
			SNew(SButton)
			.Text(LOCTEXT("AddAllPatchesButtonText", "Add all Patches"))
			.OnClicked(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnAddAllPatchesClicked)
			.Visibility_Lambda([this]()
				{
					return NumFixturePatchRows > 0 ? EVisibility::Visible : EVisibility::Collapsed;
				})
		];
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::CreateFixturePatchDetailRows(IDetailLayoutBuilder& InDetailLayout)
{
	UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = WeakFixtureGroupComponent.Get();
	if (!FixtureGroupComponent)
	{
		return;
	}

	UDMXLibrary* DMXLibrary = GetSelectedDMXLibrary(FixtureGroupComponent);
	if (!DMXLibrary)
	{
		return;
	}

	UpdateFixturePatchesInUse(DMXLibrary);

	// Listen to the entities array being changed in the library
	EntitiesHandle = InDetailLayout.GetProperty(UDMXLibrary::GetEntitiesPropertyName());
	EntitiesHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::ForceRefresh));

	// Add fixture patches as custom rows
	TArray<UDMXEntityFixturePatch*> AllFixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	for (UDMXEntityFixturePatch* FixturePatch : AllFixturePatches)
	{
		const bool bPatchIsAssigned = IsFixturePatchAssignedToPixelMapping(FixturePatch);

		if (!bPatchIsAssigned)
		{
			TSharedRef<SDMXPixelMappingFixturePatchDetailRow> FixturePatchDetailRowWidget =
				SNew(SDMXPixelMappingFixturePatchDetailRow)
				.FixturePatch(FixturePatch)
				.OnLMBDown(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnFixturePatchLMBDown, FDMXEntityFixturePatchRef(FixturePatch))
				.OnLMBUp(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnFixturePatchLMBUp, FDMXEntityFixturePatchRef(FixturePatch))
				.OnDragged(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::OnFixturePatchesDragged);

			FDMXPixelMappingDetailCustomization_FixtureGroup::FDetailRowWidgetWithPatch DetailRowWidgetWithPatch;
			DetailRowWidgetWithPatch.DetailRowWidget = FixturePatchDetailRowWidget;
			DetailRowWidgetWithPatch.WeakFixturePatch = FixturePatch;
			DetailRowWidgetsWithPatch.Add(DetailRowWidgetWithPatch);

			IDetailCategoryBuilder& FixtureListCategoryBuilder = InDetailLayout.EditCategory("Fixture List", FText::GetEmpty(), ECategoryPriority::Important);
			FixtureListCategoryBuilder.AddCustomRow(FText::GetEmpty())
				.WholeRowContent()
				[
					FixturePatchDetailRowWidget
				];

			NumFixturePatchRows++;
		}
	}
}

FReply FDMXPixelMappingDetailCustomization_FixtureGroup::OnAddAllPatchesClicked()
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	if (!Toolkit.IsValid())
	{
		return FReply::Handled();
	}

	UDMXPixelMappingFixtureGroupComponent* GroupComponent = WeakFixtureGroupComponent.Get();
	if (!GroupComponent)
	{
		return FReply::Handled();
	}

	UDMXLibrary* DMXLibrary = GetSelectedDMXLibrary(GroupComponent);
	if (!DMXLibrary)
	{
		return FReply::Handled();
	}

	UDMXPixelMapping* PixelMapping = GroupComponent->GetPixelMapping();
	if (!PixelMapping)
	{
		return FReply::Handled();
	}

	UDMXPixelMappingRootComponent* RootComponent = PixelMapping->GetRootComponent();
	if (!RootComponent)
	{
		return FReply::Handled();
	}

	// Create components
	FScopedTransaction AddAllPatchesTransaction(LOCTEXT("AddAllPatchesTransaction", "Add all Patches to Pixel Mapping"));
	GroupComponent->Modify();

	TArray<UDMXPixelMappingOutputComponent*> NewComponents;
	for (const FDMXEntityFixturePatchRef& FixturePatchRef : FixturePatches)
	{
		UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();
		if (!FixturePatch)
		{
			continue;
		}

		const bool bPatchIsAssigned = IsFixturePatchAssignedToPixelMapping(FixturePatch);
		if (bPatchIsAssigned)
		{
			continue;
		}

		const FDMXFixtureMode* FixtureModePtr = FixturePatch->GetActiveMode();
		if (FixtureModePtr && FixtureModePtr->bFixtureMatrixEnabled)
		{
			const TSharedPtr<FDMXPixelMappingComponentTemplate> ComponentTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingMatrixComponent::StaticClass(), FixturePatchRef);
			UDMXPixelMappingMatrixComponent* NewMatrixComponent = ComponentTemplate->CreateComponent<UDMXPixelMappingMatrixComponent>(RootComponent);
			NewComponents.Add(NewMatrixComponent);

			GroupComponent->AddChild(NewMatrixComponent);
		}
		else
		{
			const TSharedPtr<FDMXPixelMappingComponentTemplate> ComponentTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingFixtureGroupItemComponent::StaticClass(), FixturePatchRef);
			UDMXPixelMappingFixtureGroupItemComponent* NewGroupItemComponent = ComponentTemplate->CreateComponent<UDMXPixelMappingFixtureGroupItemComponent>(RootComponent);
			NewComponents.Add(NewGroupItemComponent);

			GroupComponent->AddChild(NewGroupItemComponent);
		}
	}

	// Layout new components inside the group
	const int32 Columns = FMath::Sqrt((float)NewComponents.Num());
	const int32 Rows = FMath::RoundFromZero((float)NewComponents.Num() / Columns);
	const float Size = FMath::Min(GroupComponent->GetSize().X / Columns, GroupComponent->GetSize().Y / Rows);
	if (Size < 1.f)
	{
		GroupComponent->SetSize(FVector2D(Size * Columns, Size * Rows));
	}
	
	const FVector2D ParentPosition = GroupComponent->GetPosition();
	int32 Column = -1;
	int32 Row = 0;
	for (UDMXPixelMappingOutputComponent* NewComponent : NewComponents)
	{
		Column++;
		if (Column > Columns)
		{
			Column = 0;
			Row++;
		}
		const FVector2D Position = ParentPosition + FVector2D(Column * Size, Row * Size);
		NewComponent->SetPosition(Position);
	}

	return FReply::Handled();
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnLibraryChanged()
{
	// Remove patches not in the library
	if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin())
	{
		if (UDMXPixelMappingFixtureGroupComponent* GroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(WeakFixtureGroupComponent))
		{
			const FScopedTransaction ChangeDMXLibraryTransaction(LOCTEXT("DMXLibraryChangedResetTransactionReason", "PixelMapping Changed DMX Library"));

			TArray<UDMXPixelMappingBaseComponent*> CachedChildren(GroupComponent->Children);
			for (UDMXPixelMappingBaseComponent* ChildComponent : CachedChildren)
			{
				if (UDMXPixelMappingFixtureGroupItemComponent* ChildGroupItem = Cast<UDMXPixelMappingFixtureGroupItemComponent>(ChildComponent))
				{
					if (ChildGroupItem->FixturePatchRef.GetFixturePatch() &&
						ChildGroupItem->FixturePatchRef.GetFixturePatch()->GetParentLibrary() != GroupComponent->DMXLibrary)
					{
						GroupComponent->RemoveChild(ChildGroupItem);
					}
				}
				else if (UDMXPixelMappingMatrixComponent* ChildMatrix = Cast<UDMXPixelMappingMatrixComponent>(ChildComponent))
				{
					if (ChildMatrix->FixturePatchRef.GetFixturePatch() &&
						ChildMatrix->FixturePatchRef.GetFixturePatch()->GetParentLibrary() != GroupComponent->DMXLibrary)
					{
						GroupComponent->RemoveChild(ChildMatrix);
					}
				}
			};
		}
	}

	if (!RequestForceRefreshHandle.IsValid())
	{
		RequestForceRefreshHandle = FCoreDelegates::OnEndFrame.AddSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::ForceRefresh);
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	if (!RequestForceRefreshHandle.IsValid())
	{
		RequestForceRefreshHandle = FCoreDelegates::OnEndFrame.AddSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::ForceRefresh);
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	if (!RequestForceRefreshHandle.IsValid())
	{
		RequestForceRefreshHandle = FCoreDelegates::OnEndFrame.AddSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::ForceRefresh);
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyPreChange()
{
	UpdateCachedScaleChildrenWithParent();
	if (!bCachedScaleChildrenWithParent)
	{
		return;
	}

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();
	TArray<UDMXPixelMappingFixtureGroupComponent*> FixtureGroupComponents;
	for (TWeakObjectPtr<UObject> Object : SelectedObjects)
	{
		if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(Object))
		{
			FixtureGroupComponents.Add(FixtureGroupComponent);
		}
	}
	if (FixtureGroupComponents.IsEmpty())
	{
		return;
	}

	PreEditChangeComponentToSizeMap.Reset();
	for (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent : FixtureGroupComponents)
	{
		PreEditChangeComponentToSizeMap.Add(FixtureGroupComponent, FixtureGroupComponent->GetSize());
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnSizePropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FSimpleDelegate::CreateSP(this, &FDMXPixelMappingDetailCustomization_FixtureGroup::HandleSizePropertyChanged));
	}
	else
	{
		HandleSizePropertyChanged();
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::HandleSizePropertyChanged()
{
	// Scale children if desired
	if (!bCachedScaleChildrenWithParent || 
		PreEditChangeComponentToSizeMap.IsEmpty())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();
	TArray<UDMXPixelMappingFixtureGroupComponent*> FixtureGroupComponents;
	for (TWeakObjectPtr<UObject> Object : SelectedObjects)
	{
		if (UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(Object))
		{
			FixtureGroupComponents.Add(FixtureGroupComponent);
		}
	}
	if (FixtureGroupComponents.IsEmpty())
	{
		return;
	}

	for (const TTuple<TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent>, FVector2D>& PreEditChangeComponentToSizeXPair : PreEditChangeComponentToSizeMap)
	{
		UDMXPixelMappingFixtureGroupComponent* const* ComponentPtr = Algo::Find(FixtureGroupComponents, PreEditChangeComponentToSizeXPair.Key.Get());
		if (!ComponentPtr)
		{
			continue;
		}

		const FVector2D GroupPosition = (*ComponentPtr)->GetPosition();
		const FVector2D OldSize = PreEditChangeComponentToSizeXPair.Value;
		const FVector2D NewSize = (*ComponentPtr)->GetSize();
		if (NewSize == FVector2D::ZeroVector || OldSize == NewSize)
		{
			// No division by zero, no unchanged values
			return;
		}

		const FVector2D RatioVector = NewSize / OldSize;
		for (UDMXPixelMappingBaseComponent* BaseChild : (*ComponentPtr)->GetChildren())
		{
			if (UDMXPixelMappingOutputComponent* Child = Cast<UDMXPixelMappingOutputComponent>(BaseChild))
			{
				Child->Modify();

				// Scale size (SetSize already clamps)
				Child->SetSize(Child->GetSize() * RatioVector);

				// Scale position
				const FVector2D ChildPosition = Child->GetPosition();
				const FVector2D NewPositionRelative = (ChildPosition - GroupPosition) * RatioVector;
				Child->SetPosition(GroupPosition + NewPositionRelative);
			}
		}
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::ForceRefresh()
{
	// Reset the handles so they won't fire any changes after refreshing
	DMXLibraryHandle.Reset();
	EntitiesHandle.Reset();

	if (ensure(PropertyUtilities.IsValid()))
	{
		PropertyUtilities->ForceRefresh();
	}

	RequestForceRefreshHandle.Reset();
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnFixturePatchLMBDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FDMXEntityFixturePatchRef FixturePatchRef)
{
	UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();
	if (FixturePatch)
	{
		if (SelectedFixturePatches.Num() == 0)
		{
			SelectedFixturePatches = TArray<FDMXEntityFixturePatchRef>({ FixturePatchRef });
		}
		else
		{
			if (MouseEvent.IsShiftDown())
			{
				// Shift select
				const int32 IndexOfAnchor = DetailRowWidgetsWithPatch.IndexOfByPredicate([this](const FDMXPixelMappingDetailCustomization_FixtureGroup::FDetailRowWidgetWithPatch& DetailRowWidgetWithPatch) {
					return
						DetailRowWidgetWithPatch.WeakFixturePatch.IsValid() &&
						SelectedFixturePatches[0].GetFixturePatch() &&
						DetailRowWidgetWithPatch.WeakFixturePatch.Get() == SelectedFixturePatches[0].GetFixturePatch();
					});

				const int32 IndexOfSelected = DetailRowWidgetsWithPatch.IndexOfByPredicate([FixturePatch](const FDMXPixelMappingDetailCustomization_FixtureGroup::FDetailRowWidgetWithPatch& DetailRowWidgetWithPatch) {
					return
						DetailRowWidgetWithPatch.WeakFixturePatch.IsValid() &&
						DetailRowWidgetWithPatch.WeakFixturePatch.Get() == FixturePatch;
					});

				if (ensure(IndexOfSelected != INDEX_NONE))
				{
					if (IndexOfAnchor == INDEX_NONE || IndexOfAnchor == IndexOfSelected)
					{
						SelectedFixturePatches = TArray<FDMXEntityFixturePatchRef>({ FixturePatchRef });
					}
					else
					{
						SelectedFixturePatches.Reset();
						SelectedFixturePatches.Add(FDMXEntityFixturePatchRef(DetailRowWidgetsWithPatch[IndexOfAnchor].WeakFixturePatch.Get()));

						const bool bAscending = IndexOfAnchor < IndexOfSelected;
						const int32 StartIndex = bAscending ? IndexOfAnchor + 1 : IndexOfSelected;
						const int32 EndIndex = bAscending ? IndexOfSelected : IndexOfAnchor - 1;

						for (int32 IndexDetailRowWidget = StartIndex; IndexDetailRowWidget <= EndIndex; IndexDetailRowWidget++)
						{
							if (UDMXEntityFixturePatch* NewlySelectedPatch = DetailRowWidgetsWithPatch[IndexDetailRowWidget].WeakFixturePatch.Get())
							{
								SelectedFixturePatches.AddUnique(FDMXEntityFixturePatchRef(NewlySelectedPatch));
							}
						}
					}
				}
			}
			else if (MouseEvent.IsControlDown())
			{
				// Ctrl select
				if (!SelectedFixturePatches.Contains(FixturePatchRef))
				{
					SelectedFixturePatches.Add(FixturePatchRef);
				}
				else
				{
					SelectedFixturePatches.Remove(FixturePatchRef);
				}
			}
		}

		UpdateFixturePatchHighlights();
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::OnFixturePatchLMBUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FDMXEntityFixturePatchRef FixturePatchRef)
{
	if (!MouseEvent.IsShiftDown() && !MouseEvent.IsControlDown())
	{
		// Make a new selection
		SelectedFixturePatches.Reset();
		SelectedFixturePatches.Add(FixturePatchRef);

		UpdateFixturePatchHighlights();
	}
}

FReply FDMXPixelMappingDetailCustomization_FixtureGroup::OnFixturePatchesDragged(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (WeakFixtureGroupComponent.IsValid())
	{
		TArray<TSharedPtr<FDMXPixelMappingComponentTemplate>> Templates;
		for (const FDMXEntityFixturePatchRef& FixturePatchRef : SelectedFixturePatches)
		{
			const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.GetFixturePatch();
			const UDMXEntityFixtureType* FixtureType = FixturePatch ? FixturePatch->GetFixtureType() : nullptr;
			const FDMXFixtureMode* ActiveModePtr = FixturePatch ? FixturePatch->GetActiveMode() : nullptr;
			if (FixturePatch && FixtureType && ActiveModePtr)
			{
				if (ActiveModePtr->bFixtureMatrixEnabled)
				{
					TSharedRef<FDMXPixelMappingComponentTemplate> FixturePatchMatrixTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingMatrixComponent::StaticClass(), FixturePatchRef);
					Templates.Add(FixturePatchMatrixTemplate);
				}
				else
				{
					TSharedRef<FDMXPixelMappingComponentTemplate> FixturePatchItemTemplate = MakeShared<FDMXPixelMappingComponentTemplate>(UDMXPixelMappingFixtureGroupItemComponent::StaticClass(), FixturePatchRef);
					Templates.Add(FixturePatchItemTemplate);
				}
			}
		}

		UpdateFixturePatchHighlights();

		ForceRefresh();

		return FReply::Handled().BeginDragDrop(FDMXPixelMappingDragDropOp::New(FVector2D::ZeroVector, Templates, WeakFixtureGroupComponent.Get()));
	}

	return FReply::Handled();
}

bool FDMXPixelMappingDetailCustomization_FixtureGroup::IsFixturePatchAssignedToPixelMapping(UDMXEntityFixturePatch* FixturePatch) const
{
	if (!WeakFixtureGroupComponent.IsValid())
	{
		return true;
	}

	const bool bPatchIsAssigned = WeakFixtureGroupComponent->Children.ContainsByPredicate([FixturePatch](UDMXPixelMappingBaseComponent* BaseComponent)
		{
			if (UDMXPixelMappingFixtureGroupItemComponent* GroupItemComponent = Cast<UDMXPixelMappingFixtureGroupItemComponent>(BaseComponent))
			{
				return GroupItemComponent->FixturePatchRef.GetFixturePatch() == FixturePatch;
			}
			else if (UDMXPixelMappingMatrixComponent* MatrixComponent = Cast<UDMXPixelMappingMatrixComponent>(BaseComponent))
			{
				return MatrixComponent->FixturePatchRef.GetFixturePatch() == FixturePatch;
			}
			return false;
		});

	return bPatchIsAssigned;
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::UpdateFixturePatchHighlights()
{
	for (const FDMXPixelMappingDetailCustomization_FixtureGroup::FDetailRowWidgetWithPatch& DetailRowWidgetWithPatch : DetailRowWidgetsWithPatch)
	{
		bool bIsSelected = SelectedFixturePatches.ContainsByPredicate([DetailRowWidgetWithPatch](const FDMXEntityFixturePatchRef& SelectedRef) {
			return
				SelectedRef.GetFixturePatch() &&
				DetailRowWidgetWithPatch.WeakFixturePatch.IsValid() &&
				SelectedRef.GetFixturePatch() == DetailRowWidgetWithPatch.WeakFixturePatch.Get();
			});

		DetailRowWidgetWithPatch.DetailRowWidget->SetHighlight(bIsSelected);
	}
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::UpdateFixturePatchesInUse(UDMXLibrary* DMXLibrary)
{
	TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	FixturePatches.Reset(FixturePatchesInLibrary.Num());
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesInLibrary)
	{
		FDMXEntityFixturePatchRef FixturePatchRef;
		FixturePatchRef.SetEntity(FixturePatch);
		FixturePatches.Add(FixturePatchRef);
	}

	SelectedFixturePatches.RemoveAll([&FixturePatchesInLibrary](const FDMXEntityFixturePatchRef& SelectedFixturePatch) {
		return !SelectedFixturePatch.GetFixturePatch() || !FixturePatchesInLibrary.Contains(SelectedFixturePatch.GetFixturePatch());
		});
}

void FDMXPixelMappingDetailCustomization_FixtureGroup::UpdateCachedScaleChildrenWithParent()
{
	const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
	if (LayoutSettings)
	{
		bCachedScaleChildrenWithParent = LayoutSettings->bScaleChildrenWithParent;
	}
}

UDMXLibrary* FDMXPixelMappingDetailCustomization_FixtureGroup::GetSelectedDMXLibrary(UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent) const
{
	if (FixtureGroupComponent)
	{
		return FixtureGroupComponent->DMXLibrary;
	}

	return nullptr;
}

UDMXPixelMappingFixtureGroupComponent* FDMXPixelMappingDetailCustomization_FixtureGroup::GetSelectedFixtureGroupComponent(const IDetailLayoutBuilder& InDetailLayout) const
{
	const TArray<TWeakObjectPtr<UObject> >& SelectedSelectedObjects = InDetailLayout.GetSelectedObjects();
	TArray<UDMXPixelMappingFixtureGroupComponent*> SelectedSelectedComponents;

	for (TWeakObjectPtr<UObject> SelectedObject : SelectedSelectedObjects)
	{
		if (UDMXPixelMappingFixtureGroupComponent* SelectedComponent = Cast<UDMXPixelMappingFixtureGroupComponent>(SelectedObject.Get()))
		{
			SelectedSelectedComponents.Add(SelectedComponent);
		}
	}

	// we only support 1 uobject editing for now
	// we set singe UObject here SDMXPixelMappingDetailsView::OnSelectedComponenetChanged()
	// and that is why we getting only one UObject from here TArray<TWeakObjectPtr<UObject>>& SDetailsView::GetSelectedObjects()
	check(SelectedSelectedComponents.Num());
	return SelectedSelectedComponents[0];
}

#undef LOCTEXT_NAMESPACE
