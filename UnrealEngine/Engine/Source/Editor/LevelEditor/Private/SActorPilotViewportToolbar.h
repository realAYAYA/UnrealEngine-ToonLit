// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Layout/Margin.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "SLevelViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"
#include "LevelEditorViewport.h"
#include "SViewportToolBar.h"
#include "LevelViewportActions.h"

#define LOCTEXT_NAMESPACE "SActorPilotViewportToolbar"

class SActorPilotViewportToolbar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SActorPilotViewportToolbar){}
		SLATE_ARGUMENT(TSharedPtr<SLevelViewport>, Viewport)
	SLATE_END_ARGS()

	FText GetActiveText() const
	{
		const AActor* Pilot = nullptr;
		auto ViewportPtr = Viewport.Pin();
		if (ViewportPtr.IsValid())
		{
			Pilot = ViewportPtr->GetLevelViewportClient().GetActiveActorLock().Get();
		}
		
		return Pilot ? FText::Format(LOCTEXT("ActiveText", "Pilot Actor: {0} "), FText::FromString(Pilot->GetActorLabel())) : FText();
	}

	EVisibility GetLockedTextVisibility() const
	{
		const AActor* Pilot = nullptr;
		auto ViewportPtr = Viewport.Pin();
		if (ViewportPtr.IsValid())
		{
			Pilot = ViewportPtr->GetLevelViewportClient().GetActiveActorLock().Get();
		}

		return Pilot && Pilot->IsLockLocation() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	void Construct(const FArguments& InArgs)
	{
		SViewportToolBar::Construct(SViewportToolBar::FArguments());

		Viewport = InArgs._Viewport;

		auto& ViewportCommands = FLevelViewportCommands::Get();

		FSlimHorizontalToolBarBuilder ToolbarBuilder(InArgs._Viewport->GetCommandList(), FMultiBoxCustomization::None);

		// Use a custom style
		FName ToolBarStyle = "EditorViewportToolBar";
		ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
		ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
		
		ToolbarBuilder.BeginSection("ActorPilot");
		{
			ToolbarBuilder.BeginBlockGroup();

			ToolbarBuilder.AddWidget(
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.StartToolbarImage"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(5.0f, 0.0f)
				[
						SNew(SImage)
						.Image(FAppStyle::Get()
						.GetBrush("LevelViewport.PilotSelectedActor"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SActorPilotViewportToolbar::GetActiveText)
				]
				+ SHorizontalBox::Slot()
				.Padding(5.0f, 0.0f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ActorLockedText", "(Locked)"))
					.ToolTipText(LOCTEXT("ActorLockedToolTipText", "This actor has locked movement so it will not be updated based on camera position"))
					.Visibility(this, &SActorPilotViewportToolbar::GetLockedTextVisibility)
				]
			]
		);
			
			static const FName EjectActorPilotName = "EjectActorPilot";
			ToolbarBuilder.AddToolBarButton(ViewportCommands.EjectActorPilot, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), EjectActorPilotName);
			
			static FName ToggleActorPilotCameraViewName = "ToggleActorPilotCameraView";
			ToolbarBuilder.AddToolBarButton(ViewportCommands.ToggleActorPilotCameraView, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ToggleActorPilotCameraViewName);
			ToolbarBuilder.EndBlockGroup();
		}
		ToolbarBuilder.EndSection();

		ChildSlot
		[
			SNew( SBorder )
			.BorderImage( FAppStyle::GetBrush("NoBorder") )
			.Padding(FMargin(4.f, 0.f))
			[
				ToolbarBuilder.MakeWidget()
			]
		];
	}

private:

	/** The viewport that we are in */
	TWeakPtr<SLevelViewport> Viewport;
};

#undef LOCTEXT_NAMESPACE
