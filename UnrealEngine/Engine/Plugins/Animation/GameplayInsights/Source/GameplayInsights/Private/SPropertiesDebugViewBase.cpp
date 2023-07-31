// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertiesDebugViewBase.h"
#include "AnimationProvider.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "GameplayProvider.h"
#include "Styling/SlateIconFinder.h"
#include "TraceServices/Model/Frames.h"
#include "VariantTreeNode.h"

#define LOCTEXT_NAMESPACE "SPropertiesDebugViewBase"

void SPropertiesDebugViewBase::Construct(const FArguments& InArgs, uint64 InObjectId, double InTimeMarker, const TraceServices::IAnalysisSession& InAnalysisSession)
{
	ObjectId = InObjectId;
	AnalysisSession = &InAnalysisSession;
	CurrentTime = InArgs._CurrentTime;

	View = SNew(SVariantValueView, InAnalysisSession).OnGetVariantValues(this, &SPropertiesDebugViewBase::GetVariantsAtFrame);

	SetTimeMarker(InTimeMarker);

	ChildSlot
	[
		View.ToSharedRef()
	];
}

void SPropertiesDebugViewBase::SetTimeMarker(double Time)
{
	if (TimeMarker != Time)
	{
		TimeMarker = Time;
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		const TraceServices::IFrameProvider& FramesProvider = TraceServices::ReadFrameProvider(*AnalysisSession);
		TraceServices::FFrame MarkerFrame;
		if(FramesProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, TimeMarker, MarkerFrame))
		{
			View->RequestRefresh(MarkerFrame);
		}
	}
}

void SPropertiesDebugViewBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (CurrentTime.IsBound())
	{
		SetTimeMarker(CurrentTime.Get());
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

#undef LOCTEXT_NAMESPACE
