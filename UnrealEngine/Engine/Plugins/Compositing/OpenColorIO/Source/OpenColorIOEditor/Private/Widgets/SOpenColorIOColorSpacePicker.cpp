// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOpenColorIOColorSpacePicker.h"

#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "OpenColorIOConfiguration.h"
#include "SResetToDefaultMenu.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SOpenColorIOColorPicker"

void SOpenColorIOColorSpacePicker::Construct(const FArguments& InArgs)
{
	Configuration = InArgs._Config;
	ColorSpaceSelection = InArgs._InitialColorSpace;
	RestrictedColorSpace = InArgs._RestrictedColor;
	DisplayViewSelection = InArgs._InitialDisplayView;
	bIsDestination = InArgs._IsDestination;
	OnColorSpaceChanged = InArgs._OnColorSpaceChanged;

	SelectionButton = SNew(SComboButton)
		.OnGetMenuContent_Lambda([this]() { return HandleColorSpaceComboButtonMenuContent(); })
		.ContentPadding(FMargin(4.0, 2.0));

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(MakeAttributeLambda([=]
				{
					if (DisplayViewSelection.IsValid())
					{
						return FText::FromString(*DisplayViewSelection.ToString());
					}
					else if (ColorSpaceSelection.ColorSpaceIndex != INDEX_NONE)
					{
						return FText::FromString(*ColorSpaceSelection.ToString());
					}
					else
					{
						return FText::FromString(TEXT("<Invalid>"));
					}
				}))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
		.VAlign(VAlign_Center)
		[
			SelectionButton.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f)
		[
			SNew(SButton)
			.ContentPadding(0)
			.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
			.ButtonStyle(FAppStyle::Get(), "ToggleButton") 
			.OnClicked(this, &SOpenColorIOColorSpacePicker::OnResetToDefault)
			.Visibility(this, &SOpenColorIOColorSpacePicker::ShouldShowResetToDefaultButton)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];
}

void SOpenColorIOColorSpacePicker::SetConfiguration(TWeakObjectPtr<UOpenColorIOConfiguration> NewConfiguration)
{
	Configuration = NewConfiguration;

	// Invalidate current color space selection
	SetCurrentColorSpace(FOpenColorIOColorSpace());
}

void SOpenColorIOColorSpacePicker::SetRestrictedColorSpace(const FOpenColorIOColorSpace& InRestrictedColorSpace)
{
	RestrictedColorSpace = InRestrictedColorSpace;
}

void SOpenColorIOColorSpacePicker::SetCurrentColorSpace(const FOpenColorIOColorSpace& NewColorSpace)
{
	ColorSpaceSelection = NewColorSpace;
	DisplayViewSelection.Reset();
	
	// Let listeners know selection has changed
	OnColorSpaceChanged.ExecuteIfBound(ColorSpaceSelection, DisplayViewSelection);

	//Close our menu
	if (SelectionButton.IsValid())
	{
		SelectionButton->SetIsOpen(false);
	}
}

void SOpenColorIOColorSpacePicker::SetCurrentDisplayView(const FOpenColorIODisplayView& NewDisplayView)
{
	ColorSpaceSelection.Reset();
	DisplayViewSelection = NewDisplayView;

	// Let listeners know selection has changed
	OnColorSpaceChanged.ExecuteIfBound(ColorSpaceSelection, DisplayViewSelection);

	//Close our menu
	if (SelectionButton.IsValid())
	{
		SelectionButton->SetIsOpen(false);
	}
}

TSharedRef<SWidget> SOpenColorIOColorSpacePicker::HandleColorSpaceComboButtonMenuContent()
{
	if (UOpenColorIOConfiguration* ConfigurationObject = Configuration.Get())
	{
		// generate menu
		const bool bShouldCloseWindowAfterClosing = false;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("AvailableColorSpaces", LOCTEXT("AvailableColorSpaces", "Available Color Spaces"));
		{
			bool ColorSpaceAdded = false;

			for (int32 i = 0; i < ConfigurationObject->DesiredColorSpaces.Num(); ++i)
			{
				const FOpenColorIOColorSpace& ColorSpace = ConfigurationObject->DesiredColorSpaces[i];
				if (ColorSpace == RestrictedColorSpace || !ColorSpace.IsValid())
				{
					continue;
				}

				MenuBuilder.AddMenuEntry
				(
					FText::FromString(ColorSpace.ToString()),
					FText::FromString(ColorSpace.ToString()),
					FSlateIcon(),
					FUIAction
					(
						FExecuteAction::CreateLambda([this, ColorSpace]
						{
							SetCurrentColorSpace(ColorSpace);
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([=]
						{
							return ColorSpaceSelection == ColorSpace;
						})
						),
						NAME_None,
						EUserInterfaceActionType::RadioButton
					);

				ColorSpaceAdded = true;
			}

			if (!ColorSpaceAdded)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoColorSpaceFound", "No available color spaces"), false, false);
			}
		}
		MenuBuilder.EndSection();

		if (bIsDestination)
		{
			MenuBuilder.BeginSection("AvailableDisplayViews", LOCTEXT("AvailableDisplayViews", "Available Display-Views"));
			{
				bool DisplayViewAdded = false;

				for (int32 i = 0; i < ConfigurationObject->DesiredDisplayViews.Num(); ++i)
				{
					const FOpenColorIODisplayView& DisplayView = ConfigurationObject->DesiredDisplayViews[i];
					if (DisplayView == DisplayViewSelection || !DisplayView.IsValid())
					{
						continue;
					}

					MenuBuilder.AddMenuEntry
					(
						FText::FromString(DisplayView.ToString()),
						FText::FromString(DisplayView.ToString()),
						FSlateIcon(),
						FUIAction
						(
							FExecuteAction::CreateLambda([this, DisplayView]
							{
								SetCurrentDisplayView(DisplayView);
							}),
							FCanExecuteAction(),
							FIsActionChecked::CreateLambda([=]
							{
								return DisplayViewSelection == DisplayView;
							})
						),
						NAME_None,
						EUserInterfaceActionType::RadioButton
					);

					DisplayViewAdded = true;
				}

				if (!DisplayViewAdded)
				{
					MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoDisplayViewFound", "No available display-view"), false, false);
				}
			}
			MenuBuilder.EndSection();
		}


		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

FReply SOpenColorIOColorSpacePicker::OnResetToDefault()
{
	SetCurrentColorSpace(FOpenColorIOColorSpace());
	SetCurrentDisplayView(FOpenColorIODisplayView());
	return FReply::Handled();
}

EVisibility SOpenColorIOColorSpacePicker::ShouldShowResetToDefaultButton() const
{
	return (ColorSpaceSelection.IsValid() || DisplayViewSelection.IsValid()) ? EVisibility::Visible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
