// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSystemViewportToolBar.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "NiagaraEditorCommands.h"
#include "EditorViewportCommands.h"
#include "NiagaraEditorSettings.h"
#include "SEditorViewportToolBarButton.h"
#include "PreviewProfileController.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemViewportToolBar"


///////////////////////////////////////////////////////////
// SNiagaraSystemViewportToolBar

void SNiagaraSystemViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SNiagaraSystemViewport> InViewport)
{
	WeakViewport = InViewport;

	// we don't want a realtime button here as we create a custom one by extending the left menu
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments().AddRealtimeButton(true).PreviewProfileController(MakeShared<FPreviewProfileController>()), InViewport);
	Sequencer = InArgs._Sequencer;
}

TSharedRef<SWidget> SNiagaraSystemViewportToolBar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		auto Commands = FNiagaraEditorCommands::Get();

		ShowMenuBuilder.AddMenuEntry(Commands.TogglePreviewGrid);
		ShowMenuBuilder.AddMenuSeparator();
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleEmitterExecutionOrder);
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleGpuTickInformation);
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleInstructionCounts);
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleMemoryInfo);
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleParticleCounts);
		ShowMenuBuilder.AddMenuEntry(Commands.ToggleStatelessInfo);
	}

	return ShowMenuBuilder.MakeWidget();
}

bool SNiagaraSystemViewportToolBar::IsViewModeSupported(EViewModeIndex ViewModeIndex) const 
{
	switch (ViewModeIndex)
	{
	case VMI_PrimitiveDistanceAccuracy:
	case VMI_MeshUVDensityAccuracy:
	case VMI_RequiredTextureResolution:
		return false;
	default:
		return true;
	}
}

EVisibility SNiagaraSystemViewportToolBar::GetSimulationRealtimeWarningVisibility() const
{
	if(Sequencer.IsValid())
	{
		return Sequencer.Pin()->GetPlaybackSpeed() == 1.f ? EVisibility::Collapsed : EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}

FReply SNiagaraSystemViewportToolBar::OnSimulationRealtimeWarningClicked() const
{
	Sequencer.Pin()->RestorePlaybackSpeed();
	return FReply::Handled();
}

FText SNiagaraSystemViewportToolBar::GetSimulationSpeedText() const
{
	if(Sequencer.IsValid())
	{
		return FText::Format(LOCTEXT("SimulationSpeed", "Speed: {0}"), FText::AsNumber(Sequencer.Pin()->GetPlaybackSpeed()));
	}

	return FText::GetEmpty();
}

void SNiagaraSystemViewportToolBar::ExtendOptionsMenu(FMenuBuilder& OptionsMenuBuilder) const
{
	OptionsMenuBuilder.BeginSection("LevelViewportNavigationOptions", LOCTEXT("NavOptionsMenuHeader", "Navigation Options"));
	OptionsMenuBuilder.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleOrbit);
	OptionsMenuBuilder.AddSubMenu(
		LOCTEXT("MotionOptions", "Motion Options"),
		LOCTEXT("MotionOptionsToolTip", "Set Motion Options For the Niagara Component"),
		FNewMenuDelegate::CreateSP(this, &SNiagaraSystemViewportToolBar::MakeMotionMenu)
	);

	OptionsMenuBuilder.EndSection();
}

void SNiagaraSystemViewportToolBar::ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const
{
	MainBoxPtr->AddSlot()
	.AutoWidth()
	[
		SNew(SEditorViewportToolBarButton)
		.Cursor(EMouseCursor::Default)
		.ButtonType(EUserInterfaceActionType::Button)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.WarningButton"))
		.OnClicked(this, &SNiagaraSystemViewportToolBar::OnSimulationRealtimeWarningClicked)
		.Visibility(this, &SNiagaraSystemViewportToolBar::GetSimulationRealtimeWarningVisibility)
		.ToolTipText(LOCTEXT("SimulationRealtimeOff_ToolTip", "This simulation is not updating in real time.  Click to turn on real time."))
		.Content()
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("EditorViewportToolBar.Font"))
			.Text(this, &SNiagaraSystemViewportToolBar::GetSimulationSpeedText)
			.ColorAndOpacity(FLinearColor::Black)
		]
	];
}

void SNiagaraSystemViewportToolBar::MakeMotionMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleMotion);

	if (TSharedPtr<SNiagaraSystemViewport> ViewportSP = WeakViewport.Pin())
	{
		TSharedRef<SNiagaraSystemViewport> Viewport = ViewportSP.ToSharedRef();

		MenuBuilder.AddWidget(
			SNew(SSpinBox<float>)
				.IsEnabled(Viewport, &SNiagaraSystemViewport::IsMotionEnabled)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.MinSliderValue(0.0f)
				.MaxSliderValue(360.0f)
				.Value(Viewport, &SNiagaraSystemViewport::GetMotionRate)
				.OnValueChanged(Viewport, &SNiagaraSystemViewport::SetMotionRate)
			,
			LOCTEXT("MotionSpeed", "Motion Speed")
		);

		MenuBuilder.AddWidget(
			SNew(SSpinBox<float>)
			.IsEnabled(Viewport, &SNiagaraSystemViewport::IsMotionEnabled)
			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.MinSliderValue(0.0f)
			.MaxSliderValue(1000.0f)
			.Value(Viewport, &SNiagaraSystemViewport::GetMotionRadius)
			.OnValueChanged(Viewport, &SNiagaraSystemViewport::SetMotionRadius)
			,
			LOCTEXT("MotionRadius", "Motion Radius")
		);
	}
}

#undef LOCTEXT_NAMESPACE
