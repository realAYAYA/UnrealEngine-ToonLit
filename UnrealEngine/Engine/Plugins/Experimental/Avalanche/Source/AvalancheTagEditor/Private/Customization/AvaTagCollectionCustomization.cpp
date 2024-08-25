// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTagCollectionCustomization.h"
#include "AvaTagCollection.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaTagCollectionCustomization"

void FAvaTagCollectionCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	const TSharedPtr<IPropertyHandle> TagMapProperty = InDetailBuilder.GetProperty(UAvaTagCollection::GetTagMapName());
	TagMapProperty->MarkHiddenByCustomization();

	IDetailCategoryBuilder& Category = InDetailBuilder.EditCategory(TEXT("Tag"));

	TSharedRef<SBox> HeaderContentWidget = SNew(SBox)
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.MinDesiredWidth(250.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Category.GetDisplayName())
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					TagMapProperty->CreateDefaultPropertyButtonWidgets()
				]
			]
		];

	Category.HeaderContent(HeaderContentWidget, /*bWholeRowContent*/true);
	Category.AddCustomBuilder(MakeShared<FAvaTagMapBuilder>(TagMapProperty.ToSharedRef()), /*bForAdvanced*/false);
}

FAvaTagMapBuilder::FAvaTagMapBuilder(const TSharedRef<IPropertyHandle>& InTagMapProperty)
	: MapProperty(InTagMapProperty->AsMap())
	, BaseProperty(InTagMapProperty)
{
	check(MapProperty.IsValid());

	// Delegate for when the number of children in the array changes
	FSimpleDelegate OnNumChildrenChanged = FSimpleDelegate::CreateRaw(this, &FAvaTagMapBuilder::OnNumChildrenChanged);
	OnNumElementsChangedHandle = MapProperty->SetOnNumElementsChanged(OnNumChildrenChanged);

	BaseProperty->MarkHiddenByCustomization();
}

FAvaTagMapBuilder::~FAvaTagMapBuilder()
{
	MapProperty->UnregisterOnNumElementsChanged(OnNumElementsChangedHandle);
}

FName FAvaTagMapBuilder::GetName() const
{
	return BaseProperty->GetProperty()->GetFName();
}

void FAvaTagMapBuilder::GenerateChildContent(IDetailChildrenBuilder& InChildrenBuilder)
{
	uint32 NumChildren = 0;
	BaseProperty->GetNumChildren(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> TagHandle = BaseProperty->GetChildHandle(ChildIndex);
		if (!TagHandle.IsValid())
		{
			continue;
		}

		if (TSharedPtr<IPropertyHandle> TagNameHandle = TagHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTag, TagName)))
		{
			InChildrenBuilder.AddProperty(TagHandle.ToSharedRef())
				.CustomWidget()
				.WholeRowContent()
				.HAlign(HAlign_Left)
				[
					SNew(SBox)
					.MinDesiredWidth(250.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							TagNameHandle->CreatePropertyValueWidget()
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							CreatePropertyButtonsWidget(TagHandle)
						]
					]
				];
		}
	}
}

void FAvaTagMapBuilder::OnNumChildrenChanged()
{
	OnRebuildChildren.ExecuteIfBound();
}

TSharedPtr<IPropertyHandle> FAvaTagMapBuilder::GetPropertyHandle() const
{
	return BaseProperty;
}

void FAvaTagMapBuilder::SetOnRebuildChildren(FSimpleDelegate InOnRebuildChildren)
{
	OnRebuildChildren = InOnRebuildChildren;
}

TSharedRef<SWidget> FAvaTagMapBuilder::CreatePropertyButtonsWidget(TSharedPtr<IPropertyHandle> InTagHandle)
{
	FMenuBuilder MenuContentBuilder(true, nullptr, nullptr, true);

	MenuContentBuilder.AddMenuEntry(NSLOCTEXT("PropertyCustomizationHelpers", "DeleteButtonLabel", "Delete")
		, FText::GetEmpty()
		, FSlateIcon()
		, FExecuteAction::CreateSP(this, &FAvaTagMapBuilder::DeleteItem, InTagHandle));

	MenuContentBuilder.AddMenuEntry(LOCTEXT("SearchForReferencesLabel", "Search for References")
		, FText::GetEmpty()
		, FSlateIcon()
		, FExecuteAction::CreateSP(this, &FAvaTagMapBuilder::SearchForReferences, InTagHandle->GetKeyHandle()));

	return SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ContentPadding(2)
		.ForegroundColor(FSlateColor::UseForeground())
		.HasDownArrow(true)
		.MenuContent()
		[
			MenuContentBuilder.MakeWidget()
		];
}

void FAvaTagMapBuilder::DeleteItem(TSharedPtr<IPropertyHandle> InTagHandle)
{
	if (InTagHandle.IsValid())
	{
		MapProperty->DeleteItem(InTagHandle->GetArrayIndex());	
	}
}

void FAvaTagMapBuilder::SearchForReferences(TSharedPtr<IPropertyHandle> InTagIdHandle)
{
	if (!FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		return;
	}

	if (!InTagIdHandle.IsValid())
	{
		return;
	}

	TArray<const void*> TagIdRawData;
	InTagIdHandle->AccessRawData(TagIdRawData);
	if (TagIdRawData.IsEmpty() || !TagIdRawData[0])
	{
		return;
	}

	const FAvaTagId& TagId = *static_cast<const FAvaTagId*>(TagIdRawData[0]);

	TArray<FAssetIdentifier> AssetIdentifiers;
	AssetIdentifiers.Emplace(FAvaTagId::StaticStruct(), *TagId.ToString());
	FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
}

#undef LOCTEXT_NAMESPACE
