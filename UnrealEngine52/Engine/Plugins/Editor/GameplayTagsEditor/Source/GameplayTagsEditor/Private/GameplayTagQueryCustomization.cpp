// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagQueryCustomization.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagContainer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Docking/TabManager.h"

#include "Editor.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "GameplayTagQueryCustomization"

void FGameplayTagQueryCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	check(StructPropertyHandle.IsValid());

	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();
	
	RefreshQueryDescription(); // will call BuildEditableQueryList();

	bool const bReadOnly = StructPropertyHandle->IsEditConst();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.Text(this, &FGameplayTagQueryCustomization::GetEditButtonText)
					.OnClicked(this, &FGameplayTagQueryCustomization::OnEditButtonClicked)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!bReadOnly)
					.Text(LOCTEXT("GameplayTagQueryCustomization_Clear", "Clear All"))
					.OnClicked(this, &FGameplayTagQueryCustomization::OnClearAllButtonClicked)
					.Visibility(this, &FGameplayTagQueryCustomization::GetClearAllVisibility)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(4.0f)
				.Visibility(this, &FGameplayTagQueryCustomization::GetQueryDescVisibility)
				[
					SNew(STextBlock)
					.Text(this, &FGameplayTagQueryCustomization::GetQueryDescText)
					.ToolTipText(this, &FGameplayTagQueryCustomization::GetQueryDescText)
					.AutoWrapText(true)
				]
			]
		];

	GEditor->RegisterForUndo(this);
}

FText FGameplayTagQueryCustomization::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

FText FGameplayTagQueryCustomization::GetEditButtonText() const
{
	if (StructPropertyHandle.IsValid())
	{
		bool const bReadOnly = StructPropertyHandle->IsEditConst();
		return
			bReadOnly
			? LOCTEXT("GameplayTagQueryCustomization_View", "View...")
			: LOCTEXT("GameplayTagQueryCustomization_Edit", "Edit...");
	}

	return FText();
}

FReply FGameplayTagQueryCustomization::OnClearAllButtonClicked()
{
	if (StructPropertyHandle.IsValid())
	{
		FString EmptyQueryAsString;
    	FGameplayTagQuery::StaticStruct()->ExportText(EmptyQueryAsString, &FGameplayTagQuery::EmptyQuery, &FGameplayTagQuery::EmptyQuery, /*OwnerObject*/nullptr, PPF_None, /*ExportRootScope*/nullptr);
    	StructPropertyHandle->SetValueFromFormattedString(EmptyQueryAsString);
	}

	RefreshQueryDescription();

	return FReply::Handled();
}

EVisibility FGameplayTagQueryCustomization::GetClearAllVisibility() const
{
	bool bAtLeastOneQueryIsNonEmpty = false;

	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery && EQ.TagQuery->IsEmpty() == false)
		{
			bAtLeastOneQueryIsNonEmpty = true;
			break;
		}
	}

	return bAtLeastOneQueryIsNonEmpty ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FGameplayTagQueryCustomization::GetQueryDescVisibility() const
{
	return QueryDescription.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void FGameplayTagQueryCustomization::RefreshQueryDescription()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableQueryList();

	// Clear the list
	QueryDescription.Empty();

	if (EditableQueries.Num() > 1)
	{
		QueryDescription = TEXT("Multiple Selected");
	}
	else if ((EditableQueries.Num() == 1) && (EditableQueries[0].TagQuery != nullptr))
	{
		QueryDescription = EditableQueries[0].TagQuery->GetDescription();
	}
}


FReply FGameplayTagQueryCustomization::OnEditButtonClicked()
{
	if (GameplayTagQueryWidgetWindow.IsValid())
	{
		// already open, just show it
		GameplayTagQueryWidgetWindow->BringToFront(true);
	}
	else
	{
		if (StructPropertyHandle.IsValid())
		{
			TArray<UObject*> OuterObjects;
			StructPropertyHandle->GetOuterObjects(OuterObjects);

			bool bReadOnly = StructPropertyHandle->IsEditConst();

			FText Title;
			if (OuterObjects.Num() > 1)
			{
				FText const AssetName = FText::Format(LOCTEXT("GameplayTagDetailsBase_MultipleAssets", "{0} Assets"), FText::AsNumber(OuterObjects.Num()));
				FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
				Title = FText::Format(LOCTEXT("GameplayTagQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
			}
			else if (OuterObjects.Num() > 0 && OuterObjects[0])
			{
				FText const AssetName = FText::FromString(OuterObjects[0]->GetName());
				FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
				Title = FText::Format(LOCTEXT("GameplayTagQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
			}

			GameplayTagQueryWidgetWindow = SNew(SWindow)
				.Title(Title)
				.HasCloseButton(false)
				.ClientSize(FVector2D(600, 400))
				[
					SNew(SGameplayTagQueryWidget, EditableQueries, StructPropertyHandle)
					.OnClosePreSave(this, &FGameplayTagQueryCustomization::PreSave)
					.OnSaveAndClose(this, &FGameplayTagQueryCustomization::CloseWidgetWindow, false)
					.OnCancel(this, &FGameplayTagQueryCustomization::CloseWidgetWindow, true)
					.ReadOnly(bReadOnly)
				];

			// NOTE: FGlobalTabmanager::Get()-> is actually dereferencing a SharedReference, not a SharedPtr, so it cannot be null.
			if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
			{
				FSlateApplication::Get().AddWindowAsNativeChild(GameplayTagQueryWidgetWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
			}
			else
			{
				FSlateApplication::Get().AddWindow(GameplayTagQueryWidgetWindow.ToSharedRef());
			}
		}
	}

	return FReply::Handled();
}

FGameplayTagQueryCustomization::~FGameplayTagQueryCustomization()
{
	if (GameplayTagQueryWidgetWindow.IsValid())
	{
		GameplayTagQueryWidgetWindow->RequestDestroyWindow();
	}
	GEditor->UnregisterForUndo(this);
}

void FGameplayTagQueryCustomization::BuildEditableQueryList()
{	
	EditableQueries.Empty();

	if (StructPropertyHandle.IsValid())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);
		
		for (int32 Idx = 0; Idx < RawStructData.Num(); ++Idx)
		{
			// Null outer objects may mean that we are inside a UDataTable. This is ok though. We can still dirty the data table via FNotify Hook. (see ::CloseWidgetWindow). However undo will not work.
			UObject* Obj = OuterObjects.IsValidIndex(Idx) ? OuterObjects[Idx] : nullptr;

			EditableQueries.Add(SGameplayTagQueryWidget::FEditableGameplayTagQueryDatum(Obj, (FGameplayTagQuery*)RawStructData[Idx]));
		}
	}	
}

void FGameplayTagQueryCustomization::PreSave()
{
	if (StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->NotifyPreChange();
	}
}

void FGameplayTagQueryCustomization::CloseWidgetWindow(bool WasCancelled)
{
	// Notify change.
	if (!WasCancelled && StructPropertyHandle.IsValid())
	{
		StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}

	if (GameplayTagQueryWidgetWindow.IsValid())
	{
		GameplayTagQueryWidgetWindow->RequestDestroyWindow();
		GameplayTagQueryWidgetWindow = nullptr;

		RefreshQueryDescription();
	}
}

#undef LOCTEXT_NAMESPACE
