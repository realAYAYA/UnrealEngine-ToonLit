// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisualLogTrack.h"
#include "IRewindDebugger.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "VisualLoggerProvider.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "VisualLogTrack"

namespace RewindDebugger
{

UVLogDetailsObject* FVisualLogCategoryTrack::InitializeDetailsObject()
{
	UVLogDetailsObject* DetailsObject = NewObject<UVLogDetailsObject>();
	DetailsObject->SetFlags(RF_Standalone);
	DetailsObjectWeakPtr = MakeWeakObjectPtr(DetailsObject);
	DetailsView->SetObject(DetailsObject);
	return DetailsObject;
}

FVisualLogCategoryTrack::FVisualLogCategoryTrack(uint64 InObjectId, const FName& InCategory) :
	ObjectId(InObjectId),
	Category(InCategory)
{
	TrackName = FText::FromName(Category);
	EventData = MakeShared<SEventTimelineView::FTimelineEventData>();
	Icon = FSlateIcon("EditorStyle", "Sequencer.Tracks.Event", "Sequencer.Tracks.Event");

	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InitializeDetailsObject();
}

FVisualLogCategoryTrack::~FVisualLogCategoryTrack()
{
	if (UVLogDetailsObject* DetailsObject = DetailsObjectWeakPtr.Get())
	{
		DetailsObject->ClearFlags(RF_Standalone);
	}
}

TSharedPtr<SEventTimelineView::FTimelineEventData> FVisualLogCategoryTrack::GetEventData() const
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

TSharedPtr<SWidget> FVisualLogCategoryTrack::GetDetailsViewInternal() 
{
	return DetailsView;
}

bool FVisualLogCategoryTrack::UpdateInternal()
{
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	
	if (const FVisualLoggerProvider* VisLogProvider = AnalysisSession->ReadProvider<FVisualLoggerProvider>(FVisualLoggerProvider::ProviderName))
	{
		if(EventUpdateRequested > 10)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FVisualLogTrack::UpdateEventPointsInternal);
			EventUpdateRequested = 0;
			
			EventData->Points.SetNum(0,EAllowShrinking::No);
			EventData->Windows.SetNum(0);

			TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
			
			VisLogProvider->ReadVisualLogEntryTimeline(ObjectId, [this, StartTime, EndTime, VisLogProvider, AnalysisSession](const FVisualLoggerProvider::VisualLogEntryTimeline& InTimeline)
			{
				InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, VisLogProvider, AnalysisSession](double InStartTime, double InEndTime, uint32 InDepth, const FVisualLogEntry& InMessage)
				{
					for(const FVisualLogShapeElement& Element : InMessage.ElementsToDraw)
					{
						EventData->Points.Add({InMessage.TimeStamp,FText::FromName(Element.Category), FText::FromString(Element.Description),Element.GetFColor()});
					}
					return TraceServices::EEventEnumerate::Continue;
				});
			});
		}


		double CurrentScrubTime = IRewindDebugger::Instance()->CurrentTraceTime();
		if (PreviousScrubTime != CurrentScrubTime)
		{
			PreviousScrubTime = CurrentScrubTime;

			UVLogDetailsObject* DetailsObject = DetailsObjectWeakPtr.Get();
			if (DetailsObject == nullptr)
			{
				// this should not happen unless the object was garbage collected (which should not happen since it's marked as Standalone)
				DetailsObject = InitializeDetailsObject();
			}

			DetailsObject->VisualLogDetails.SetNum(0,EAllowShrinking::No);
			
			const TraceServices::IFrameProvider& FramesProvider = TraceServices::ReadFrameProvider(*AnalysisSession);
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
			
			TraceServices::FFrame MarkerFrame;
			if(FramesProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, CurrentScrubTime, MarkerFrame))
			{
				VisLogProvider->ReadVisualLogEntryTimeline(ObjectId, [this, DetailsObject, &MarkerFrame, EndTime, VisLogProvider, AnalysisSession](const FVisualLoggerProvider::VisualLogEntryTimeline& InTimeline)
				{
					InTimeline.EnumerateEvents(MarkerFrame.StartTime, MarkerFrame.EndTime, [this, DetailsObject, VisLogProvider, AnalysisSession](double InStartTime, double InEndTime, uint32 InDepth, const FVisualLogEntry& InMessage)
					{
						for(const FVisualLogShapeElement& Element : InMessage.ElementsToDraw)
						{
							DetailsObject->VisualLogDetails.Add({Element.Category, Element.Description});
						}
						
						for(const FVisualLogLine& Line : InMessage.LogLines)
						{
							DetailsObject->VisualLogDetails.Add({Line.Category, Line.Line});
						}
						return TraceServices::EEventEnumerate::Continue;
					});
				});
			}
		}
	}
	
	bool bChanged = false;
	return bChanged;
	
}

TSharedPtr<SWidget> FVisualLogTrack::GetDetailsViewInternal() 
{
	return nullptr;
}

TSharedPtr<SWidget> FVisualLogCategoryTrack::GetTimelineViewInternal()
{
	return SNew(SEventTimelineView)
		.ViewRange_Lambda([]() { return IRewindDebugger::Instance()->GetCurrentViewRange(); })
		.EventData_Raw(this, &FVisualLogCategoryTrack::GetEventData);
}

static const FName VisualLogName("Visual Logging");

FName FVisualLogTrackCreator::GetTargetTypeNameInternal() const
{
 	static const FName AnimInstanceName("Object");
	return AnimInstanceName;
}
	
FName FVisualLogTrackCreator::GetNameInternal() const
{
	return VisualLogName;
}
	
void FVisualLogTrackCreator::GetTrackTypesInternal(TArray<FRewindDebuggerTrackType>& Types) const
{
	Types.Add({VisualLogName, LOCTEXT("Visual Logging", "Visual Logging")});
}

TSharedPtr<RewindDebugger::FRewindDebuggerTrack> FVisualLogTrackCreator::CreateTrackInternal(uint64 ObjectId) const
{
	return MakeShared<RewindDebugger::FVisualLogTrack>(ObjectId);
}

		
FVisualLogTrack::FVisualLogTrack(uint64 InObjectId) :
	ObjectId(InObjectId)
{
	Icon = FSlateIcon("EditorStyle", "Sequencer.Tracks.Event", "Sequencer.Tracks.Event");
}


bool FVisualLogTrack::UpdateInternal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVisualLogTrack::UpdateInternal);
	IRewindDebugger* RewindDebugger = IRewindDebugger::Instance();
	
	TRange<double> TraceTimeRange = RewindDebugger->GetCurrentTraceRange();
	double StartTime = TraceTimeRange.GetLowerBoundValue();
	double EndTime = TraceTimeRange.GetUpperBoundValue();
	
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	const FVisualLoggerProvider* VisLogProvider = AnalysisSession->ReadProvider<FVisualLoggerProvider>(FVisualLoggerProvider::ProviderName);
	
	bool bChanged = false;
	
	if(VisLogProvider)
	{
		TArray<FName> UniqueTrackIds;
		
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	
		VisLogProvider->ReadVisualLogEntryTimeline(ObjectId, [this, StartTime, EndTime, VisLogProvider, AnalysisSession, &UniqueTrackIds](const FVisualLoggerProvider::VisualLogEntryTimeline& InTimeline)
		{
			InTimeline.EnumerateEvents(StartTime, EndTime, [this, StartTime, EndTime, VisLogProvider, AnalysisSession, &UniqueTrackIds](double InStartTime, double InEndTime, uint32 InDepth, const FVisualLogEntry& InMessage)
			{
				for(const FVisualLogShapeElement& Element : InMessage.ElementsToDraw)
				{
					UniqueTrackIds.AddUnique(Element.Category);
				}
				return TraceServices::EEventEnumerate::Continue;
			});
		});
	
		UniqueTrackIds.StableSort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
		const int32 TrackCount = UniqueTrackIds.Num();
	
		if (Children.Num() != TrackCount)
			bChanged = true;
	
		Children.SetNum(UniqueTrackIds.Num());
		for(int i = 0; i < TrackCount; i++)
		{
			if (!Children[i].IsValid() || !(Children[i].Get()->GetName() == UniqueTrackIds[i]))
			{
				Children[i] = MakeShared<FVisualLogCategoryTrack>(ObjectId, UniqueTrackIds[i]);
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
	
void FVisualLogTrack::IterateSubTracksInternal(TFunction<void(TSharedPtr<FRewindDebuggerTrack> SubTrack)> IteratorFunction)
{
	for(TSharedPtr<FVisualLogCategoryTrack>& Track : Children)
	{
		IteratorFunction(Track);
	}
};

bool FVisualLogTrackCreator::HasDebugInfoInternal(uint64 ObjectId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVisualLogTrack::HasDebugInfoInternal);
	const TraceServices::IAnalysisSession* AnalysisSession = IRewindDebugger::Instance()->GetAnalysisSession();
	
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	bool bHasData = false;
	if (const FVisualLoggerProvider* VLogProvider = AnalysisSession->ReadProvider<FVisualLoggerProvider>(FVisualLoggerProvider::ProviderName))
	{
		VLogProvider->ReadVisualLogEntryTimeline(ObjectId,  [&bHasData](const FVisualLoggerProvider::VisualLogEntryTimeline& InTimeline)
	 	{
	 		bHasData = true;
	 	});
	}
	return bHasData;
}

	
}

#undef LOCTEXT_NAMESPACE