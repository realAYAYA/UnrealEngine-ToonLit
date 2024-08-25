// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/TerminalNodeRefreshAssetCustomization.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SFilePathPicker.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorDirectories.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "TerminalNodeRefreshAssetCustomization"

namespace UE::Chaos::ClothAsset
{
	TSharedRef<IPropertyTypeCustomization> FTerminalNodeRefreshAssetCustomization::MakeInstance()
	{
		return MakeShareable(new FTerminalNodeRefreshAssetCustomization);
	}

	void FTerminalNodeRefreshAssetCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		RefreshAsset = StructPropertyHandle->GetChildHandle(TEXT("bRefreshAsset"));

		// Create whole row header
		HeaderRow
		.WholeRowContent()
		.MinDesiredWidth(250)
		.MaxDesiredWidth(0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "RoundButton")
				.ToolTipText(LOCTEXT("Button_RefreshAsset_Tooltip", "Refresh asset"))
				.OnClicked(this, &FTerminalNodeRefreshAssetCustomization::OnClicked)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Persona.ReimportAsset"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(4, 0, 0, 0))
					.VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFontBold())
						.Text(StructPropertyHandle->GetPropertyDisplayName())
						.ShadowOffset(FVector2D(1, 1))
					]
				]
			]
		];
	}

	FReply FTerminalNodeRefreshAssetCustomization::OnClicked()
	{
		if (RefreshAsset)
		{
			RefreshAsset->SetValue(true);
		}
		return FReply::Handled();
	}
}  // End namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
