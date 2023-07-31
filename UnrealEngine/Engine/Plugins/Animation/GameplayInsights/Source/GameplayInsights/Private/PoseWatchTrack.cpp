// Copyright Epic Games, Inc. All Rights Reserved.


#if WITH_EDITOR


#include "PoseWatchTrack.h"
#include "SCurveTimelineView.h"
#include "IRewindDebugger.h"
#include "GameplayProvider.h"
#include "AnimationProvider.h"
#include "SNotifiesView.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Engine/PoseWatchRenderData.h"
#include "Engine/PoseWatch.h"
#include "ObjectTrace.h"

#define LOCTEXT_NAMESPACE "PoseWatchesTrack"

namespace RewindDebugger
{

FPoseWatchTrack::FPoseWatchTrack(uint64 InObjectId, const FPoseWatchTrack::FPoseWatchTrackId& InPoseWatchTrackId)
	: ObjectId(InObjectId)
	, PostWatchTrackId(InPoseWatchTrackId)
	, PoseWatchOwner(nullptr)
{
	EnabledSegments = MakeShared<SSegmentedTimelineView::FSegmentData>();
	Icon = UPoseWatchPoseElement::StaticGetIcon();

#if OBJECT_TRACE_ENABLED
	if (UObject* ObjectInstance = FObjectTrace::GetObjectFromId(ObjectId))
	{
		if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(ObjectInstance))
		{
			if (UAnimBlueprintGeneratedClass* InstanceClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass()))
			{
				FAnimBlueprintDebugData& DebugData = InstanceClass->GetAnimBlueprintDebugData();
				for (const FAnimNodePoseWatch& PoseWatch : DebugData.AnimNodePoseWatch)
				{
					if (PoseWatch.NodeID == PostWatchTrackId.NameId)
					{
						PoseWatchOwner = PoseWatch.PoseWatchPoseElement;
						break;
					}
				}
			}
		}
	}
#endif
}

FText FPoseWatchTrack::GetDisplayNameInternal() const
{
	return TrackName;
}

TSharedPtr<SSegmentedTimelineView::FSegmentData> FPoseWatchTrack::GetSegmentData() const
{
	return EnabledSegments;
}

bool FPoseWatchTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	if(GameplayProvider && AnimationProvider && EnabledSegments.IsValid())
	{
		EnabledSegments->Segments.SetNum(0, false);

		struct FPoseWatchEnabledTime
		{
			double RecordingTime;
			bool bIsEnabled;

			bool operator <(const FPoseWatchEnabledTime& Other) const
			{
				return RecordingTime < Other.RecordingTime;
			}
		};

		TArray<FPoseWatchEnabledTime> RecordingTimes;

		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		AnimationProvider->ReadPoseWatchTimeline(ObjectId, [this, StartTime, EndTime, &RecordingTimes](const FAnimationProvider::PoseWatchTimeline& InPoseWatchTimeline)
		{
			InPoseWatchTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, &RecordingTimes](double InStartTime, double InEndTime, uint32 InDepth, const FPoseWatchMessage& InMessage)
			{
				if (InMessage.PoseWatchId == PostWatchTrackId.NameId)
				{
					FPoseWatchEnabledTime EnabledTime;
					EnabledTime.RecordingTime = InMessage.RecordingTime;
					EnabledTime.bIsEnabled = InMessage.bIsEnabled;

					RecordingTimes.Add(EnabledTime);
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		if (!RecordingTimes.IsEmpty())
		{
			RecordingTimes.Sort();
			double RangeStart = 0.0;

			bool bIsEnabled = RecordingTimes[0].bIsEnabled;
			for (int32 Index = 1; Index < RecordingTimes.Num(); ++Index)
			{
				// Going from disabled to enabled
				if (!bIsEnabled && RecordingTimes[Index].bIsEnabled)
				{
					RangeStart = RecordingTimes[Index].RecordingTime;
					bIsEnabled = true;
				}

				// Going from enabled to disabled
				else if (bIsEnabled && !RecordingTimes[Index].bIsEnabled)
				{
					double RangeEnd = RecordingTimes[Index].RecordingTime;
					bIsEnabled = false;
					EnabledSegments->Segments.Add(TRange<double>(RangeStart, RangeEnd));
				}
			}

			if (bIsEnabled)
			{
				// This region hasn't closed yet
				EnabledSegments->Segments.Add(TRange<double>(RangeStart, RecordingTimes.Last().RecordingTime));
			}
		}
	}

	bool bChanged = false;
	if (PoseWatchOwner && PoseWatchOwner->GetParent())
	{
		bChanged = !TrackName.EqualTo(PoseWatchOwner->GetParent()->GetLabel());
		TrackName = PoseWatchOwner->GetParent()->GetLabel();
	}

	return bChanged;
}

TSharedPtr<SWidget> FPoseWatchTrack::GetTimelineViewInternal()
{
	const auto GetPoseWatchColorLambda = [this]() -> FLinearColor { return PoseWatchOwner ? FLinearColor(PoseWatchOwner->GetColor()) : FLinearColor::White; };

	const auto TimelineView = SNew(SSegmentedTimelineView)
		.FillColor_Lambda(GetPoseWatchColorLambda)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.SegmentData_Raw(this, &FPoseWatchTrack::GetSegmentData);

	return TimelineView;
}

FName FPoseWatchesTrackCreator::GetTargetTypeNameInternal() const
{
	static const FName AnimInstanceName("AnimInstance");
	return AnimInstanceName;
}
	
FName FPoseWatchesTrackCreator::GetNameInternal() const
{
	static const FName PoseWatchesName("PoseWatches");
	return PoseWatchesName;
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FPoseWatchesTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
	return MakeShared<RewindDebugger::FPoseWatchesTrack>(ObjectId);
}

		
FPoseWatchesTrack::FPoseWatchesTrack(uint64 InObjectId)
	: ObjectId(InObjectId)
{
	Icon = UPoseWatchPoseElement::StaticGetIcon();

#if OBJECT_TRACE_ENABLED
	if (UObject* ObjectInstance = FObjectTrace::GetObjectFromId(ObjectId))
	{
		if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(ObjectInstance))
		{
			if (UAnimBlueprintGeneratedClass* InstanceClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstance->GetClass()))
			{
				AnimBPGenClass = InstanceClass;
			}
		}
	}
#endif
}


bool FPoseWatchesTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);
	
	bool bChanged = false;
	
	if(GameplayProvider && AnimationProvider)
	{
		TArray<FPoseWatchTrack::FPoseWatchTrackId> UniqueTrackIds;
		
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		AnimationProvider->ReadPoseWatchTimeline(ObjectId, [this, StartTime, EndTime, &UniqueTrackIds](const FAnimationProvider::PoseWatchTimeline& InPoseWatchTimeline)
		{
			InPoseWatchTimeline.EnumerateEvents(StartTime, EndTime, [this, &UniqueTrackIds](double InStartTime, double InEndTime, uint32 InDepth, const FPoseWatchMessage& InMessage)
			{
				// Only add the pose watch track if the pose watch still exists to attach to
				if (AnimBPGenClass)
				{
					for (const FAnimNodePoseWatch& PoseWatch : AnimBPGenClass->GetAnimBlueprintDebugData().AnimNodePoseWatch)
					{
						if (PoseWatch.NodeID == InMessage.PoseWatchId)
						{
							UniqueTrackIds.AddUnique({ InMessage.PoseWatchId });
							break;
						}
					}
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});
		
		UniqueTrackIds.StableSort();
		const int32 TrackCount = UniqueTrackIds.Num();

		if (Children.Num() != TrackCount)
			bChanged = true;

		Children.SetNum(UniqueTrackIds.Num());
		for(int i = 0; i < TrackCount; i++)
		{
			if (!Children[i].IsValid() || !(Children[i].Get()->GetPoseWatchTrackId() == UniqueTrackIds[i]))
			{
				Children[i] = MakeShared<FPoseWatchTrack>(ObjectId, UniqueTrackIds[i]);
				bChanged = true;
			}

			bChanged = bChanged || Children[i]->Update();
		}
	}
	
	return bChanged;
}
	
void FPoseWatchesTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FPoseWatchTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FPoseWatchesTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName))
	{
		AnimationProvider->ReadPoseWatchTimeline(ObjectId, [&bHasData](const FAnimationProvider::PoseWatchTimeline& InTimeline)
		{
			bHasData = true;
		});
	}
	return bHasData;
}

	
}

#undef LOCTEXT_NAMESPACE

#endif