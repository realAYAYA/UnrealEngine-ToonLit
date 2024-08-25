// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserTrace.h"
#include "ObjectTrace.h"
#include "IObjectChooser.h"
#include "Serialization/BufferArchive.h"

#if CHOOSER_TRACE_ENABLED

UE_TRACE_CHANNEL(ChooserChannel)

UE_TRACE_EVENT_BEGIN(Chooser, ChooserEvaluation)
	UE_TRACE_EVENT_FIELD(double, RecordingTime)
	UE_TRACE_EVENT_FIELD(uint64, ChooserId)
	UE_TRACE_EVENT_FIELD(uint64, OwnerId)
	UE_TRACE_EVENT_FIELD(int32, SelectedIndex)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Chooser, ChooserValue)
	UE_TRACE_EVENT_FIELD(double, RecordingTime)
	UE_TRACE_EVENT_FIELD(uint64, ChooserId)
	UE_TRACE_EVENT_FIELD(uint64, OwnerId)
	UE_TRACE_EVENT_FIELD(uint8[], Value)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Key)
UE_TRACE_EVENT_END()

namespace
{
	UObject* GetContextObject(const FChooserEvaluationContext& Context)
	{
		for (const FStructView& ContextEntry : Context.Params)
		{
			if (const FChooserEvaluationInputObject* ContextObjectInput = ContextEntry.GetPtr<FChooserEvaluationInputObject>())
			{
				return ContextObjectInput->Object;
			}
		}
		return nullptr;
	}
}



void FChooserTrace::OutputChooserValueArchive(const FChooserEvaluationContext& Context, const TCHAR* InKey, const FBufferArchive& InValueArchive)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ChooserChannel);

	if (!bChannelEnabled  || Context.DebuggingInfo.CurrentChooser == nullptr)
	{
		return;
	}

	const UObject* ContextObject = GetContextObject(Context);

	if (ContextObject == nullptr || CANNOT_TRACE_OBJECT(ContextObject->GetWorld()))
	{
		return;
	}
	
	TRACE_OBJECT(Context.DebuggingInfo.CurrentChooser);
	TRACE_OBJECT(ContextObject);

	UE_TRACE_LOG(Chooser, ChooserValue, ChooserChannel)
		<< ChooserValue.RecordingTime(FObjectTrace::GetWorldElapsedTime(ContextObject->GetWorld()))
    	<< ChooserValue.ChooserId(FObjectTrace::GetObjectId(Context.DebuggingInfo.CurrentChooser))
    	<< ChooserValue.OwnerId(FObjectTrace::GetObjectId(ContextObject))
    	<< ChooserValue.Key(InKey)
    	<< ChooserValue.Value(InValueArchive.GetData(), InValueArchive.Num());
}

void FChooserTrace::OutputChooserEvaluation(const UObject* InChooser, const FChooserEvaluationContext& Context, uint32 InSelectedIndex)
{
	bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(ChooserChannel);
	if (!bChannelEnabled || InChooser == nullptr)
	{
		return;
	}
	
	const UObject* ContextObject = GetContextObject(Context);

	if (ContextObject == nullptr || CANNOT_TRACE_OBJECT(ContextObject->GetWorld()))
	{
		return;
	}

	TRACE_OBJECT(InChooser);
	TRACE_OBJECT(ContextObject);

	UE_TRACE_LOG(Chooser, ChooserEvaluation, ChooserChannel)
	<< ChooserEvaluation.RecordingTime(FObjectTrace::GetWorldElapsedTime(ContextObject->GetWorld()))
	<< ChooserEvaluation.ChooserId(FObjectTrace::GetObjectId(InChooser))
	<< ChooserEvaluation.OwnerId(FObjectTrace::GetObjectId(ContextObject))
	<< ChooserEvaluation.SelectedIndex(InSelectedIndex);
}

#endif

