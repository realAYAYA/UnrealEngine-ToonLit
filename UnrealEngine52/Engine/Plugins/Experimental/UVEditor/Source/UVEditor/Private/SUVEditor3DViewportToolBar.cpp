// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUVEditor3DViewportToolBar.h"

#include "EditorViewportCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SEditorViewportToolBarMenu.h"
#include "SEditorViewportViewMenu.h"
#include "SUVEditor3DViewport.h"
#include "Styling/AppStyle.h"
#include "UVEditorCommands.h"
#include "UVEditorStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SUVEditor3DViewportToolBar"

void SUVEditor3DViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SUVEditor3DViewport> InUVEditor3DViewport)
{
	UVEditor3DViewportPtr = InUVEditor3DViewport;
	CommandList = InArgs._CommandList;

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	TSharedPtr<SHorizontalBox> MainBoxPtr;

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
		.Cursor(EMouseCursor::Default)
		[
			SAssignNew( MainBoxPtr, SHorizontalBox )
		]
	];

	MainBoxPtr->AddSlot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Left)
		[
			MakeDisplayToolBar(InArgs._Extenders)
		];

	MainBoxPtr->AddSlot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Right)
		[
			MakeToolBar(InArgs._Extenders)
		];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SUVEditor3DViewportToolBar::MakeDisplayToolBar(const TSharedPtr<FExtender> InExtenders)
{
	TSharedRef<SEditorViewport> ViewportRef = StaticCastSharedPtr<SEditorViewport>(UVEditor3DViewportPtr.Pin()).ToSharedRef();

	return SNew(SEditorViewportViewMenu, ViewportRef, SharedThis(this))
		.Cursor(EMouseCursor::Default)
		.MenuExtenders(InExtenders);
}

TSharedRef<SWidget> SUVEditor3DViewportToolBar::MakeToolBar(const TSharedPtr<FExtender> InExtenders)
{
	// The following is modeled after portions of STransformViewportToolBar, which gets 
	// used in SCommonEditorViewportToolbarBase.

	// The buttons are hooked up to actual functions via command bindings in SUVEditor3DViewport::BindCommands(),
	// and the toolbar gets built in SUVEditor3DViewport::MakeViewportToolbar().

	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);

	//// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.BeginSection("OrbitFlyToggle");
	{
		ToolbarBuilder.BeginBlockGroup();
	    
		// TODO: Right now we're (sort-of) hardcoding the icons in here. We should have a style set for the uv
		// editor that sets the correct icons for these.

		// Orbit Camera
		static FName OrbitCameraName = FName(TEXT("OrbitCamera"));
		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().EnableOrbitCamera, NAME_None, TAttribute<FText>(), TAttribute<FText>(), 
			TAttribute<FSlateIcon>(FSlateIcon(FUVEditorStyle::Get().GetStyleSetName(), "UVEditor.OrbitCamera")), OrbitCameraName);

		// Fly Camera
		static FName FlyCameraName = FName(TEXT("FlyCamera"));
		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().EnableFlyCamera, NAME_None, TAttribute<FText>(), TAttribute<FText>(),
			TAttribute<FSlateIcon>(FSlateIcon(FUVEditorStyle::Get().GetStyleSetName(), "UVEditor.FlyCamera")), FlyCameraName);

		ToolbarBuilder.EndBlockGroup();

		ToolbarBuilder.BeginBlockGroup();

		// Focus Camera
		static FName FocusCameraName = FName(TEXT("FocusCamera"));
		ToolbarBuilder.AddToolBarButton(FUVEditorCommands::Get().SetFocusCamera, NAME_None, TAttribute<FText>(), TAttribute<FText>(),
			TAttribute<FSlateIcon>(FSlateIcon(FUVEditorStyle::Get().GetStyleSetName(), "UVEditor.FocusCamera")), FocusCameraName);

		ToolbarBuilder.EndBlockGroup();
	}

	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
