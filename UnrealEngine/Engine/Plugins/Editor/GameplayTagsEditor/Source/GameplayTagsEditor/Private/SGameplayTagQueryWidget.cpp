// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagQueryWidget.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "GameplayTagsManager.h"

#define LOCTEXT_NAMESPACE "GameplayTagQueryWidget"

void SGameplayTagQueryWidget::Construct(const FArguments& InArgs, const TArray<FEditableGameplayTagQueryDatum>& EditableTagQueries, const TSharedPtr<IPropertyHandle> InTagQueryPropertyHandle)
{
	ensure(EditableTagQueries.Num() > 0);
	TagQueries = EditableTagQueries;
	TagQueryPropertyHandle = InTagQueryPropertyHandle;

	bReadOnly = InArgs._ReadOnly;
	bAutoSave = InArgs._AutoSave;
	OnClosePreSave = InArgs._OnClosePreSave;
	OnSaveAndClose = InArgs._OnSaveAndClose;
	OnCancel = InArgs._OnCancel;
	OnQueryChanged = InArgs._OnQueryChanged;

	// Tag the assets as transactional so they can support undo/redo
	for (int32 AssetIdx = 0; AssetIdx < TagQueries.Num(); ++AssetIdx)
	{
		UObject* TagQueryOwner = TagQueries[AssetIdx].TagQueryOwner.Get();
		if (TagQueryOwner)
		{
			TagQueryOwner->SetFlags(RF_Transactional);
		}
	}

	UGameplayTagsManager::Get().OnGetCategoriesMetaFromPropertyHandle.AddSP(this, &SGameplayTagQueryWidget::OnGetCategoriesMetaFromPropertyHandle);

	// build editable query object tree from the runtime query data
	UEditableGameplayTagQuery* const EQ = CreateEditableQuery(*TagQueries[0].TagQuery);
	EditableQuery = EQ;

	// create details view for the editable query object
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.bShowObjectLabel = false;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Details = PropertyModule.CreateDetailView(ViewArgs);
	Details->SetObject(EQ);
	Details->OnFinishedChangingProperties().AddSP(this, &SGameplayTagQueryWidget::OnFinishedChangingProperties);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(!bReadOnly)
					.Visibility(this, &SGameplayTagQueryWidget::GetSaveAndCloseButtonVisibility)
					.OnClicked(this, &SGameplayTagQueryWidget::OnSaveAndCloseClicked)
					.Text(LOCTEXT("GameplayTagQueryWidget_SaveAndClose", "Save and Close"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility(this, &SGameplayTagQueryWidget::GetCancelButtonVisibility)
					.OnClicked(this, &SGameplayTagQueryWidget::OnCancelClicked)
					.Text(LOCTEXT("GameplayTagQueryWidget_Cancel", "Close Without Saving"))
				]
			]
			// to delete!
			+ SVerticalBox::Slot()
			[
				Details.ToSharedRef()
			]
		]
	];
}

void SGameplayTagQueryWidget::OnGetCategoriesMetaFromPropertyHandle(TSharedPtr<IPropertyHandle> PropertyHandle, FString& MetaString)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for (const UObject* Object : OuterObjects)
	{
		UObject* Outer = Object->GetOuter();
		while (Outer)
		{
			if (Outer == EditableQuery)
			{
				// This is us, re-route
				MetaString = UGameplayTagsManager::StaticGetCategoriesMetaFromPropertyHandle(TagQueryPropertyHandle);
				return;
			}
			Outer = Outer->GetOuter();
		}
	}
}

void SGameplayTagQueryWidget::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Auto saved changes will not call pre and post notify; auto save should only be used to make changes coming from blueprints
	if (bAutoSave)
	{
		SaveToTagQuery();
	}

	OnQueryChanged.ExecuteIfBound();
}

EVisibility SGameplayTagQueryWidget::GetSaveAndCloseButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SGameplayTagQueryWidget::GetCancelButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

UEditableGameplayTagQuery* SGameplayTagQueryWidget::CreateEditableQuery(FGameplayTagQuery& Q)
{
	UEditableGameplayTagQuery* const AnEditableQuery = Q.CreateEditableQuery();
	if (AnEditableQuery)
	{
		// prevent GC, will explicitly remove from root later
		AnEditableQuery->AddToRoot();
	}

	return AnEditableQuery;
}

SGameplayTagQueryWidget::~SGameplayTagQueryWidget()
{
	// clean up our temp editing uobjects
	UEditableGameplayTagQuery* const Q = EditableQuery.Get();
	if (Q)
	{
		Q->RemoveFromRoot();
	}
	
	UGameplayTagsManager::Get().OnGetCategoriesMetaFromPropertyHandle.RemoveAll(this);
}


void SGameplayTagQueryWidget::SaveToTagQuery()
{
	if (!EditableQuery.IsValid() || bReadOnly)
	{
		return;
	}

	FGameplayTagQuery NewQuery;
	NewQuery.BuildFromEditableQuery(*EditableQuery.Get());
	FString NewQueryAsString = EditableQuery.Get()->GetTagQueryExportText(NewQuery);

	// set query through property handle if possible to propagate changes to loaded instances
	// Note that all queries share the same property handle so we only need to set once since it will
	// take care of applying to all selected objects
	IPropertyHandle* PropertyHandle = TagQueryPropertyHandle.Get();
	if (PropertyHandle != nullptr)
	{
		PropertyHandle->SetValueFromFormattedString(NewQueryAsString);
	}

	// write to all selected queries
	for (FEditableGameplayTagQueryDatum& TQ : TagQueries)
	{
		if (TQ.TagQueryExportText != nullptr)
		{
			*TQ.TagQueryExportText = NewQueryAsString;
		}

		// when property handle is not available (e.g. BP pin) we set the query and dirty the package manually
		if (PropertyHandle == nullptr)
		{
			*TQ.TagQuery = NewQuery;
			if (TQ.TagQueryOwner.IsValid())
			{
				TQ.TagQueryOwner->MarkPackageDirty();
			}
		}
	}
}

FReply SGameplayTagQueryWidget::OnSaveAndCloseClicked()
{
	OnClosePreSave.ExecuteIfBound();

	SaveToTagQuery();

	OnSaveAndClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SGameplayTagQueryWidget::OnCancelClicked() const
{
	OnCancel.ExecuteIfBound();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
