// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SClothEditorViewportToolBarBase.h"
#include "SEditorViewport.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SChaosClothAssetEditorViewportToolBarBase"

TSharedRef<SWidget> SChaosClothAssetEditorViewportToolBarBase::GenerateClothViewportOptionsMenu() const
{
	// (See also SCommonEditorViewportToolbarBase::GenerateOptionsMenu)

	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();
	
	const bool bIsPerspective = GetViewportClient().GetViewportType() == LVT_Perspective;
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		OptionsMenuBuilder.BeginSection("ClothEditorViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options"));
		{
			if (bIsPerspective)
			{
				OptionsMenuBuilder.AddWidget(GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)"));
			}
		}
		OptionsMenuBuilder.EndSection();

		// TODO: Add any other options for our cloth viewports

		ExtendOptionsMenu(OptionsMenuBuilder);
	}

	return OptionsMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SChaosClothAssetEditorViewportToolBarBase::GenerateFOVMenu() const
{
	// Re-implementation of inaccessible SCommonEditorViewportToolbarBase::GenerateFOVMenu()

	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return 
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<float>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinValue(FOVMin)
					.MaxValue(FOVMax)
					.Value(this, &SChaosClothAssetEditorViewportToolBarBase::OnGetFOVValue)
					.OnValueChanged(this, &SChaosClothAssetEditorViewportToolBarBase::OnFOVValueChanged)
				]
			]
		];
}

float SChaosClothAssetEditorViewportToolBarBase::OnGetFOVValue() const
{
	// Re-implementation of inaccessible SCommonEditorViewportToolbarBase::OnGetFOVValue()

	return GetViewportClient().ViewFOV;
}

#undef LOCTEXT_NAMESPACE
