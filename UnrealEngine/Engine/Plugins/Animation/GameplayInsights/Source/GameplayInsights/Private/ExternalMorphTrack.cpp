// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExternalMorphTrack.h"
#include "SCurveTimelineView.h"
#include "IRewindDebugger.h"
#include "GameplayProvider.h"
#include "AnimationProvider.h"
#include "SAnimationCurvesView.h"

#define LOCTEXT_NAMESPACE "ExternalMorphTrack"

namespace RewindDebugger
{
	FExternalMorphSetGroupTrack::FExternalMorphSetGroupTrack(uint64 InObjectId)
		: ObjectId(InObjectId)
	{
		SetIsExpanded(false);
		Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");
	}

	void FExternalMorphSetGroupTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
	{
		for (TSharedPtr<FExternalMorphSetTrack>& Track : Children)
		{
			IteratorFunction(Track);
		}
	};

	bool FExternalMorphSetGroupTrack::UpdateInternal()
	{
		IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();

		// Convert time range to from rewind debugger times to profiler times.
		TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
		double StartTime = TraceTimeRange.GetLowerBoundValue();
		double EndTime = TraceTimeRange.GetUpperBoundValue();
	
		const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
		const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

		TArray<int32> MorphSetIndexValues;

		bool bChanged = false;
		if (GameplayProvider && AnimationProvider)
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

			AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [this, AnimationProvider, StartTime, EndTime, &MorphSetIndexValues](const FAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bHasCurves)
			{
				InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, AnimationProvider, &MorphSetIndexValues](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InMessage)
				{
					if (InEndTime > StartTime && InStartTime < EndTime)
					{
						AnimationProvider->EnumerateExternalMorphSets(InMessage, [this, &MorphSetIndexValues](const FExternalMorphWeightMessage& InSet)
						{
							MorphSetIndexValues.AddUnique(InSet.Index);
						});
					}
					return TraceServices::EEventEnumerate::Continue;
				});
			});

			bChanged = false;
			if (MorphSetIndexValues.Num() != Children.Num() || PrevMorphSetIndexValues != MorphSetIndexValues)
			{
				bChanged = true;
				Children.SetNum(MorphSetIndexValues.Num());
				for (int32 Index = 0; Index < MorphSetIndexValues.Num(); ++Index)
				{
					TSharedPtr<FExternalMorphSetTrack> NewTrack = MakeShared<FExternalMorphSetTrack>(ObjectId, MorphSetIndexValues[Index]);
					Children[Index] = NewTrack;
				}
			}

			for (int32 Index = 0; Index < Children.Num(); ++Index)
			{
				bChanged |= Children[Index]->Update();
			}

			PrevMorphSetIndexValues = MorphSetIndexValues;
		}

		return bChanged;
	}

	//----------------------------------------------------------------------------------------------

	FExternalMorphSetTrack::FExternalMorphSetTrack(uint64 InObjectId, int32 InMorphSetIndex)
		: ObjectId(InObjectId)
		, MorphSetIndex(InMorphSetIndex)
	{
		SetIsExpanded(false);
		Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");
	}

	TSharedPtr<SWidget> FExternalMorphSetTrack::GetDetailsViewInternal()
	{
		IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		return SNew(SAnimationCurvesView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
					.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
	
	}

	void FExternalMorphSetTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
	{
		for (TSharedPtr<FExternalMorphTrack>& Track : Children)
		{
			IteratorFunction(Track);
		}
	};

	bool FExternalMorphSetTrack::UpdateInternal()
	{
		IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();

		// Convert time range to from rewind debugger times to profiler times.
		TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
		double StartTime = TraceTimeRange.GetLowerBoundValue();
		double EndTime = TraceTimeRange.GetUpperBoundValue();
	
		const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
		const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

		int32 NumMorphTracks = 0;
		bool bChanged = false;
		if (GameplayProvider && AnimationProvider)
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

			AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [this, AnimationProvider, StartTime, EndTime, &NumMorphTracks](const FAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bHasCurves)
			{
				InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, AnimationProvider, &NumMorphTracks](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InMessage)
				{
					if (InEndTime > StartTime && InStartTime < EndTime)
					{
						AnimationProvider->EnumerateExternalMorphSets(InMessage, [this, &NumMorphTracks](const FExternalMorphWeightMessage& InSet)
						{
							if (InSet.Index == MorphSetIndex)
							{
								NumMorphTracks = InSet.Weights.Num();
							}
						});
					}
					return TraceServices::EEventEnumerate::Continue;
				});
			});

			bChanged = false;
			if (NumMorphTracks != Children.Num())
			{
				bChanged = true;
				Children.SetNum(NumMorphTracks);
				for (int32 Index = 0; Index < NumMorphTracks; ++Index)
				{
					TSharedPtr<FExternalMorphTrack> NewTrack = MakeShared<FExternalMorphTrack>(ObjectId, Index, MorphSetIndex);
					Children[Index] = NewTrack;
				}
			}

			check(Children.Num() == NumMorphTracks);
			for (int32 Index = 0; Index < NumMorphTracks; ++Index)
			{
				bChanged |= Children[Index]->Update();
			}
		}

		return bChanged;
	}

	//----------------------------------------------------------------------------------------------

	FExternalMorphTrack::FExternalMorphTrack(uint64 InObjectId, int32 InMorphIndex, int32 InMorphSetIndex) :
		MorphIndex(InMorphIndex),
		MorphSetIndex(InMorphSetIndex),
		ObjectId(InObjectId)
	{
		Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");
	}

	TSharedPtr<SCurveTimelineView::FTimelineCurveData> FExternalMorphTrack::GetCurveData() const
	{
		if (!CurveData.IsValid())
		{
			CurveData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
		}
	
		CurvesUpdateRequested++;
		return CurveData;
	}

	bool FExternalMorphTrack::UpdateInternal()
	{
		IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
		const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

		if (CurvesUpdateRequested > 25 && AnimationProvider)
		{
			CurvesUpdateRequested = 0;
			UpdateCurvePointsInternal();
		}

		bool bChanged = false;
		if (Name.IsEmpty())
		{
			Name = FText::Format(LOCTEXT("MorphNameFormat", "Morph Target {0}"), MorphIndex);
			bChanged = true;
		}

		return bChanged;
	}

	void FExternalMorphTrack::UpdateCurvePointsInternal()
	{
		IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
		const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

		// Convert time range to from rewind debugger times to profiler times.
		TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
		double StartTime = TraceTimeRange.GetLowerBoundValue();
		double EndTime = TraceTimeRange.GetUpperBoundValue();
	
		TArray<SCurveTimelineView::FTimelineCurveData::CurvePoint>& CurvePoints = CurveData->Points;
		CurvePoints.Reset();
		CurvePoints.Reserve(256);

		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		AnimationProvider->ReadSkeletalMeshPoseTimeline(ObjectId, [this, AnimationProvider, StartTime, EndTime, AnalysisSession, &CurvePoints](const FAnimationProvider::SkeletalMeshPoseTimeline& InTimeline, bool bHasCurves)
		{
			InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, AnimationProvider, AnalysisSession, &CurvePoints](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& InMessage)
			{
				if (InEndTime > StartTime && InStartTime < EndTime)
				{
					const double Time = InMessage.RecordingTime;		
					AnimationProvider->EnumerateExternalMorphSets(InMessage, [this, Time, &CurvePoints](const FExternalMorphWeightMessage& InWeight)
					{
						if (InWeight.Index == MorphSetIndex)
						{
							CurvePoints.Add({Time, InWeight.Weights[MorphIndex]});
						}
					});
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});
	}

	TSharedPtr<SWidget> FExternalMorphTrack::GetDetailsViewInternal() 
	{
		IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
		TSharedPtr<SAnimationCurvesView> AnimationCurvesView = SNew(SAnimationCurvesView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
			.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
		return AnimationCurvesView;
	}

	TSharedPtr<SWidget> FExternalMorphTrack::GetTimelineViewInternal()
	{
		FLinearColor CurveColor(0.5,0.5,0.5);
	
		return SNew(SCurveTimelineView)
			.CurveColor(CurveColor)
			.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
			.RenderFill(false)
			.CurveData_Raw(this, &FExternalMorphTrack::GetCurveData);
	}

	//----------------------------------------------------------------------------------------------

	FName FExternalMorphSetGroupTrackCreator::GetTargetTypeNameInternal() const
	{
		static const FName SkelMeshName("SkeletalMeshComponent");
		return SkelMeshName;
	}
	
	static const FName ExternalMorphSetName("ExternalMorphSetGroup");
	
	FName FExternalMorphSetGroupTrackCreator::GetNameInternal() const
	{
		return ExternalMorphSetName;
	}
	
	void FExternalMorphSetGroupTrackCreator::GetTrackTypesInternal(TArray<FRewindDebuggerTrackType>& Types) const 
    {
    	Types.Add({ExternalMorphSetName, LOCTEXT("External Morph Sets", "External Morph Sets")});
    }

	TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FExternalMorphSetGroupTrackCreator::CreateTrackInternal(uint64 ObjectId) const
	{
		return MakeShared<RewindDebugger::FExternalMorphSetGroupTrack>(ObjectId);
	}

	bool FExternalMorphSetGroupTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
	{
		const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		if (const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName))
		{
			return AnimationProvider->HasExternalMorphSets(ObjectId);
		}
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
