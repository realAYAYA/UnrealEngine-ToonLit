// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/EventSection.h"
#include "MovieSceneEventUtils.h"
#include "K2Node_CustomEvent.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "ISectionLayoutBuilder.h"
#include "Styling/AppStyle.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "MovieSceneTrack.h"
#include "SequencerTimeSliderController.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MovieSceneSequence.h"
#include "EditorFontGlyphs.h"
#include "ScopedTransaction.h"
#include "TimeToPixel.h"

#define LOCTEXT_NAMESPACE "EventSection"

bool FEventSectionBase::IsSectionSelected() const
{
	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();

	TArray<UMovieSceneTrack*> SelectedTracks;
	SequencerPtr->GetSelectedTracks(SelectedTracks);

	UMovieSceneSection* Section = WeakSection.Get();
	UMovieSceneTrack*   Track   = Section ? CastChecked<UMovieSceneTrack>(Section->GetOuter()) : nullptr;
	return Track && SelectedTracks.Contains(Track);
}

void FEventSectionBase::PaintEventName(FSequencerSectionPainter& Painter, int32 LayerId, const FString& InEventString, float PixelPos, bool bIsEventValid) const
{
	static const int32   FontSize      = 10;
	static const float   BoxOffsetPx   = 10.f;
	static const TCHAR*  WarningString = TEXT("\xf071");

	const FSlateFontInfo FontAwesomeFont = FAppStyle::Get().GetFontStyle("FontAwesome.10");
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const FLinearColor   DrawColor       = FAppStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

	TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Setup the warning size. Static since it won't ever change
	static FVector2D WarningSize    = FontMeasureService->Measure(WarningString, FontAwesomeFont);
	const  FMargin   WarningPadding = (bIsEventValid || InEventString.Len() == 0) ? FMargin(0.f) : FMargin(0.f, 0.f, 4.f, 0.f);
	const  FMargin   BoxPadding     = FMargin(4.0f, 2.0f);

	const FVector2D  TextSize       = FontMeasureService->Measure(InEventString, SmallLayoutFont);
	const FVector2D  IconSize       = bIsEventValid ? FVector2D::ZeroVector : WarningSize;
	const FVector2D  PaddedIconSize = IconSize + WarningPadding.GetDesiredSize();
	const FVector2D  BoxSize        = FVector2D(TextSize.X + PaddedIconSize.X, FMath::Max(TextSize.Y, PaddedIconSize.Y )) + BoxPadding.GetDesiredSize();

	// Flip the text position if getting near the end of the view range
	bool  bDrawLeft    = (Painter.SectionGeometry.Size.X - PixelPos) < (BoxSize.X + 22.f) - BoxOffsetPx;
	float BoxPositionX = bDrawLeft ? PixelPos - BoxSize.X - BoxOffsetPx : PixelPos + BoxOffsetPx;
	if (BoxPositionX < 0.f)
	{
		BoxPositionX = 0.f;
	}

	FVector2D BoxOffset  = FVector2D(BoxPositionX,                    Painter.SectionGeometry.Size.Y*.5f - BoxSize.Y*.5f);
	FVector2D IconOffset = FVector2D(BoxPadding.Left,                 BoxSize.Y*.5f - IconSize.Y*.5f);
	FVector2D TextOffset = FVector2D(IconOffset.X + PaddedIconSize.X, BoxSize.Y*.5f - TextSize.Y*.5f);

	// Draw the background box
	FSlateDrawElement::MakeBox(
		Painter.DrawElements,
		LayerId + 1,
		Painter.SectionGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxOffset)),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.5f)
	);

	if (!bIsEventValid)
	{
		// Draw a warning icon for unbound repeaters
		FSlateDrawElement::MakeText(
			Painter.DrawElements,
			LayerId + 2,
			Painter.SectionGeometry.ToPaintGeometry(IconSize, FSlateLayoutTransform(BoxOffset + IconOffset)),
			WarningString,
			FontAwesomeFont,
			Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
			FAppStyle::GetWidgetStyle<FTextBlockStyle>("Log.Warning").ColorAndOpacity.GetSpecifiedColor()
		);
	}

	FSlateDrawElement::MakeText(
		Painter.DrawElements,
		LayerId + 2,
		Painter.SectionGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(BoxOffset + TextOffset)),
		InEventString,
		SmallLayoutFont,
		Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
		DrawColor
	);
}

int32 FEventSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();
	UMovieSceneEventSection* EventSection = Cast<UMovieSceneEventSection>( WeakSection.Get() );
	if (!EventSection || !IsSectionSelected())
	{
		return LayerId;
	}

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	for (int32 KeyIndex = 0; KeyIndex < EventSection->GetEventData().GetKeyTimes().Num(); ++KeyIndex)
	{
		FFrameNumber EventTime = EventSection->GetEventData().GetKeyTimes()[KeyIndex];
		FEventPayload EventData = EventSection->GetEventData().GetKeyValues()[KeyIndex];

		if (EventSection->GetRange().Contains(EventTime))
		{
			FString EventString = EventData.EventName.ToString();
			if (!EventString.IsEmpty())
			{
				const float PixelPos = TimeToPixelConverter.FrameToPixel(EventTime);
				PaintEventName(Painter, LayerId, EventString, PixelPos);
			}
		}
	}

	return LayerId + 3;
}

int32 FEventTriggerSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	UMovieSceneEventTriggerSection* EventTriggerSection = Cast<UMovieSceneEventTriggerSection>(WeakSection.Get());
	if (!EventTriggerSection || !IsSectionSelected())
	{
		return LayerId;
	}

	UMovieSceneSequence* Sequence = EventTriggerSection->GetTypedOuter<UMovieSceneSequence>();
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	UBlueprint* SequenceDirectorBP = SequenceEditor ? SequenceEditor->FindDirectorBlueprint(Sequence) : nullptr;

	// If we do not have a sequence director BP we can't possibly be bound to anything
	if (!SequenceDirectorBP)
	{
		return LayerId;
	}

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	TArrayView<const FFrameNumber> Times  = EventTriggerSection->EventChannel.GetData().GetTimes();
	TArrayView<FMovieSceneEvent>   Events = EventTriggerSection->EventChannel.GetData().GetValues();

	TRange<FFrameNumber> EventSectionRange = EventTriggerSection->GetRange();
	for (int32 KeyIndex = 0; KeyIndex < Times.Num(); ++KeyIndex)
	{
		FFrameNumber EventTime = Times[KeyIndex];
		if (EventSectionRange.Contains(EventTime))
		{
			UK2Node* EndpointNode = FMovieSceneEventUtils::FindEndpoint(&Events[KeyIndex], EventTriggerSection, SequenceDirectorBP);

			FString EventString = EndpointNode ? EndpointNode->GetNodeTitle(ENodeTitleType::MenuTitle).ToString() : FString();
			bool bIsEventValid = true;

			const float PixelPos = TimeToPixelConverter.FrameToPixel(EventTime);
			PaintEventName(Painter, LayerId, EventString, PixelPos, bIsEventValid);
		}
	}

	return LayerId + 3;
}

FReply FEventTriggerSection::OnKeyDoubleClicked(const TArray<FKeyHandle>& KeyHandles)
{
	UMovieSceneEventTriggerSection* EventTriggerSection = Cast<UMovieSceneEventTriggerSection>( WeakSection.Get() );
	if (!EventTriggerSection)
	{
		return FReply::Handled();
	}

	UMovieSceneSequence* Sequence = EventTriggerSection->GetTypedOuter<UMovieSceneSequence>();
	check(Sequence);

	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	if (!SequenceEditor)
	{
		return FReply::Handled();
	}

	UBlueprint* SequenceDirectorBP = SequenceEditor->GetOrCreateDirectorBlueprint(Sequence);
	if (!SequenceDirectorBP)
	{
		return FReply::Handled();
	}

	TMovieSceneChannelData<FMovieSceneEvent> ChannelData = EventTriggerSection->EventChannel.GetData();
	for (FKeyHandle KeyHandle : KeyHandles)
	{
		const int32 EventIndex = ChannelData.GetIndex(KeyHandle);
		if (EventIndex == INDEX_NONE)
		{
			continue;
		}

		FMovieSceneEvent* EventEntryPoint = &ChannelData.GetValues()[EventIndex];
		UK2Node* Endpoint = FMovieSceneEventUtils::FindEndpoint(EventEntryPoint, EventTriggerSection, SequenceDirectorBP);

		if (!Endpoint)
		{
			FScopedTransaction Transaction(LOCTEXT("CreateEventEndpoint", "Create Event Endpoint"));
			Endpoint = FMovieSceneEventUtils::BindNewUserFacingEvent(EventEntryPoint, EventTriggerSection, SequenceDirectorBP);
		}

		if (Endpoint)
		{
			FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Endpoint, false);
			return FReply::Handled();
		}
	}

	return FReply::Handled();
}

int32 FEventRepeaterSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	UMovieSceneEventRepeaterSection* EventRepeaterSection = Cast<UMovieSceneEventRepeaterSection>(WeakSection.Get());
	if (!EventRepeaterSection)
	{
		return LayerId;
	}

	UMovieSceneSequence* Sequence = EventRepeaterSection->GetTypedOuter<UMovieSceneSequence>();
	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	UBlueprint* SequenceDirectorBP = SequenceEditor ? SequenceEditor->FindDirectorBlueprint(Sequence) : nullptr;

	// If we do not have a sequence director BP we can't possibly be bound to anything
	if (!SequenceDirectorBP)
	{
		return LayerId;
	}

	UK2Node* EndpointNode = FMovieSceneEventUtils::FindEndpoint(&EventRepeaterSection->Event, EventRepeaterSection, SequenceDirectorBP);

	float TextOffsetX = EventRepeaterSection->GetRange().GetLowerBound().IsClosed() ? FMath::Max(0.f, Painter.GetTimeConverter().FrameToPixel(EventRepeaterSection->GetRange().GetLowerBoundValue())) : 0.f;

	FString EventString = EndpointNode ? EndpointNode->GetNodeTitle(ENodeTitleType::MenuTitle).ToString() : FString();
	bool bIsEventValid = true;
	PaintEventName(Painter, LayerId, EventString, TextOffsetX, bIsEventValid);

	return LayerId + 1;
}

FReply FEventRepeaterSection::OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();
	if (!SequencerPtr)
	{
		return FReply::Handled();
	}

	UMovieSceneEventRepeaterSection* EventRepeaterSection = Cast<UMovieSceneEventRepeaterSection>(WeakSection.Get());
	if (!EventRepeaterSection)
	{
		return FReply::Handled();
	}

	UMovieSceneSequence* Sequence = EventRepeaterSection->GetTypedOuter<UMovieSceneSequence>();
	check(Sequence);

	FMovieSceneSequenceEditor* SequenceEditor = FMovieSceneSequenceEditor::Find(Sequence);
	if (!SequenceEditor)
	{
		return FReply::Handled();
	}

	UBlueprint* SequenceDirectorBP = SequenceEditor->GetOrCreateDirectorBlueprint(Sequence);
	if (!SequenceDirectorBP)
	{
		return FReply::Handled();
	}

	UK2Node* Endpoint = FMovieSceneEventUtils::FindEndpoint(&EventRepeaterSection->Event, EventRepeaterSection, SequenceDirectorBP);

	if (!Endpoint)
	{
		FScopedTransaction Transaction(LOCTEXT("BindRepeaterEvent", "Create Event Endpoint"));
		Endpoint = FMovieSceneEventUtils::BindNewUserFacingEvent(&EventRepeaterSection->Event, EventRepeaterSection, SequenceDirectorBP);
	}

	if (Endpoint)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Endpoint, false);
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE // "EventSection"