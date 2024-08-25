// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataflowPreviewScene.h"
#include "Widgets/SCompoundWidget.h"
#include "ITransportControl.h"  // EPlaybackMode::Type

class SScrubControlPanel;
class SButton;
class UAnimSingleNodeInstance;

/** Dataflow simulation panel to control an animation/simulation */
class SDataflowSimulationPanel : public SCompoundWidget
{
private:

	/** Data flow playback mode */
	enum class EDataflowPlaybackMode : int32
	{
		Default,
		Looping,
		PingPong
	};

	/** View min/max to run the simulation */
	SLATE_BEGIN_ARGS(SDataflowSimulationPanel)	{}
		SLATE_ATTRIBUTE( float, ViewInputMin )
		SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_END_ARGS()

	/** Construct the simulation widget */
	void Construct( const FArguments& InArgs, TWeakPtr<FDataflowSimulationScene> InPreviewScene );

	/** Delegate when the playback mode is changed */
	TSharedRef<SWidget> OnCreatePreviewPlaybackModeWidget();

	/** Delegate when the simulation controls are pressed*/
	FReply OnClick_Forward_Step();
	FReply OnClick_Forward_End();
	FReply OnClick_Backward_Step();
	FReply OnClick_Backward_End();
	FReply OnClick_Forward();
	FReply OnClick_Backward();
	FReply OnClick_PreviewPlaybackMode();

	/** Change the playback settings */
	void ApplyPlaybackSettings();

	/** Main simulation delegates */
	void OnTickPlayback(double InCurrentTime, float InDeltaTime);
	void OnValueChanged(float NewValue);
	void OnBeginSliderMovement();

	/** Get the playback mode used in the widget */
	EPlaybackMode::Type GetPlaybackMode() const;

	/** Get ethe current scrub value */
	float GetScrubValue() const;

	/** Animation instance accessors */
	UAnimSingleNodeInstance* GetPreviewAnimationInstance();
	const UAnimSingleNodeInstance* GetPreviewAnimationInstance() const;

	/** Update the widget based on the modified animation instance*/
	void UpdatePreviewAnimationInstance();

	/** Get the number of keys */
	uint32 GetNumberOfKeys() const;

	/** Get the sequence length */
	float GetSequenceLength() const;

	/** Get the display drag */
	bool GetDisplayDrag() const;

	/** Simulation scene to be used for the widget */
	TWeakPtr<FDataflowSimulationScene> SimulationScene;

	/** Scrub widget defined for the timeline */
	TSharedPtr<SScrubControlPanel> ScrubControlPanel;

	/** Playback mode button */
	TSharedPtr<SButton> PreviewPlaybackModeButton;

	/** Preview playback mode (looping...)*/
	EDataflowPlaybackMode PreviewPlaybackMode = EDataflowPlaybackMode::Looping;
};
