// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCAssetPathElementCustomization.h"

#include "Behaviour/Builtin/Path/RCSetAssetByPathBehaviour.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "UI/RemoteControlPanelStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RCAssetPathElementCustomization"

TSharedRef<IPropertyTypeCustomization> FRCAssetPathElementCustomization::MakeInstance()
{
	return MakeShared<FRCAssetPathElementCustomization>();
}

void FRCAssetPathElementCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	ArrayEntryHandle = InPropertyHandle;
	IsInputHandle = ArrayEntryHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRCAssetPathElement, bIsInput));
	PathHandle = ArrayEntryHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FRCAssetPathElement, Path));

	if (!IsInputHandle.IsValid() || !PathHandle.IsValid())
	{
		return;
	}

	const TSharedRef<SToolTip> GetAssetPathToolTipWidget = SNew(SToolTip)
		.Text(LOCTEXT("RCGetAssetPathButton_Tooltip", "Get the path of the currently first selected asset in the content browser and set it to the current path"));

	InHeaderRow.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		];

	InHeaderRow.ValueContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 0.f)
		.VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.Style(&FRemoteControlPanelStyle::Get()->GetWidgetStyle<FCheckBoxStyle>("RemoteControlPathBehaviour.AssetCheckBox"))
			.IsChecked(this, &FRCAssetPathElementCustomization::IsChecked)
			.OnCheckStateChanged(this, &FRCAssetPathElementCustomization::OnCheckStateChanged)
			.IsFocusable(false)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RCInputButtonAssetPath", "RCInput"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		]

		+SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			PathHandle->CreatePropertyValueWidget(false)
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked(this, &FRCAssetPathElementCustomization::OnGetAssetFromSelectionClicked)
			.ToolTip(GetAssetPathToolTipWidget)
			.IsFocusable(false)
			.ContentPadding(0)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Use"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

void FRCAssetPathElementCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
}

ECheckBoxState FRCAssetPathElementCustomization::IsChecked() const
{
	ECheckBoxState ReturnValue = ECheckBoxState::Undetermined;
	if (IsInputHandle.IsValid())
	{
		bool bIsInput;
		const FPropertyAccess::Result Result = IsInputHandle->GetValue(bIsInput);
		if (Result == FPropertyAccess::Success)
		{
			ReturnValue = bIsInput ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}
	return ReturnValue;
}

void FRCAssetPathElementCustomization::OnCheckStateChanged(ECheckBoxState InNewState) const
{
	if (!IsInputHandle.IsValid())
	{
		return;
	}
	IsInputHandle->SetValue(InNewState == ECheckBoxState::Checked ? true : false);
}

FReply FRCAssetPathElementCustomization::OnGetAssetFromSelectionClicked() const
{
	TArray<FAssetData> AssetData;
	GEditor->GetContentBrowserSelections(AssetData);
	if (AssetData.Num() <= 0)
	{
		return FReply::Handled();
	}
				
	// Clear it, in case it is an already used one.
	// Use the first one in the Array
	int32 IndexOfLast;
	const UObject* SelectedAsset = AssetData[0].GetAsset();
	FString PathString = SelectedAsset->GetPathName();

	// Remove the initial Game
	PathString.RemoveFromStart("/Game/");

	// Remove anything after the last /
	PathString.FindLastChar('/', IndexOfLast);
	if (IndexOfLast != INDEX_NONE)
	{
		PathString.RemoveAt(IndexOfLast, PathString.Len() - IndexOfLast);
	}
	else
	{
		// if the Index is -1 then it means that we are selecting an asset already in the topmost folder
		// So we clear the string since it will just contains the AssetName
		PathString.Empty();
	}

	if (IsInputHandle.IsValid())
	{
		IsInputHandle->SetValue(false);
	}
	if (PathHandle.IsValid())
	{
		PathHandle->SetValue(PathString);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
