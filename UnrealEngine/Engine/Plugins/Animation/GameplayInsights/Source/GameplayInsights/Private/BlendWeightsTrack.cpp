// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlendWeightsTrack.h"
#include "SCurveTimelineView.h"
#include "IRewindDebugger.h"
#include "GameplayProvider.h"
#include "AnimationProvider.h"
#include "Styling/SlateIconFinder.h"
#include "SBlendWeightsView.h"

#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

#define LOCTEXT_NAMESPACE "BlendWeightsTrack"

namespace RewindDebugger
{

FBlendWeightsTrack::FBlendWeightsTrack(uint64 InObjectId) : ObjectId(InObjectId)
{
#if WITH_EDITOR
	Icon = FSlateIconFinder::FindIconForClass(UAnimSequence::StaticClass());
#endif
}

void FBlendWeightsTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FBlendWeightTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FBlendWeightsTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();

	struct TrackId
	{
		TrackId() : AssetId(0), NodeId(0) {}
		TrackId(uint64 InAssetId, uint32 InNodeId) : AssetId(InAssetId), NodeId(InNodeId) { }
		bool operator ==(const TrackId& Other) const { return AssetId == Other.AssetId && NodeId == Other.NodeId; }
		
		uint64 AssetId;
		uint32 NodeId;
	};	

	TArray<TrackId> UniqueTrackIds;
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();

	bool bChanged = false;
	
	// convert time range to from rewind debugger times to profiler times
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	// count number of unique animations in the current time range
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	int AnimationCount = 0;

	if(GameplayProvider && AnimationProvider)
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		UniqueTrackIds.SetNum(0, false);

		AnimationProvider->ReadTickRecordTimeline(ObjectId, [&UniqueTrackIds,&GameplayProvider, StartTime, EndTime](const FAnimationProvider::TickRecordTimeline& InTimeline)
		{
			// this isn't very efficient, and it gets called every frame.  will need optimizing
			InTimeline.EnumerateEvents(StartTime, EndTime, [&UniqueTrackIds, StartTime, EndTime](double InStartTime, double InEndTime, uint32 InDepth, const FTickRecordMessage& InMessage)
			{
				if (InEndTime > StartTime && InStartTime < EndTime)
				{
					UniqueTrackIds.AddUnique(TrackId(InMessage.AssetId, InMessage.NodeId));
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		UniqueTrackIds.StableSort([](const TrackId &A, const TrackId &B)  { return A.NodeId > B.NodeId; });
		
		AnimationCount = UniqueTrackIds.Num();

		if (Children.Num()!=AnimationCount)
			bChanged = true;
		
		Children.SetNum(AnimationCount);
		for(int i = 0; i < AnimationCount; i++)
		{
			if (!Children[i].IsValid() || Children[i].Get()->GetAssetId() != UniqueTrackIds[i].AssetId)
			{
				Children[i] = MakeShared<FBlendWeightTrack>(ObjectId, UniqueTrackIds[i].AssetId, UniqueTrackIds[i].NodeId);
				bChanged = true;
			}

			bChanged = bChanged || Children[i]->Update();
		}
	}

	return bChanged;
}

TSharedPtr<SWidget> FBlendWeightsTrack::GetDetailsViewInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	return SNew(SBlendWeightsView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
				.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
}


FBlendWeightTrack::FBlendWeightTrack(uint64 InObjectId, uint64 InAssetId, uint32 InNodeId, FBlendWeightTrack::ECurveType InCurveType) :
	 AssetId(InAssetId)
	, NodeId(InNodeId)
	, CurveType(InCurveType)
	, ObjectId(InObjectId)
{
	SetIsExpanded(false);
}


void FBlendWeightTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for (TSharedPtr<FBlendWeightTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

TSharedPtr<SCurveTimelineView::FTimelineCurveData> FBlendWeightTrack::GetCurveData() const
{
	if (!CurveData.IsValid())
	{
		CurveData = MakeShared<SCurveTimelineView::FTimelineCurveData>();
	}
	
	CurvesUpdateRequested++;
	
	return CurveData;
}

bool FBlendWeightTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	// compute curve points
	//
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();

	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);
	const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName);

	int AnimationCount = 0;

	if(CurvesUpdateRequested > 10 && GameplayProvider && AnimationProvider)
	{
		auto& CurvePoints = CurveData->Points;
		CurvePoints.SetNum(0,false);
		
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		
		AnimationProvider->ReadTickRecordTimeline(ObjectId, [AnalysisSession, StartTime, EndTime, &CurvePoints, this](const FAnimationProvider::TickRecordTimeline& InTimeline)
		{
			InTimeline.EnumerateEvents(StartTime, EndTime, [&CurvePoints, AnalysisSession, this](double InStartTime, double InEndTime, uint32 InDepth, const FTickRecordMessage& InMessage)
			{
				if (InMessage.AssetId == AssetId && InMessage.NodeId == NodeId)
				{
					float Weight = 0;
					switch (CurveType)
					{
						case ECurveType::BlendWeight:					Weight = InMessage.BlendWeight; break; 
						case ECurveType::PlaybackTime:					Weight = InMessage.PlaybackTime; break; 
						case ECurveType::RootMotionWeight:				Weight = InMessage.RootMotionWeight; break; 
						case ECurveType::PlayRate:						Weight = InMessage.PlayRate; break; 
						case ECurveType::BlendSpacePositionX:			Weight = InMessage.BlendSpacePositionX; break; 
						case ECurveType::BlendSpacePositionY:			Weight = InMessage.BlendSpacePositionY; break; 
						case ECurveType::BlendSpaceFilteredPositionX:	Weight = InMessage.BlendSpaceFilteredPositionX; break; 
						case ECurveType::BlendSpaceFilteredPositionY:	Weight = InMessage.BlendSpaceFilteredPositionY; break; 
					}
					
					CurvePoints.Add({ InMessage.RecordingTime,	Weight });
				}
								
				return TraceServices::EEventEnumerate::Continue;
			});
		});

		CurvesUpdateRequested = 0;
	}

	// update Icon:
	
	bool bChanged = false;

	if (CurveType == ECurveType::BlendWeight)
	{
		// Blend Weight track gets name/icon of animation
		if (CurveName.IsEmpty() && GameplayProvider)
		{
			if (const FObjectInfo* ObjectInfo = GameplayProvider->FindObjectInfo(AssetId))
			{
				CurveName = FText::FromString(ObjectInfo->Name);
				bChanged = true;
				bool bIsBlendSpace = false;

				if (const UClass* FoundClass = GameplayProvider->FindClass(ObjectInfo->ClassId))
				{
					Icon = FSlateIconFinder::FindIconForClass(FoundClass);
#if WITH_EDITOR
					if (FoundClass->IsChildOf(UBlendSpace::StaticClass()))
					{
						bIsBlendSpace = true;	
					}
#endif
				}

				// Blend Weight track gets child tracks for each of the other bits of extra data
				if (Children.Num() == 0)
				{
					int MaxCurve = static_cast<int>(bIsBlendSpace ? ECurveType::BlendSpaceFilteredPositionY : ECurveType::PlayRate);
					for (int i = static_cast<int>(ECurveType::BlendWeight) + 1; i <= MaxCurve; i++)
					{
						Children.Add(MakeShared<FBlendWeightTrack>(ObjectId, AssetId, NodeId, static_cast<ECurveType>(i)));
					}
				}
			}
		}

	}
	else
	{
		// other tracks get the curve name

		if (CurveName.IsEmpty())
		{
			switch (CurveType)
			{
				case ECurveType::BlendWeight:					break;
				case ECurveType::PlaybackTime:					CurveName = LOCTEXT("PlaybackTime", "Playback Time"); break;
				case ECurveType::RootMotionWeight:				CurveName = LOCTEXT("RootMotionWeight", "Root Motion Weight"); break;
				case ECurveType::PlayRate:						CurveName = LOCTEXT("PlaybackRate", "Playback Rate"); break;
				case ECurveType::BlendSpacePositionX:			CurveName = LOCTEXT("BlendSpaceX", "X"); break;
				case ECurveType::BlendSpacePositionY:			CurveName = LOCTEXT("BlendSpaceY", "Y"); break;
				case ECurveType::BlendSpaceFilteredPositionX:	CurveName = LOCTEXT("BlendSpaceXF", "X (Filtered)"); break;
				case ECurveType::BlendSpaceFilteredPositionY:	CurveName = LOCTEXT("BlendSpaceYF", "Y (Filtered)"); break;
			}

			Icon = FSlateIcon("EditorStyle", "AnimGraph.Attribute.Curves.Icon", "AnimGraph.Attribute.Curves.Icon");

			bChanged = true;
		}
	}

	for (auto& Child : Children)
	{
		bChanged |= Child->Update();
	}

	return bChanged;
}

static FLinearColor MakeBlendWeightCurveColor(uint32 InSeed, bool bInLine = false)
{
	FRandomStream Stream(InSeed);
	const uint8 Hue = (uint8)(Stream.FRand() * 255.0f);
	const uint8 SatVal = bInLine ? 196 : 128;
	return FLinearColor::MakeFromHSV8(Hue, SatVal, SatVal);
}

TSharedPtr<SWidget> FBlendWeightTrack::GetTimelineViewInternal()
{
	FLinearColor Color;
	switch(CurveType)
	{
	case ECurveType::BlendWeight:
		Color = MakeBlendWeightCurveColor(CityHash32(reinterpret_cast<char*>(&AssetId), 8));
		Color.A = 0.5f;
		break; 
	case ECurveType::PlaybackTime:
		Color = FLinearColor::MakeFromHSV8(0, 50, 50);
		break;
	case ECurveType::RootMotionWeight:
		Color = FLinearColor::MakeFromHSV8(60, 50, 50);
		break;
	case ECurveType::PlayRate:		
		Color = FLinearColor::MakeFromHSV8(120, 50, 50);
		break;
	case ECurveType::BlendSpacePositionX:
		Color = FLinearColor::MakeFromHSV8(180, 50, 50);
		break;
	case ECurveType::BlendSpacePositionY:
		Color = FLinearColor::MakeFromHSV8(240, 50, 50);
		break;
	case ECurveType::BlendSpaceFilteredPositionX:
		Color = FLinearColor::MakeFromHSV8(180, 50, 80);
		break;
	case ECurveType::BlendSpaceFilteredPositionY:
		Color = FLinearColor::MakeFromHSV8(240, 50, 80);
		break;
	}

	FLinearColor CurveColor = Color;
	CurveColor.R *= 0.5;
	CurveColor.G *= 0.5;
	CurveColor.B *= 0.5;

	TSharedPtr<SCurveTimelineView> CurveTimelineView = SNew(SCurveTimelineView)
		.FillColor(Color)
		.CurveColor(CurveColor)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.RenderFill(CurveType == ECurveType::BlendWeight)
		.CurveData_Raw(this, &FBlendWeightTrack::GetCurveData);

	if (CurveType == ECurveType::BlendWeight)
	{
		CurveTimelineView->SetFixedRange(0, 1);
	}

	return CurveTimelineView;
}

TSharedPtr<SWidget> FBlendWeightTrack::GetDetailsViewInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	TSharedPtr<SBlendWeightsView> BlendWeightsView = SNew(SBlendWeightsView, ObjectId, RewindDebugger->CurrentTraceTime(), *RewindDebugger->GetAnalysisSession())
				.CurrentTime_Lambda([RewindDebugger]{ return RewindDebugger->CurrentTraceTime(); });
	BlendWeightsView->SetAssetFilter(AssetId,NodeId);
	return BlendWeightsView;
}

bool FBlendWeightTrack::HandleDoubleClickInternal()
{
#if WITH_EDITOR
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	if (const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		const FGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<FGameplayProvider>(FGameplayProvider::ProviderName);

		const FObjectInfo& AssetInfo = GameplayProvider->GetObjectInfo(GetAssetId());

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetInfo.PathName);

		return true;
	}
#endif
	return false;
}

FName FBlendWeightsTrackCreator::GetTargetTypeNameInternal() const
{
	static const FName TargetTypeName = "AnimInstance";
	return TargetTypeName;
}

FName FBlendWeightsTrackCreator::GetNameInternal() const
{
	static const FName BlendWeightsName("BlendWeights");
	return BlendWeightsName;
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FBlendWeightsTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
 	return MakeShared<RewindDebugger::FBlendWeightsTrack>(ObjectId);
}

bool FBlendWeightsTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FAnimationProvider* AnimationProvider = AnalysisSession->ReadProvider<FAnimationProvider>(FAnimationProvider::ProviderName))
	{
		AnimationProvider->ReadAnimGraphTimeline(ObjectId, [&bHasData](const FAnimationProvider::AnimGraphTimeline& InGraphTimeline)
		{
			bHasData = true;
		});
	}
	
	return bHasData;
}

}
#undef LOCTEXT_NAMESPACE
