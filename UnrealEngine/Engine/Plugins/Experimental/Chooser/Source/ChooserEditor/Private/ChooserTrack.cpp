// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserTrack.h"

#include "Chooser.h"
#include "ChooserProvider.h"
#include "Editor.h"
#include "IGameplayProvider.h"
#include "IRewindDebugger.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "ChooserTrack"

namespace UE::ChooserEditor
{

FChooserTrack::FChooserTrack(uint64 InObjectId, uint64 InChooserId) :
	ObjectId(InObjectId),
	ChooserId(InChooserId)
{
	EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
	Icon = FSlateIcon("EditorStyle", "Sequencer.Tracks.Event", "Sequencer.Tracks.Event");
	
	const IGameplayProvider* GameplayProvider = IRewindDebugger::Instance()->GetAnalysisSession()->ReadProvider<IGameplayProvider>("GameplayProvider");
	const FObjectInfo& ChooserInfo = GameplayProvider->GetObjectInfo(InChooserId);
	TrackName = FText::FromString(ChooserInfo.Name);
	ChooserTable = FindObject<UChooserTable>(nullptr, ChooserInfo.PathName);
}

FChooserTrack::~FChooserTrack()
{
}

TSharedPtr<SEventTimelineView::FTimelineEventData> FChooserTrack::GetEventData() const
{
	if (!EventData.IsValid())
	{
		EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
	}
	
	EventUpdateRequested++;
	
	return EventData;
}
	
static FLinearColor MakeNotifyColor(uint32 InSeed, bool bInLine = false)
{
	FRandomStream Stream(InSeed);
	const uint8 Hue = (uint8)(Stream.FRand() * 255.0f);
	const uint8 SatVal = bInLine ? 196 : 128;
	return FLinearColor::MakeFromHSV8(Hue, SatVal, SatVal);
}

TSharedPtr<SWidget> FChooserTrack::GetDetailsViewInternal() 
{
	return nullptr;
}

bool FChooserTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	
	TRange<double> RecordingTimeRange = RewindDebugger->GetCurrentViewRange();
	double StartTime = RecordingTimeRange.GetLowerBoundValue();
	double EndTime = RecordingTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FChooserProvider* ChooserProvider = AnalysisSession->ReadProvider<FChooserProvider>(FChooserProvider::ProviderName);
	
	if(EventUpdateRequested > 10 && ChooserProvider)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FChooserTrack::UpdateEventPointsInternal);
		EventUpdateRequested = 0;
		
		EventData->Points.SetNum(0,EAllowShrinking::No);
		EventData->Windows.SetNum(0);

		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		
		ChooserProvider->ReadChooserEvaluationTimeline(ObjectId, [this, StartTime, EndTime](const FChooserProvider::ChooserEvaluationTimeline& InTimeline)
		{
			InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime](double InStartTime, double InEndTime, uint32 InDepth, const FChooserEvaluationData& ChooserEvaluationData)
			{
				if (ChooserEvaluationData.ChooserId == ChooserId)
				{
					EventData->Points.Add({InStartTime, FText(), FText(), FLinearColor::White});
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});
	}

	EventUpdateRequested++;

	bool bChanged = false;
	return bChanged;
	
}

bool FChooserTrack::HandleDoubleClickInternal()
{
	const IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	if (const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);

		const IGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<IGameplayProvider>("GameplayProvider");
		const FObjectInfo& AssetInfo = GameplayProvider->GetObjectInfo(GetChooserId());

		// attach chooser table editor debugging
		if (ChooserTable)
		{
			const FObjectInfo& OwnerObjectInfo  = GameplayProvider->GetObjectInfo(ObjectId);
			ChooserTable->SetDebugTarget(OwnerObjectInfo.Name);
			ChooserTable->bEnableDebugTesting = true;
		}
		

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetInfo.PathName);

		return true;
	}
	return false;
}

TSharedPtr<SWidget> FChoosersTrack::GetDetailsViewInternal() 
{
	return nullptr;
}

TSharedPtr<SWidget> FChooserTrack::GetTimelineViewInternal()
{
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Raw(this, &FChooserTrack::GetEventData);
}

static const FName ChoosersName("Choosers");

 FName FChoosersTrackCreator::GetTargetTypeNameInternal() const
 {
 	static const FName ObjectName("Object");
	return ObjectName;
}
	
FName FChoosersTrackCreator::GetNameInternal() const
{
	return ChoosersName;
}

void FChoosersTrackCreator::GetTrackTypesInternal(TArray<RewindDebugger::FRewindDebuggerTrackType>& Types) const
{
	Types.Add({ChoosersName, LOCTEXT("Chooser", "Choosers")});
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FChoosersTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
	return MakeShared<FChoosersTrack>(ObjectId);
}

		
FChoosersTrack::FChoosersTrack(uint64 InObjectId) :
	ObjectId(InObjectId)
{
	Icon = FSlateIcon("EditorStyle", "Sequencer.Tracks.Event", "Sequencer.Tracks.Event");
}


bool FChoosersTrack::UpdateInternal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FChoosersTrack::UpdateInternal);
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	
	TRange<double> RecordingTimeRange = RewindDebugger->GetCurrentViewRange();
	double StartTime = RecordingTimeRange.GetLowerBoundValue();
	double EndTime = RecordingTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FChooserProvider* ChooserProvider = AnalysisSession->ReadProvider<FChooserProvider>(FChooserProvider::ProviderName);
	
	bool bChanged = false;
	
	if(ChooserProvider)
	{
		TArray<uint64> UniqueTrackIds;
		
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
		
		ChooserProvider->ReadChooserEvaluationTimeline(ObjectId, [this, StartTime, EndTime, &UniqueTrackIds](const FChooserProvider::ChooserEvaluationTimeline& InTimeline)
		{
			InTimeline.EnumerateEvents(StartTime, EndTime, [this, &UniqueTrackIds](double InStartTime, double InEndTime, uint32 InDepth, const FChooserEvaluationData& ChooserEvaluationData)
			{
				UniqueTrackIds.AddUnique(ChooserEvaluationData.ChooserId);
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
			if (!Children[i].IsValid() || !(Children[i].Get()->GetChooserId() == UniqueTrackIds[i]))
			{
				Children[i] = MakeShared<FChooserTrack>(ObjectId, UniqueTrackIds[i]);
				bChanged = true;
			}
		
			if (Children[i]->Update())
			{
				bChanged = true;
			}
		}
	}
	
	return bChanged;
}

void FChoosersTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FChooserTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FChoosersTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FChoosersTrack::HasDebugInfoInternal);
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FChooserProvider* ChooserProvider = AnalysisSession->ReadProvider<FChooserProvider>(FChooserProvider::ProviderName))
	{
		ChooserProvider->ReadChooserEvaluationTimeline(ObjectId, [&bHasData](const FChooserProvider::ChooserEvaluationTimeline& InTimeline)
	 	{
	 		bHasData = true;
	 	});
	}
	return bHasData;
}

	
}

#undef LOCTEXT_NAMESPACE