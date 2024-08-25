// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkHubPlaybackWidget.h"

#include "FrameNumberNumericInterface.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SSimpleTimeSlider.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"

#define LOCTEXT_NAMESPACE "SLiveLinkHubPlaybackWidget"

void SLiveLinkHubPlaybackWidget::Construct(const FArguments& InArgs)
{
	OnPlayForwardDelegate = InArgs._OnPlayForward;
	OnPlayReverseDelegate = InArgs._OnPlayReverse;

	OnFirstFrameDelegate = InArgs._OnFirstFrame;
	OnLastFrameDelegate = InArgs._OnLastFrame;

	OnPreviousFrameDelegate = InArgs._OnPreviousFrame;
	OnNextFrameDelegate = InArgs._OnNextFrame;

	OnGetPausedDelegate = InArgs._IsPaused;
	OnGetIsInReverseDelegate = InArgs._IsInReverse;

	OnGetCurrentTimeDelegate = InArgs._GetCurrentTime;
	OnGetTotalLengthDelegate = InArgs._GetTotalLength;

	OnSetCurrentTimeDelegate = InArgs._SetCurrentTime;

	OnGetViewRangeDelegate = InArgs._GetViewRange;
	OnSetViewRangeDelegate = InArgs._SetViewRange;

	OnGetSelectionStartTimeDelegate = InArgs._GetSelectionStartTime;
	OnGetSelectionEndTimeDelegate = InArgs._GetSelectionEndTime;
	OnSetSelectionStartTimeDelegate = InArgs._SetSelectionStartTime;
	OnSetSelectionEndTimeDelegate = InArgs._SetSelectionEndTime;
	
	OnSetLoopingDelegate = InArgs._OnSetLooping;
	OnGetLoopingDelegate = InArgs._IsLooping;

	OnGetFrameRate = InArgs._GetFrameRate;

	const TAttribute<EFrameNumberDisplayFormats> GetDisplayFormatAttr = MakeAttributeSP(this, &SLiveLinkHubPlaybackWidget::GetDisplayFormat);
	const TAttribute<FFrameRate> GetDisplayRateAttr = MakeAttributeSP(this, &SLiveLinkHubPlaybackWidget::GetFrameRate);
	
	// Create our numeric type interface so we can pass it to the time slider below.
	NumberInterface = MakeShareable(new FFrameNumberInterface(GetDisplayFormatAttr, 0, GetDisplayRateAttr, GetDisplayRateAttr));

	NumberInterface->DisplayFormatChanged();
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(16.f, 8.f)
		[
			SNew(SSimpleTimeSlider)
			.ClampRangeHighlightSize(0.15f)
			.ClampRangeHighlightColor(FLinearColor::Gray.CopyWithNewOpacity(0.5f))
			.ScrubPosition_Lambda([this]()
			{
				// TimeSlider needs actual time in seconds, not the frame time.
				const double Seconds = GetCurrentTime() / GetFrameRate().Numerator;
				return Seconds;
			})
			.ViewRange(this, &SLiveLinkHubPlaybackWidget::GetViewRange)
			.OnViewRangeChanged(this, &SLiveLinkHubPlaybackWidget::SetViewRange)
			.ClampRange(this, &SLiveLinkHubPlaybackWidget::GetClampRange)
			.OnScrubPositionChanged_Lambda(
				[this](double NewScrubTime, bool bIsScrubbing)
						{
							if (bIsScrubbing)
							{
								//  Convert time in seconds to frame time as double.
								SetCurrentTime(NewScrubTime * GetFrameRate().Numerator);
							}
						})
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.f, 8.f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnFirstFramePressed)
				.ToolTipText(LOCTEXT("ToFront", "To Front"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Backward_End"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnPreviousFramePressed)
				.ToolTipText(LOCTEXT("ToPrevious", "To Previous"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Backward_Step"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnPlayReversePressed)
				.ToolTipText_Lambda([this]()
				{
					const bool bIsPaused = IsPaused();
					return bIsPaused ? LOCTEXT("ReverseButton", "Reverse") : LOCTEXT("PauseButton", "Pause");
				})
				.ButtonStyle( FAppStyle::Get(), "Animation.PlayControlsButton" )
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SLiveLinkHubPlaybackWidget::GetPlayReverseIcon)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnPlayForwardPressed)
				.ToolTipText_Lambda([this]()
				{
					const bool bIsPaused = IsPaused();
					return bIsPaused ? LOCTEXT("PlayButton", "Play") : LOCTEXT("PauseButton", "Pause");
				})
				.ButtonStyle( FAppStyle::Get(), "Animation.PlayControlsButton" )
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SLiveLinkHubPlaybackWidget::GetPlayForwardIcon)
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnNextFramePressed)
				.ToolTipText(LOCTEXT("ToNext", "To Next"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Forward_Step"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnLastFramePressed)
				.ToolTipText(LOCTEXT("ToEnd", "To End"))
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(FAppStyle::Get().GetBrush("Animation.Forward_End"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "Animation.PlayControlsButton")
				.OnClicked(this, &SLiveLinkHubPlaybackWidget::OnLoopPressed)
				.ToolTipText_Raw(this, &SLiveLinkHubPlaybackWidget::GetLoopTooltip)
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Image(this, &SLiveLinkHubPlaybackWidget::GetLoopIcon)
				]
			]
			+SHorizontalBox::Slot()
			.Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetSelectionStartTime)
				.ToolTipText(LOCTEXT("SelectionStartTime", "Selection start time"))
				.OnValueCommitted(this, &SLiveLinkHubPlaybackWidget::OnSelectionStartTimeCommitted)
				.OnValueChanged(this, &SLiveLinkHubPlaybackWidget::SetSelectionStartTime)
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
				.ClearKeyboardFocusOnCommit(true)
				.Delta(this, &SLiveLinkHubPlaybackWidget::GetSpinboxDelta)
				.LinearDeltaSensitivity(25)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetCurrentTime)
				.ToolTipText(LOCTEXT("CurrentTime", "Current playback time"))
				.OnValueCommitted(this, &SLiveLinkHubPlaybackWidget::OnCurrentTimeCommitted)
				.OnValueChanged(this, &SLiveLinkHubPlaybackWidget::SetCurrentTime)
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
				.ClearKeyboardFocusOnCommit(true)
				.Delta(this, &SLiveLinkHubPlaybackWidget::GetSpinboxDelta)
				.LinearDeltaSensitivity(25)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetSelectionEndTime)
				.ToolTipText(LOCTEXT("SelectionEndTime", "Selection end time"))
				.OnValueCommitted(this, &SLiveLinkHubPlaybackWidget::OnSelectionEndTimeCommitted)
				.OnValueChanged(this, &SLiveLinkHubPlaybackWidget::SetSelectionEndTime)
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
				.ClearKeyboardFocusOnCommit(true)
				.Delta(this, &SLiveLinkHubPlaybackWidget::GetSpinboxDelta)
				.LinearDeltaSensitivity(25)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			[
				SNew(SSpinBox<double>)
				.Value(this, &SLiveLinkHubPlaybackWidget::GetTotalLength)
				.IsEnabled(false)
				.ToolTipText(LOCTEXT("RecordingLength", "Recording length"))
				.MinValue(TOptional<double>())
				.MaxValue(TOptional<double>())
				.Style(&FAppStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
				.TypeInterface(NumberInterface)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 0.f)
			.AutoWidth()
			[
				SNew(SComboButton)
				.HasDownArrow(true)
				.OnGetMenuContent(this, &SLiveLinkHubPlaybackWidget::MakePlaybackSettingsDropdown)
				.ForegroundColor(FSlateColor::UseStyle())
				.ToolTipText(LOCTEXT("PlaybackSettings_Tooltip", "Change playback settings."))
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			]
		]
	];
}

FReply SLiveLinkHubPlaybackWidget::OnPlayForwardPressed()
{
	OnPlayForwardDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnPlayReversePressed()
{
	OnPlayReverseDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnFirstFramePressed()
{
	OnFirstFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnLastFramePressed()
{
	OnLastFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnPreviousFramePressed()
{
	OnPreviousFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnNextFramePressed()
{
	OnNextFrameDelegate.ExecuteIfBound();
	return FReply::Handled();
}

FReply SLiveLinkHubPlaybackWidget::OnLoopPressed()
{
	check(OnSetLoopingDelegate.IsBound() && OnGetLoopingDelegate.IsBound());
	OnSetLoopingDelegate.Execute(!OnGetLoopingDelegate.Execute());
	return FReply::Handled();
}

void SLiveLinkHubPlaybackWidget::SetCurrentTime(double InTime)
{
	check(OnSetCurrentTimeDelegate.IsBound());

	const FQualifiedFrameTime FrameTime = SecondsToFrameTime(InTime);

	OnSetCurrentTimeDelegate.Execute(FrameTime);
}

void SLiveLinkHubPlaybackWidget::OnCurrentTimeCommitted(double InTime, ETextCommit::Type InTextCommit)
{
	SetCurrentTime(InTime);
}

double SLiveLinkHubPlaybackWidget::GetCurrentTime() const
{
	check(OnGetCurrentTimeDelegate.IsBound());
	const FQualifiedFrameTime FrameTime = OnGetCurrentTimeDelegate.Execute();
		
	return FrameTime.Time.AsDecimal();
}

double SLiveLinkHubPlaybackWidget::GetTotalLength() const
{
	check(OnGetTotalLengthDelegate.IsBound());
	const FQualifiedFrameTime FrameTime = OnGetTotalLengthDelegate.Execute();
	return FrameTime.Time.AsDecimal();
}

double SLiveLinkHubPlaybackWidget::GetSelectionStartTime() const
{
	check(OnGetSelectionStartTimeDelegate.IsBound());
	const FQualifiedFrameTime FrameTime = OnGetSelectionStartTimeDelegate.Execute();
	return FrameTime.Time.AsDecimal();
}

void SLiveLinkHubPlaybackWidget::OnSelectionStartTimeCommitted(double InTime, ETextCommit::Type InTextCommit)
{
	SetSelectionStartTime(InTime);
}

void SLiveLinkHubPlaybackWidget::SetSelectionStartTime(double InTime)
{
	check(OnSetSelectionStartTimeDelegate.IsBound());
	const FQualifiedFrameTime FrameTime = SecondsToFrameTime(InTime);
	OnSetSelectionStartTimeDelegate.Execute(FrameTime);
}

double SLiveLinkHubPlaybackWidget::GetSelectionEndTime() const
{
	check(OnGetSelectionEndTimeDelegate.IsBound());
	const FQualifiedFrameTime FrameTime = OnGetSelectionEndTimeDelegate.Execute();

	return FrameTime.Time.AsDecimal();
}

void SLiveLinkHubPlaybackWidget::OnSelectionEndTimeCommitted(double InTime, ETextCommit::Type InTextCommit)
{
	SetSelectionEndTime(InTime);
}

void SLiveLinkHubPlaybackWidget::SetSelectionEndTime(double InTime)
{
	check(OnSetSelectionEndTimeDelegate.IsBound());
	const FQualifiedFrameTime FrameTime = SecondsToFrameTime(InTime);
	OnSetSelectionEndTimeDelegate.Execute(FrameTime);
}

TRange<double> SLiveLinkHubPlaybackWidget::GetViewRange() const
{
	check(OnGetViewRangeDelegate.IsBound());
	return OnGetViewRangeDelegate.Execute();
}

void SLiveLinkHubPlaybackWidget::SetViewRange(TRange<double> InRange)
{
	check(OnSetViewRangeDelegate.IsBound());
	OnSetViewRangeDelegate.Execute(InRange);
}

TRange<double> SLiveLinkHubPlaybackWidget::GetClampRange() const
{
	const FFrameRate FrameRate = GetFrameRate();
	const double Start = GetSelectionStartTime() / FrameRate.Numerator;
	const double End = GetSelectionEndTime() / FrameRate.Numerator;
	return TRange<double>(Start, End);
}

bool SLiveLinkHubPlaybackWidget::IsPaused() const
{
	return OnGetPausedDelegate.IsBound() ? OnGetPausedDelegate.Execute() : false;
}

bool SLiveLinkHubPlaybackWidget::IsPlayingInReverse() const
{
	return OnGetIsInReverseDelegate.IsBound() ? OnGetIsInReverseDelegate.Execute() : false;
}

double SLiveLinkHubPlaybackWidget::GetSpinboxDelta() const
{
	return GetFrameRate().AsDecimal() * GetFrameRate().AsInterval();
}

EFrameNumberDisplayFormats SLiveLinkHubPlaybackWidget::GetDisplayFormat() const
{
	return DisplayFormat;
}

void SLiveLinkHubPlaybackWidget::SetDisplayFormat(EFrameNumberDisplayFormats InDisplayFormat)
{
	DisplayFormat = InDisplayFormat;
	NumberInterface->DisplayFormatChanged();
}

bool SLiveLinkHubPlaybackWidget::CompareDisplayFormat(EFrameNumberDisplayFormats InDisplayFormat) const
{
	return DisplayFormat == InDisplayFormat;
}

FText SLiveLinkHubPlaybackWidget::GetDisplayFormatAsText() const
{
	return DisplayFormat == EFrameNumberDisplayFormats::Frames ? LOCTEXT("DisplayFormat_TimeFrames", "Frames") :
		DisplayFormat == EFrameNumberDisplayFormats::Seconds ? LOCTEXT("DisplayFormat_TimeSeconds", "Seconds") :
		LOCTEXT("DisplayFormat_Timecode", "Timecode") ;
}

FFrameRate SLiveLinkHubPlaybackWidget::GetFrameRate() const
{
	check(OnGetFrameRate.IsBound());
	return OnGetFrameRate.Execute();
}

FQualifiedFrameTime SLiveLinkHubPlaybackWidget::SecondsToFrameTime(double InTime) const
{
	return FQualifiedFrameTime(FFrameTime::FromDecimal(InTime), GetFrameRate());
}

const FSlateBrush* SLiveLinkHubPlaybackWidget::GetPlayForwardIcon() const
{
	return IsPaused() || IsPlayingInReverse() ? FAppStyle::Get().GetBrush("Animation.Forward") : FAppStyle::Get().GetBrush("Animation.Pause");
}

const FSlateBrush* SLiveLinkHubPlaybackWidget::GetPlayReverseIcon() const
{
	return IsPaused() || !IsPlayingInReverse() ? FAppStyle::Get().GetBrush("Animation.Backward") : FAppStyle::Get().GetBrush("Animation.Pause");
}

const FSlateBrush* SLiveLinkHubPlaybackWidget::GetLoopIcon() const
{
	check(OnGetLoopingDelegate.IsBound());
	const bool bLooping = OnGetLoopingDelegate.Execute();
	return bLooping ? FAppStyle::Get().GetBrush("Animation.Loop.Enabled")
		: FAppStyle::Get().GetBrush("Animation.Loop.Disabled");
}

FText SLiveLinkHubPlaybackWidget::GetLoopTooltip() const
{
	check(OnGetLoopingDelegate.IsBound());
	const bool bLooping = OnGetLoopingDelegate.Execute();

	return bLooping ? LOCTEXT("Loop", "Loop") : LOCTEXT("NoLoop", "No looping");
}

TSharedRef<SWidget> SLiveLinkHubPlaybackWidget::MakePlaybackSettingsDropdown()
{
	constexpr bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, nullptr);

	const FCanExecuteAction AlwaysExecute = FCanExecuteAction::CreateLambda([]{ return true; });

	MenuBuilder.BeginSection("ShowTimeAsSection", LOCTEXT("ShowTimeAs", "Show Time As"));
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_TimecodeLabel", "Timecode"),
		LOCTEXT("Menu_TimecodeTooltip", "Display values in timecode format."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SLiveLinkHubPlaybackWidget::SetDisplayFormat, EFrameNumberDisplayFormats::NonDropFrameTimecode),
			AlwaysExecute,
			FIsActionChecked::CreateSP(this, &SLiveLinkHubPlaybackWidget::CompareDisplayFormat, EFrameNumberDisplayFormats::NonDropFrameTimecode)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_FramesLabel", "Frames"),
		LOCTEXT("Menu_FramesTooltip", "Display values as frame numbers."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SLiveLinkHubPlaybackWidget::SetDisplayFormat, EFrameNumberDisplayFormats::Frames),
			AlwaysExecute,
			FIsActionChecked::CreateSP(this, &SLiveLinkHubPlaybackWidget::CompareDisplayFormat, EFrameNumberDisplayFormats::Frames)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Menu_SecondsLabel", "Seconds"),
		LOCTEXT("Menu_SecondsTooltip", "Display values in seconds."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SLiveLinkHubPlaybackWidget::SetDisplayFormat, EFrameNumberDisplayFormats::Seconds),
			AlwaysExecute,
			FIsActionChecked::CreateSP(this, &SLiveLinkHubPlaybackWidget::CompareDisplayFormat, EFrameNumberDisplayFormats::Seconds)
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
	
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
