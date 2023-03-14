// Copyright Epic Games, Inc. All Rights Reserved.

#include "STransportControl.h"

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "Framework/SlateDelegates.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "Layout/Clipping.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Misc/EnumRange.h"
#include "SlateGlobals.h"
#include "SlateOptMacros.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateTypes.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "STransportControl"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SWidget> STransportControl::MakeTransportControlWidget(ETransportControlWidgetType WidgetType, bool bAreButtonsFocusable, const FOnMakeTransportWidget& MakeCustomWidgetDelegate)
{
	switch(WidgetType)
	{
	case ETransportControlWidgetType::BackwardEnd:
		return SNew(SButton)
			. ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Backward_End"))
			. OnClicked(TransportControlArgs.OnBackwardEnd)
			. Visibility(TransportControlArgs.OnBackwardEnd.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToFront", "To Front") )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable);
	case ETransportControlWidgetType::BackwardStep:
		return SNew(SButton)
			. ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Backward_Step"))
			. OnClicked(TransportControlArgs.OnBackwardStep)
			. Visibility(TransportControlArgs.OnBackwardStep.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToPrevious", "To Previous") )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable);
	case ETransportControlWidgetType::BackwardPlay:
		return SAssignNew(BackwardPlayButton, SButton)
			. OnClicked(TransportControlArgs.OnBackwardPlay)
			. Visibility(TransportControlArgs.OnBackwardPlay.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("Reverse", "Reverse") )
			. ButtonStyle( FAppStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable)
			[
				SNew(SImage)
				. Image( this, &STransportControl::GetBackwardStatusIcon )
			];
	case ETransportControlWidgetType::Record:
		return SAssignNew(RecordButton, SButton)
			. ButtonStyle(FAppStyle::Get(), "NoBorder")
			. OnClicked(TransportControlArgs.OnRecord)
			. Visibility(TransportControlArgs.OnRecord.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( this, &STransportControl::GetRecordStatusTooltip )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable)
			[
				SNew(SImage)
				.Image(this, &STransportControl::GetRecordStatusIcon )
			];
	case ETransportControlWidgetType::ForwardPlay:
		return SAssignNew(ForwardPlayButton, SButton)
			. OnClicked(TransportControlArgs.OnForwardPlay)
			. Visibility(TransportControlArgs.OnForwardPlay.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( this, &STransportControl::GetForwardStatusTooltip )
			. ButtonStyle( FAppStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable)
			[
				SNew(SImage)
				. Image( this, &STransportControl::GetForwardStatusIcon )
			];
	case ETransportControlWidgetType::ForwardStep:
		return SNew(SButton)
			. ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Forward_Step"))
			. OnClicked(TransportControlArgs.OnForwardStep)
			. Visibility(TransportControlArgs.OnForwardStep.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToNext", "To Next") )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable);
	case ETransportControlWidgetType::ForwardEnd:
		return SNew(SButton)
			. ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Forward_End"))
			. OnClicked(TransportControlArgs.OnForwardEnd)
			. Visibility(TransportControlArgs.OnForwardEnd.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( LOCTEXT("ToEnd", "To End") )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable);
	case ETransportControlWidgetType::Loop:
		return SAssignNew(LoopButton, SButton)
			. OnClicked(TransportControlArgs.OnToggleLooping)
			. Visibility(TransportControlArgs.OnGetLooping.IsBound() ? EVisibility::Visible : EVisibility::Collapsed)
			. ToolTipText( this, &STransportControl::GetLoopStatusTooltip )
			. ButtonStyle( FAppStyle::Get(), "NoBorder" )
			. ContentPadding(2.0f)
			. IsFocusable(bAreButtonsFocusable)
			[
				SNew(SImage)
				. Image( this, &STransportControl::GetLoopStatusIcon )
			];
	case ETransportControlWidgetType::Custom:
		if(MakeCustomWidgetDelegate.IsBound())
		{
			return MakeCustomWidgetDelegate.Execute();
		}
		break;
	}

	return nullptr;
}

void STransportControl::Construct( const STransportControl::FArguments& InArgs )
{
	TransportControlArgs = InArgs._TransportArgs;
	
	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::ClipToBounds);

	if(TransportControlArgs.WidgetsToCreate.Num() > 0)
	{
		for(FTransportControlWidget WidgetDesc : TransportControlArgs.WidgetsToCreate)
		{
			TSharedPtr<SWidget> Widget = MakeTransportControlWidget(WidgetDesc.WidgetType, InArgs._TransportArgs.bAreButtonsFocusable, WidgetDesc.MakeCustomWidgetDelegate);
			if(Widget.IsValid())
			{
				HorizontalBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					Widget.ToSharedRef()
				];
			}
		}	
	}
	else
	{
		for(ETransportControlWidgetType WidgetType : TEnumRange<ETransportControlWidgetType>())
		{
			TSharedPtr<SWidget> Widget = MakeTransportControlWidget(WidgetType, InArgs._TransportArgs.bAreButtonsFocusable);
			if(Widget.IsValid())
			{
				HorizontalBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					Widget.ToSharedRef()
				];
			}
		}
	}

	ChildSlot
	[
		HorizontalBox
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool STransportControl::IsTickable() const
{
	// Only bother if an active timer delegate was provided
	return TransportControlArgs.OnTickPlayback.IsBound() && TransportControlArgs.OnGetPlaybackMode.IsBound();
}

void STransportControl::Tick( float DeltaTime )
{
	const auto PlaybackMode = TransportControlArgs.OnGetPlaybackMode.Execute();
	const bool bIsPlaying = PlaybackMode == EPlaybackMode::PlayingForward || PlaybackMode == EPlaybackMode::PlayingReverse;

	if ( bIsPlaying && !ActiveTimerHandle.IsValid() )
	{
		ActiveTimerHandle = RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &STransportControl::TickPlayback ) );
	}
	else if ( !bIsPlaying && ActiveTimerHandle.IsValid() )
	{
		UnRegisterActiveTimer( ActiveTimerHandle.Pin().ToSharedRef() );
	}
}

const FSlateBrush* STransportControl::GetForwardStatusIcon() const
{
	EPlaybackMode::Type PlaybackMode = EPlaybackMode::Stopped;
	if (TransportControlArgs.OnGetPlaybackMode.IsBound())
	{
		PlaybackMode = TransportControlArgs.OnGetPlaybackMode.Execute();
	}

	bool bIsRecording = false;
	if (TransportControlArgs.OnGetRecording.IsBound())
	{
		bIsRecording = TransportControlArgs.OnGetRecording.Execute();
	}

	if ( PlaybackMode == EPlaybackMode::PlayingForward)
	{
		return ForwardPlayButton.IsValid() && ForwardPlayButton->IsPressed() ? 
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Pause").Pressed : 
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Pause").Normal;
	}

	return ForwardPlayButton.IsValid() && ForwardPlayButton->IsPressed() ? 
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Forward").Pressed :
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Forward").Normal;
}

FText STransportControl::GetForwardStatusTooltip() const
{
	if (TransportControlArgs.OnGetPlaybackMode.IsBound() &&
		TransportControlArgs.OnGetPlaybackMode.Execute() == EPlaybackMode::PlayingForward)
	{
		return LOCTEXT("Pause", "Pause");
	}

	return LOCTEXT("Play", "Play");
}

const FSlateBrush* STransportControl::GetRecordStatusIcon() const
{
	bool bIsRecording = false;
	if (TransportControlArgs.OnGetRecording.IsBound())
	{
		bIsRecording = TransportControlArgs.OnGetRecording.Execute();
	}

	if (bIsRecording)
	{
		return RecordButton.IsValid() && RecordButton->IsPressed() ?
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Recording").Pressed :
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Recording").Normal;
	}

	return RecordButton.IsValid() && RecordButton->IsPressed() ?
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Record").Pressed :
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Record").Normal;
}

FText STransportControl::GetRecordStatusTooltip() const
{
	if (TransportControlArgs.OnGetRecording.IsBound() &&
		TransportControlArgs.OnGetRecording.Execute())
	{
		return LOCTEXT("StopRecording", "Stop Recording");
	}

	return LOCTEXT("Record", "Record");
}

const FSlateBrush* STransportControl::GetBackwardStatusIcon() const
{
	if (TransportControlArgs.OnGetPlaybackMode.IsBound() &&
		TransportControlArgs.OnGetPlaybackMode.Execute() == EPlaybackMode::PlayingReverse)
	{
		return BackwardPlayButton.IsValid() && BackwardPlayButton->IsPressed() ? 
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Pause").Pressed : 
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Pause").Normal;
	}

	return BackwardPlayButton.IsValid() && BackwardPlayButton->IsPressed() ? 
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Backward").Pressed : 
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Backward").Normal;
}

const FSlateBrush* STransportControl::GetLoopStatusIcon() const
{
	if (TransportControlArgs.OnGetLooping.IsBound() &&
		TransportControlArgs.OnGetLooping.Execute())
	{
		return LoopButton.IsValid() && LoopButton->IsPressed() ? 
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Enabled").Pressed : 
			&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Enabled").Normal;
	}

	return LoopButton.IsValid() && LoopButton->IsPressed() ? 
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Disabled").Pressed : 
		&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Animation.Loop.Disabled").Normal;
}

FText STransportControl::GetLoopStatusTooltip() const
{
	if (TransportControlArgs.OnGetLooping.IsBound() &&
		TransportControlArgs.OnGetLooping.Execute())
	{
		return LOCTEXT("Looping", "Looping");
	}

	return LOCTEXT("NoLooping", "No Looping");
}

EActiveTimerReturnType STransportControl::TickPlayback( double InCurrentTime, float InDeltaTime )
{
	TransportControlArgs.OnTickPlayback.Execute( InCurrentTime, InDeltaTime );
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
