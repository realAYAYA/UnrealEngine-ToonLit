// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Model/PointTimeline.h"
#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Model/IntervalTimeline.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices { class IAnalysisSession; }

struct FChooserEvaluationData
{
	uint64 ChooserId;
	int32 SelectedIndex;
};

struct FChooserValueData
{
	uint64 ChooserId;
	FName Key;
	// probably should get this using the linear allocator somehow:
	TArray<uint8> Value;
};

class FChooserProvider : public TraceServices::IProvider
{
public:
	typedef TraceServices::ITimeline<FChooserEvaluationData> ChooserEvaluationTimeline;
	typedef TraceServices::ITimeline<FChooserValueData> ChooserValueTimeline;
	
	static FName ProviderName;

	FChooserProvider(TraceServices::IAnalysisSession& InSession);

	bool ReadChooserEvaluationTimeline(uint64 InObjectId, TFunctionRef<void(const ChooserEvaluationTimeline&)> Callback) const;
	bool ReadChooserValueTimeline(uint64 InObjectId, TFunctionRef<void(const ChooserValueTimeline&)> Callback) const;
	void EnumerateChooserEvaluationTimelines(TFunctionRef<void(uint64 OwnerId, const ChooserEvaluationTimeline&)> Callback) const;



	void AppendChooserEvaluation(uint64 InObjectId, uint64 InChooser, int32 InResultId, double InRecordingTime);

	void AppendChooserValue(uint64 InChooserId, uint64 InObjectId, double InRecordingTime, const FName& Key, TArrayView<const uint8> SerializedValue)
    {
    	Session.WriteAccessCheck();
    
    	TSharedPtr<TraceServices::TPointTimeline<FChooserValueData>> Timeline = ChooserValueTimelines[GetTimelineIndex(InObjectId)];
		FChooserValueData EventData;
		EventData.ChooserId = InChooserId;
		EventData.Key = Key;
		EventData.Value = SerializedValue;
    	Timeline->AppendEvent(InRecordingTime, EventData);
    }

private:
	uint32 GetTimelineIndex(uint64 ObjectId);
	
	TraceServices::IAnalysisSession& Session;

	TMap<uint64, uint32> ObjectIdToChooserEvaluationTimelines;
	TArray<TSharedRef<TraceServices::TPointTimeline<FChooserEvaluationData>>> ChooserEvaluationTimelines;
	TArray<TSharedRef<TraceServices::TPointTimeline<FChooserValueData>>> ChooserValueTimelines;
};
