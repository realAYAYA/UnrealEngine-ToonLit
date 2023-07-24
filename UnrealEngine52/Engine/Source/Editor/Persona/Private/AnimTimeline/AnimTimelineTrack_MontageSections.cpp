// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimTimeline/AnimTimelineTrack_MontageSections.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SLeafWidget.h"
#include "Animation/AnimMontage.h"
#include "SCurveEditor.h"
#include "Styling/AppStyle.h"
#include "AnimTimeline/AnimModel_AnimMontage.h"

#define LOCTEXT_NAMESPACE "FAnimTimelineTrack_MontageSections"

ANIMTIMELINE_IMPLEMENT_TRACK(FAnimTimelineTrack_MontageSections);

FAnimTimelineTrack_MontageSections::FAnimTimelineTrack_MontageSections(const TSharedRef<FAnimModel>& InModel)
	: FAnimTimelineTrack(LOCTEXT("TrackTitle", "Sections"), LOCTEXT("TrackTooltip", "Montage sections"), InModel)
{
	SetHeight(32.0f);
}

#undef LOCTEXT_NAMESPACE
