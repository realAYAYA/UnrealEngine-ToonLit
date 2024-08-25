// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerChooser.h"

#include "Chooser.h"
#include "ChooserProvider.h"
#include "IGameplayProvider.h"

FRewindDebuggerChooser::FRewindDebuggerChooser()
{

}

void FRewindDebuggerChooser::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	const TraceServices::IAnalysisSession* AnalysisSession = RewindDebugger->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*AnalysisSession);
	
	const FChooserProvider* ChooserProvider = AnalysisSession->ReadProvider<FChooserProvider>(FChooserProvider::ProviderName);
	const IGameplayProvider* GameplayProvider = AnalysisSession->ReadProvider<IGameplayProvider>("GameplayProvider");
	
	const TraceServices::IFrameProvider& FrameProvider = TraceServices::ReadFrameProvider(*RewindDebugger->GetAnalysisSession());
	TraceServices::FFrame Frame;
	if (FrameProvider.GetFrameFromTime(ETraceFrameType::TraceFrameType_Game, RewindDebugger->CurrentTraceTime(), Frame))
	{
		ChooserProvider->EnumerateChooserEvaluationTimelines([GameplayProvider, ChooserProvider, RewindDebugger, Frame](uint64 OwnerId, const FChooserProvider::ChooserEvaluationTimeline& ChooserEvaluationTimeline)
		{
			double StartTime = RewindDebugger->GetScrubTime();
			double EndTime = StartTime + Frame.EndTime - Frame.StartTime;
			
			ChooserEvaluationTimeline.EnumerateEvents(StartTime, EndTime, [GameplayProvider, ChooserProvider, StartTime, EndTime, OwnerId](double InStartTime, double InEndTime, uint32 InDepth, const FChooserEvaluationData& ChooserEvaluationData)  
			{
				const FObjectInfo& ChooserInfo = GameplayProvider->GetObjectInfo(ChooserEvaluationData.ChooserId);
				
				if (UChooserTable* Chooser = FindObject<UChooserTable>(nullptr, ChooserInfo.PathName))
				{
					const FObjectInfo& ContextObjectInfo = GameplayProvider->GetObjectInfo(OwnerId);
					
					// add to recent context objects list, so that this object is selectable as a target in the chooser editor
                 	Chooser->AddRecentContextObject(ContextObjectInfo.Name);
					
					UChooserTable* ContextOwner = Chooser->GetContextOwner();
					if (ContextOwner->HasDebugTarget())
					{
						if (ContextOwner->GetDebugTargetName() == ContextObjectInfo.Name)
						{
							Chooser->SetDebugSelectedRow(ChooserEvaluationData.SelectedIndex);
							Chooser->bDebugTestValuesValid = true;

							ChooserProvider->ReadChooserValueTimeline(OwnerId, [StartTime, EndTime, Chooser, &ChooserEvaluationData](const FChooserProvider::ChooserValueTimeline& ValueTimeline)
							{
								ValueTimeline.EnumerateEvents(StartTime,EndTime,[Chooser, ChooserEvaluationData](double StartTime, double EndTime, uint32 Depth, const FChooserValueData& ValueData)
								{
									if (ChooserEvaluationData.ChooserId == ValueData.ChooserId)
									{
										for(FInstancedStruct& ColumnStruct : Chooser->ColumnsStructs)
										{
											FChooserColumnBase& Column = ColumnStruct.GetMutable<FChooserColumnBase>();
											if(ValueData.Key == Column.GetInputValue()->GetDebugName())
											{
												Column.SetTestValue(ValueData.Value);
											}
										}
									}
									return TraceServices::EEventEnumerate::Continue;
								});
							});
						}
					}
				}
				
				return TraceServices::EEventEnumerate::Continue;
			});
			

		});
	}
}

void FRewindDebuggerChooser::RecordingStarted(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("Chooser"), true);
}

void FRewindDebuggerChooser::RecordingStopped(IRewindDebugger*)
{
	UE::Trace::ToggleChannel(TEXT("Chooser"), false);
}