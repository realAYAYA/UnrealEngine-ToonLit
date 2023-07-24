// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimCurvesTrack.h"
#include "SCurveTimelineView.h"
#include "IRewindDebugger.h"
#include "GameplayProvider.h"
#include "AnimationProvider.h"
#include "SAnimationCurvesView.h"

#define LOCTEXT_NAMESPACE "AnimCurvesTrack"

namespace RewindDebugger
{

FAnimCurvesTrack::FAnimCurvesTrack(uint64 InObjectId) : ObjectId(InObjectId)
{
	Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");
}

TSharedPtr<SWidget> FAnimCurvesTrack::GetDetailsViewInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	return SNew(SAnimationCurvesView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
				.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
	
}

void FAnimCurvesTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FAnimCurveTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FAnimCurvesTrack::UpdateInternal()
{
	TArray<uint32> UniqueTrackIds;

	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();

	bool bChanged = false;
	
	// convert time range to from rewind debugger times to profiler times
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	// count number of unique animations in the current time range
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	if(GameplayProvider && AnimationProvider)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		UniqueTrackIds.SetNum(0, false);

		AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [&UniqueTrackIds,AnimationProvider, StartTime, EndTime](const FAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bHasCurves)
		{
			// this isn't very efficient, and it gets called every frame.  will need optimizing
			InTimeline.EnumerateEvents(StartTime, EndTime, [&UniqueTrackIds, StartTime, EndTime, AnimationProvider](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InMessage)
			{
				if (InEndTime > StartTime && InStartTime < EndTime)
				{
					AnimationProvider->EnumerateSkeletalMeshCurves(InMessage, [&UniqueTrackIds](const FSkeletalMeshNamedCurve& InCurve)
					{
						UniqueTrackIds.AddUnique(InCurve.Id);
					});
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		UniqueTrackIds.StableSort();
		
		const int32 CurveCount = UniqueTrackIds.Num();

		if (Children.Num() != UniqueTrackIds.Num())
			bChanged = true;
		
		Children.SetNum(CurveCount);
		for(int i = 0; i < CurveCount; i++)
		{
			if (!Children[i].IsValid() || Children[i].Get()->GetCurveId() != UniqueTrackIds[i])
			{
				Children[i] = MakeShared<FAnimCurveTrack>(ObjectId, UniqueTrackIds[i]);
				bChanged = true;
			}

			bChanged = bChanged || Children[i]->Update();
		}
	}

	return bChanged;
}


FAnimCurveTrack::FAnimCurveTrack(uint64 InObjectId, uint32 InCurveId) :
	CurveId(InCurveId),
	ObjectId(InObjectId)
{
	Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");
}

TSharedPtr<SCurveTimelineView::FTimelineCurveData> FAnimCurveTrack::GetCurveData() const
{
	if (!CurveData.IsValid())
	{
		CurveData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
	}
	
	CurvesUpdateRequested++;
	
	return CurveData;
}

bool FAnimCurveTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	// compute curve points
	//
	// convert time range to from rewind debugger times to profiler times
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	
	if(CurvesUpdateRequested > 10 && GameplayProvider && AnimationProvider)
	{
		CurvesUpdateRequested = 0;
		auto& CurvePoints = CurveData->Points;
		CurvePoints.SetNum(0,false);
	
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [this, AnimationProvider, StartTime, EndTime, AnalysisSession, &CurvePoints](const FAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bHasCurves)
		{
			// this isn't very efficient, and it gets called every frame.  will need optimizing
			InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, AnimationProvider, AnalysisSession, &CurvePoints](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InMessage)
			{
				if (InEndTime > StartTime && InStartTime < EndTime)
				{
					double Time = InMessage.RecordingTime;
					
					AnimationProvider->EnumerateSkeletalMeshCurves(InMessage, [this, Time, &CurvePoints](const FSkeletalMeshNamedCurve& InCurve)
					{
						if (InCurve.Id == CurveId)
						{
							CurvePoints.Add({Time,InCurve.Value});
						}
					});
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});
	}
		
	bool bChanged = false;

	if (CurveName.IsEmpty() && AnimationProvider)
	{
		CurveName = FText::FromString(AnimationProvider->GetName(CurveId));
		bChanged = true;
	}

	return bChanged;
}

TSharedPtr<SWidget> FAnimCurveTrack::GetDetailsViewInternal() 
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	TSharedPtr<SAnimationCurvesView> AnimationCurvesView = SNew(SAnimationCurvesView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
				.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
	AnimationCurvesView->SetCurveFilter(CurveId);
	return AnimationCurvesView;
}

TSharedPtr<SWidget> FAnimCurveTrack::GetTimelineViewInternal()
{
	FLinearColor CurveColor(0.5,0.5,0.5);
	
	return SNew(SCurveTimelineView)
		.CurveColor(CurveColor)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.RenderFill(false)
		.CurveData_Raw(this, &FAnimCurveTrack::GetCurveData);
}


static const FName AnimationCurvesName("AnimationCurves");

FName FAnimationCurvesTrackCreator::GetTargetTypeNameInternal() const
{
	static const FName SkelMeshName("SkeletalMeshComponent");
	return SkelMeshName;
}
	
FName FAnimationCurvesTrackCreator::GetNameInternal() const
{
	return AnimationCurvesName;
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FAnimationCurvesTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
	return MakeShared<RewindDebugger::FAnimCurvesTrack>(ObjectId);
}

bool FAnimationCurvesTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName))
	{
		AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [&bHasData](const FAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bInHasCurves)
		{
			bHasData = bInHasCurves;
		});
	}
	return bHasData;
}

	
}

#undef LOCTEXT_NAMESPACE