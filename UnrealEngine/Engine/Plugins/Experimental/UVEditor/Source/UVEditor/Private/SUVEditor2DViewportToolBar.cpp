// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUVEditor2DViewportToolBar.h"

#include "EditorViewportCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UVEditorCommands.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "UVEditorUXSettings.h"
#include "UVEditor2DViewportClient.h"
#include "SViewportToolBarComboMenu.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "UVEditor2DViewportToolbar"

void SUVEditor2DViewportToolBar::Construct(const FArguments& InArgs)
{
	CommandList = InArgs._CommandList;
	Viewport2DClient = InArgs._Viewport2DClient;

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
		.Cursor(EMouseCursor::Default)
		[
			SNew( SHorizontalBox )

			// The first slot is just a spacer so that we get three evenly spaced columns 
			// and the selection toolbar can go in the center of the center one.
			+ SHorizontalBox::Slot()
			.Padding(ToolbarSlotPadding)
			.HAlign(HAlign_Right)

			+ SHorizontalBox::Slot()
			.Padding(ToolbarSlotPadding)
			.HAlign(HAlign_Center)
			[
				MakeSelectionToolBar(InArgs._Extenders)
			]

	        + SHorizontalBox::Slot()
			.Padding(ToolbarSlotPadding)
			.HAlign(HAlign_Right)
			[
				MakeTransformToolBar(InArgs._Extenders)
			]

			+ SHorizontalBox::Slot()
			.Padding(ToolbarSlotPadding)
			.HAlign(HAlign_Right)
			[
				MakeGizmoToolBar(InArgs._Extenders)
			]
		]
	];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::MakeSelectionToolBar(const TSharedPtr<FExtender> InExtenders)
{

	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls should not be focusable as it fights with the press space to change transform 
	// mode feature, which we may someday have.
	ToolbarBuilder.SetIsFocusable(false);

	// Widget controls
	ToolbarBuilder.BeginSection("SelectionModes");
	{
		ToolbarBuilder.BeginBlockGroup();

		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().VertexSelection, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TEXT("VertexSelection"));

		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().EdgeSelection, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TEXT("EdgeSelection"));

		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().TriangleSelection, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TEXT("TriangleSelection"));

		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().IslandSelection, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TEXT("IslandSelection"));

		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().FullMeshSelection, NAME_None,
			TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TEXT("FullMeshSelection"));

		ToolbarBuilder.EndBlockGroup();
	}

	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::MakeGizmoToolBar(const TSharedPtr<FExtender> InExtenders)
{
	// The following is modeled after portions of STransformViewportToolBar, which gets 
	// used in SCommonEditorViewportToolbarBase.

	// The buttons are hooked up to actual functions via command bindings in SEditorViewport::BindCommands(),
	// and the toolbar gets built in SUVEditor2DViewport::MakeViewportToolbar().

	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls should not be focusable as it fights with the press space to change transform 
	// mode feature, which we may someday have.
	ToolbarBuilder.SetIsFocusable(false);

	// Widget controls
	ToolbarBuilder.BeginSection("Transform");
	{
		ToolbarBuilder.BeginBlockGroup();

		// Select Mode
		static FName SelectModeName = FName(TEXT("SelectMode"));
		ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().SelectMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), SelectModeName);

		// Translate Mode
		static FName TranslateModeName = FName(TEXT("TranslateMode"));
		ToolbarBuilder.AddToolBarButton(FEditorViewportCommands::Get().TranslateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateModeName);

		ToolbarBuilder.EndBlockGroup();
	}

	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

// This is mostly copied from the version found in STransformViewportToolbar.cpp, which provides the
// transform/snapping controls for the level editor main window. We only want a subset of that 
// functionality though and moreover we want to store the state in the UVEditor instead of the editor
// global settings. So we make a few tweaks here from the original to control the exact snapping options
// available and the activation methods.
TSharedRef<SWidget> SUVEditor2DViewportToolBar::MakeTransformToolBar(const TSharedPtr< FExtender > InExtenders)
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls cannot be focusable as it fights with the press space to change transform mode feature
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection("LocationGridSnap");
	{
		// Grab the existing UICommand 
		// TODO: Should we replace this with our own version at some point? 
		// Same for the other commands below for rotation and scaling.
		TSharedPtr<FUICommandInfo> Command = FEditorViewportCommands::Get().LocationGridSnap;

		static FName PositionSnapName = FName(TEXT("PositionSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(
			SNew(SViewportToolBarComboMenu)
			.Style(ToolBarStyle)
			.IsChecked(this, &SUVEditor2DViewportToolBar::IsLocationGridSnapChecked)
			.OnCheckStateChanged(this, &SUVEditor2DViewportToolBar::HandleToggleLocationGridSnap)
			.Label(this, &SUVEditor2DViewportToolBar::GetLocationGridLabel)
			.OnGetMenuContent(this, &SUVEditor2DViewportToolBar::FillLocationGridSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("LocationGridSnap_ToolTip", "Set the Translation Snap value"))
			.Icon(Command->GetIcon())
			.MinDesiredButtonWidth(24.0f)
			.ParentToolBar(SharedThis(this)),
			PositionSnapName,
			false,
			HAlign_Fill,

			// explicitly specify what this widget should look like as a menu item
			FNewMenuDelegate::CreateLambda([this, Command](FMenuBuilder& InMenuBuilder)
				{
					// TODO - debug why can't just use the Command / mapping isn't working 
					InMenuBuilder.AddMenuEntry(Command);

					InMenuBuilder.AddWrapperSubMenu(
						LOCTEXT("GridSnapMenuSettings", "Translation Snap Settings"),
						LOCTEXT("GridSnapMenuSettings_ToolTip", "Set the Translation Snap value"),
						FOnGetContent::CreateSP(this, &SUVEditor2DViewportToolBar::FillLocationGridSnapMenu),
						FSlateIcon(Command->GetIcon())
					);
				}
		));
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("RotationGridSnap");
	{
		// Grab the existing UICommand 
		TSharedPtr<FUICommandInfo> Command = FEditorViewportCommands::Get().RotationGridSnap;

		static FName RotationSnapName = FName(TEXT("RotationSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(
			SNew(SViewportToolBarComboMenu)
			.Style(ToolBarStyle)
			.IsChecked(this, &SUVEditor2DViewportToolBar::IsRotationGridSnapChecked)
			.OnCheckStateChanged(this, &SUVEditor2DViewportToolBar::HandleToggleRotationGridSnap)
			.Label(this, &SUVEditor2DViewportToolBar::GetRotationGridLabel)
			.OnGetMenuContent(this, &SUVEditor2DViewportToolBar::FillRotationGridSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("RotationGridSnap_ToolTip", "Set the Rotation Snap value"))
			.Icon(Command->GetIcon())
			.ParentToolBar(SharedThis(this)),
			RotationSnapName,
			false,
			HAlign_Fill,

			// explicitly specify what this widget should look like as a menu item
			FNewMenuDelegate::CreateLambda([this, Command](FMenuBuilder& InMenuBuilder)
				{
					InMenuBuilder.AddMenuEntry(Command);

					InMenuBuilder.AddWrapperSubMenu(
						LOCTEXT("RotationGridSnapMenuSettings", "Rotation Snap Settings"),
						LOCTEXT("RotationGridSnapMenuSettings_ToolTip", "Set the Rotation Snap value"),
						FOnGetContent::CreateSP(this, &SUVEditor2DViewportToolBar::FillRotationGridSnapMenu),
						FSlateIcon(Command->GetIcon())
					);
				}
		));
	}
	ToolbarBuilder.EndSection();


	ToolbarBuilder.BeginSection("ScaleGridSnap");
	{
		// Grab the existing UICommand 
		TSharedPtr<FUICommandInfo> Command = FEditorViewportCommands::Get().ScaleGridSnap;

		static FName ScaleSnapName = FName(TEXT("ScaleSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(
			SNew(SViewportToolBarComboMenu)
			.Cursor(EMouseCursor::Default)
			.Style(ToolBarStyle)
			.IsChecked(this, &SUVEditor2DViewportToolBar::IsScaleGridSnapChecked)
			.OnCheckStateChanged(this, &SUVEditor2DViewportToolBar::HandleToggleScaleGridSnap)
			.Label(this, &SUVEditor2DViewportToolBar::GetScaleGridLabel)
			.OnGetMenuContent(this, &SUVEditor2DViewportToolBar::FillScaleGridSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("ScaleGridSnap_ToolTip", "Set the Scaling Snap value"))
			.Icon(Command->GetIcon())
			.MinDesiredButtonWidth(24.0f)
			.ParentToolBar(SharedThis(this)),
			ScaleSnapName,
			false,
			HAlign_Fill,

			// explicitly specify what this widget should look like as a menu item
			FNewMenuDelegate::CreateLambda([this, Command](FMenuBuilder& InMenuBuilder)
				{
					InMenuBuilder.AddMenuEntry(Command);

					InMenuBuilder.AddWrapperSubMenu(
						LOCTEXT("ScaleGridSnapMenuSettings", "Scale Snap Settings"),
						LOCTEXT("ScaleGridSnapMenuSettings_ToolTip", "Set the Scale Snap value"),
						FOnGetContent::CreateSP(this, &SUVEditor2DViewportToolBar::FillScaleGridSnapMenu),
						FSlateIcon(Command->GetIcon())
					);
				}
		));
	}
	ToolbarBuilder.EndSection();

	TSharedRef<SWidget> TransformBar = ToolbarBuilder.MakeWidget();
	TransformBar->SetEnabled(TAttribute<bool>::CreateLambda([this]() {return Viewport2DClient->AreWidgetButtonsEnabled(); }));
	return TransformBar;
}

// The following methods again mimic the patterns found in the STransformViewportToolbar.cpp,
// to serve as a drop in replacement for the menu infrastructure above. These have been altered
// to adjust the menu options and change how the snap settings are stored/forwarded to the UVEditor.

FText SUVEditor2DViewportToolBar::GetLocationGridLabel() const
{
	return FText::AsNumber(Viewport2DClient->GetLocationGridSnapValue());
}

FText SUVEditor2DViewportToolBar::GetRotationGridLabel() const
{
	return FText::Format(LOCTEXT("GridRotation - Number - DegreeSymbol", "{0}\u00b0"), FText::AsNumber(Viewport2DClient->GetRotationGridSnapValue()));
}

FText SUVEditor2DViewportToolBar::GetScaleGridLabel() const
{
	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 5;

	const float CurGridAmount = Viewport2DClient->GetScaleGridSnapValue();
	return FText::AsNumber(CurGridAmount, &NumberFormattingOptions);
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::FillLocationGridSnapMenu()
{
	TArray<float> GridSizes;
	GridSizes.Reserve(FUVEditorUXSettings::MaxLocationSnapValue());
	for (int32 Index = 0; Index < FUVEditorUXSettings::MaxLocationSnapValue(); ++Index)
	{
		GridSizes.Add(FUVEditorUXSettings::LocationSnapValue(Index));
	}
	return BuildLocationGridCheckBoxList("Snap", LOCTEXT("LocationSnapText", "Snap Delta Distances"), GridSizes);
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::BuildLocationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes) const
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder LocationGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	LocationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for (int32 CurGridSizeIndex = 0; CurGridSizeIndex < InGridSizes.Num(); ++CurGridSizeIndex)
	{
		const float CurGridSize = InGridSizes[CurGridSizeIndex];

		LocationGridMenuBuilder.AddMenuEntry(
			FText::AsNumber(CurGridSize),
			FText::Format(LOCTEXT("LocationGridSize_ToolTip", "Sets snap delta to {0}"), FText::AsNumber(CurGridSize)),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, CurGridSize]() {Viewport2DClient->SetLocationGridSnapValue(CurGridSize); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, CurGridSize]() { return FMath::IsNearlyEqual(Viewport2DClient->GetLocationGridSnapValue(), CurGridSize); })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	LocationGridMenuBuilder.EndSection();

	return LocationGridMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::FillRotationGridSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return SNew(SUniformGridPanel)

		+ SUniformGridPanel::Slot(0, 0)
		[
			BuildRotationGridCheckBoxList("Common", LOCTEXT("RotationCommonText", "Common"), ViewportSettings->CommonRotGridSizes)
		]

	+ SUniformGridPanel::Slot(1, 0)
		[
			BuildRotationGridCheckBoxList("Div360", LOCTEXT("RotationDivisions360DegreesText", "Divisions of 360\u00b0"), ViewportSettings->DivisionsOf360RotGridSizes)
		];
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::BuildRotationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes) const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder RotationGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	RotationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for (int32 CurGridAngleIndex = 0; CurGridAngleIndex < InGridSizes.Num(); ++CurGridAngleIndex)
	{
		const float CurGridAngle = InGridSizes[CurGridAngleIndex];

		FText MenuName = FText::Format(LOCTEXT("RotationGridAngle", "{0}\u00b0"), FText::AsNumber(CurGridAngle)); /*degree symbol*/
		FText ToolTipText = FText::Format(LOCTEXT("RotationGridAngle_ToolTip", "Sets rotation snap angle to {0}"), MenuName); /*degree symbol*/

		RotationGridMenuBuilder.AddMenuEntry(
			MenuName,
			ToolTipText,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, CurGridAngle]() {Viewport2DClient->SetRotationGridSnapValue(CurGridAngle); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, CurGridAngle]() {return FMath::IsNearlyEqual(Viewport2DClient->GetRotationGridSnapValue(), CurGridAngle); })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	RotationGridMenuBuilder.EndSection();

	return RotationGridMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SUVEditor2DViewportToolBar::FillScaleGridSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
	const bool bShouldCloseWindowAfterMenuSelection = true;

	FNumberFormattingOptions NumberFormattingOptions;
	NumberFormattingOptions.MaximumFractionalDigits = 5;

	FMenuBuilder ScaleGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	ScaleGridMenuBuilder.BeginSection("ScaleSnapOptions", LOCTEXT("ScaleSnapOptions", "Scale Snap"));

	for (int32 CurGridAmountIndex = 0; CurGridAmountIndex < ViewportSettings->ScalingGridSizes.Num(); ++CurGridAmountIndex)
	{
		const float CurGridAmount = ViewportSettings->ScalingGridSizes[CurGridAmountIndex];

		FText MenuText;
		FText ToolTipText;

		if (GEditor->UsePercentageBasedScaling())
		{
			MenuText = FText::AsPercent(CurGridAmount / 100.0f, &NumberFormattingOptions);
			ToolTipText = FText::Format(LOCTEXT("ScaleGridAmountOld_ToolTip", "Snaps scale values to {0}"), MenuText);
		}
		else
		{
			MenuText = FText::AsNumber(CurGridAmount, &NumberFormattingOptions);
			ToolTipText = FText::Format(LOCTEXT("ScaleGridAmount_ToolTip", "Snaps scale values to increments of {0}"), MenuText);
		}

		ScaleGridMenuBuilder.AddMenuEntry(
			MenuText,
			ToolTipText,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, CurGridAmount]() {Viewport2DClient->SetScaleGridSnapValue(CurGridAmount); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, CurGridAmount]() {return FMath::IsNearlyEqual(Viewport2DClient->GetScaleGridSnapValue(), CurGridAmount); })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	ScaleGridMenuBuilder.EndSection();

	return ScaleGridMenuBuilder.MakeWidget();
}

ECheckBoxState SUVEditor2DViewportToolBar::IsLocationGridSnapChecked() const
{
	return Viewport2DClient->GetLocationGridSnapEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SUVEditor2DViewportToolBar::IsRotationGridSnapChecked() const
{
	return Viewport2DClient->GetRotationGridSnapEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SUVEditor2DViewportToolBar::IsScaleGridSnapChecked() const
{
	return Viewport2DClient->GetScaleGridSnapEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUVEditor2DViewportToolBar::HandleToggleLocationGridSnap(ECheckBoxState InState)
{
	Viewport2DClient->SetLocationGridSnapEnabled(InState == ECheckBoxState::Checked);
}

void SUVEditor2DViewportToolBar::HandleToggleRotationGridSnap(ECheckBoxState InState)
{
	Viewport2DClient->SetRotationGridSnapEnabled(InState == ECheckBoxState::Checked);
}

void SUVEditor2DViewportToolBar::HandleToggleScaleGridSnap(ECheckBoxState InState)
{
	Viewport2DClient->SetScaleGridSnapEnabled(InState == ECheckBoxState::Checked);
}

#undef LOCTEXT_NAMESPACE