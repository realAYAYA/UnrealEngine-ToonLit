// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagCustomization.h"
#include "Widgets/Input/SComboButton.h"

#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "GameplayTagsEditorModule.h"
#include "Widgets/Input/SHyperlink.h"

#define LOCTEXT_NAMESPACE "GameplayTagCustomization"

TSharedRef<IPropertyTypeCustomization> FGameplayTagCustomizationPublic::MakeInstance()
{
	return MakeInstanceWithOptions({});
}

TSharedRef<IPropertyTypeCustomization> FGameplayTagCustomizationPublic::MakeInstanceWithOptions(const FGameplayTagCustomizationOptions& Options)
{
	return MakeShareable(new FGameplayTagCustomization(Options));
}

FGameplayTagCustomization::FGameplayTagCustomization(const FGameplayTagCustomizationOptions& InOptions):
	Options(InOptions)
{}

void FGameplayTagCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TagContainer = MakeShareable(new FGameplayTagContainer);
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagChanged = FSimpleDelegate::CreateSP(this, &FGameplayTagCustomization::OnPropertyValueChanged);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagChanged);

	BuildEditableContainerList();

	FUIAction SearchForReferencesAction(FExecuteAction::CreateSP(this, &FGameplayTagCustomization::OnSearchForReferences));

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SAssignNew(EditButton, SComboButton)
			.OnGetMenuContent(this, &FGameplayTagCustomization::GetListContent)
			.OnMenuOpenChanged(this, &FGameplayTagCustomization::OnGameplayTagListMenuOpenStateChanged)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GameplayTagCustomization_Edit", "Edit"))
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Visibility(this, &FGameplayTagCustomization::GetVisibilityForTagTextBlockWidget, true)
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(this, &FGameplayTagCustomization::SelectedTag)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Visibility(this, &FGameplayTagCustomization::GetVisibilityForTagTextBlockWidget, false)
			.Padding(4.0f)
			[
				SNew(SHyperlink)
				.Text(this, &FGameplayTagCustomization::SelectedTag)
				.OnNavigate( this, &FGameplayTagCustomization::OnTagDoubleClicked)
			]
		]
	]
	.AddCustomContextMenuAction(SearchForReferencesAction,
		LOCTEXT("FGameplayTagCustomization_SearchForReferences", "Search For References"),
		LOCTEXT("FGameplayTagCustomization_SearchForReferencesTooltip", "Find references for this tag"),
		FSlateIcon());

	GEditor->RegisterForUndo(this);
}

void FGameplayTagCustomization::OnTagDoubleClicked()
{
	UGameplayTagsManager::Get().NotifyGameplayTagDoubleClickedEditor(TagName);
}

void FGameplayTagCustomization::OnSearchForReferences()
{
	FName TagFName(*TagName, FNAME_Find);
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound() && !TagFName.IsNone())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Add(FAssetIdentifier(FGameplayTag::StaticStruct(), TagFName));
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

EVisibility FGameplayTagCustomization::GetVisibilityForTagTextBlockWidget(bool ForTextWidget) const
{
	return (UGameplayTagsManager::Get().ShowGameplayTagAsHyperLinkEditor(TagName) ^ ForTextWidget) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FGameplayTagCustomization::GetListContent()
{
	BuildEditableContainerList();
	
	FString Categories = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);

	bool bReadOnly = StructPropertyHandle->IsEditConst();

	TSharedRef<SGameplayTagWidget> TagWidget =
		SNew(SGameplayTagWidget, EditableContainers)
		.Filter(Categories)
		.ReadOnly(bReadOnly)
		.TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
		.MultiSelect(false)
		.OnTagChanged(this, &FGameplayTagCustomization::OnTagChanged)
		.PropertyHandle(StructPropertyHandle)
		.ForceHideAddNewTag(Options.bForceHideAddTag)
		.ForceHideAddNewTagSource(Options.bForceHideAddTagSource);

	LastTagWidget = TagWidget;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400)
		[
			TagWidget
		];
}

void FGameplayTagCustomization::OnGameplayTagListMenuOpenStateChanged(bool bIsOpened)
{
	if (bIsOpened)
	{
		TSharedPtr<SGameplayTagWidget> TagWidget = LastTagWidget.Pin();
		if (TagWidget.IsValid())
		{
			EditButton->SetMenuContentWidgetToFocus(TagWidget->GetWidgetToFocusOnOpen());
		}
	}
}

void FGameplayTagCustomization::OnPropertyValueChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);
			FGameplayTagContainer* Container = EditableContainers[0].TagContainer;			
			if (Tag && Container)
			{
				Container->Reset();
				Container->AddTag(*Tag);
				TagName = Tag->ToString();
			}			
		}
	}
}

void FGameplayTagCustomization::OnTagChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);			

			// Update Tag from the one selected from list
			FGameplayTagContainer* Container = EditableContainers[0].TagContainer;
			if (Tag && Container)
			{
				for (auto It = Container->CreateConstIterator(); It; ++It)
				{
					*Tag = *It;
					TagName = It->ToString();
				}
			}
		}
	}
}

void FGameplayTagCustomization::PostUndo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

void FGameplayTagCustomization::PostRedo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

FGameplayTagCustomization::~FGameplayTagCustomization()
{
	// Forcibly close the popup to avoid crashes later
	if (EditButton.IsValid() && EditButton->IsOpen())
	{
		EditButton->SetIsOpen(false);
	}

	GEditor->UnregisterForUndo(this);
}

void FGameplayTagCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if(StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);
			if (Tag && Tag->IsValid())
			{
				TagName = Tag->ToString();
				TagContainer->AddTag(*Tag);
			}
		}

		EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(nullptr, TagContainer.Get()));
	}
}

FText FGameplayTagCustomization::SelectedTag() const
{
	return FText::FromString(*TagName);
}

TSharedRef<IPropertyTypeCustomization> FGameplayTagCreationWidgetHelperDetails::MakeInstance()
{
	return MakeShareable(new FGameplayTagCreationWidgetHelperDetails());
}

void FGameplayTagCreationWidgetHelperDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{

}

void FGameplayTagCreationWidgetHelperDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FString FilterString = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);
	const float MaxPropertyWidth = 480.0f;
	const float MaxPropertyHeight = 240.0f;

	StructBuilder.AddCustomRow(NSLOCTEXT("GameplayTagReferenceHelperDetails", "NewTag", "NewTag"))
		.ValueContent()
		.MaxDesiredWidth(MaxPropertyWidth)
		[
			SAssignNew(TagWidget, SGameplayTagWidget, TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum>())
			.Filter(FilterString)
		.NewTagName(FilterString)
		.MultiSelect(false)
		.GameplayTagUIMode(EGameplayTagUIMode::ManagementMode)
		.MaxHeight(MaxPropertyHeight)
		.NewTagControlsInitiallyExpanded(true)
		//.OnTagChanged(this, &FGameplayTagsSettingsCustomization::OnTagChanged)
		];

}

#undef LOCTEXT_NAMESPACE
