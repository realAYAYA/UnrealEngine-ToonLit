// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_TimeToEvent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchSchema.h"

UPoseSearchFeatureChannel_TimeToEvent::UPoseSearchFeatureChannel_TimeToEvent()
{
	bUseBlueprintQueryOverride = Cast<UBlueprintGeneratedClass>(GetClass()) != nullptr;
}

bool UPoseSearchFeatureChannel_TimeToEvent::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = 1;
	Schema->SchemaCardinality += ChannelCardinality;
	return true;
}

void UPoseSearchFeatureChannel_TimeToEvent::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const
{
	using namespace UE::PoseSearch;

	if (!bUseBlueprintQueryOverride)
	{
		UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchFeatureChannel_TimeToEvent::BuildQuery - UPoseSearchFeatureChannel_TimeToEvent is designed to work only as BluePrint overridden class. The query TimeToEvent value will be defaulted to 0"));
		return;
	}

	if (SearchContext.GetAnimInstances().IsEmpty())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchFeatureChannel_Position::BuildQuery - no provided anim instance!"));
	}
	
	const float TimeToEvent = BP_GetTimeToEvent(SearchContext.GetAnimInstances()[0]);
	FFeatureVectorHelper::EncodeFloat(SearchContext.EditFeatureVector(), ChannelDataOffset, TimeToEvent);
}

#if WITH_EDITOR
void UPoseSearchFeatureChannel_TimeToEvent::FillWeights(TArrayView<float> Weights) const
{
	for (int32 i = 0; i < ChannelCardinality; ++i)
	{
		Weights[ChannelDataOffset + i] = Weight;
	}
}

bool UPoseSearchFeatureChannel_TimeToEvent::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	using namespace UE::PoseSearch;

	const FPoseSearchTimedNotifies<UAnimNotifyState_PoseSearchSamplingEvent> TimedNotifies(SamplingAttributeId, Indexer);
	for (int32 SampleIdx = Indexer.GetBeginSampleIdx(); SampleIdx != Indexer.GetEndSampleIdx(); ++SampleIdx)
	{
		const float SampleTime = Indexer.CalculateSampleTime(SampleIdx);
		const float EventTime = TimedNotifies.GetClosestFutureEvent(SampleTime).Time;
		FFeatureVectorHelper::EncodeFloat(Indexer.GetPoseVector(SampleIdx), ChannelDataOffset, EventTime - SampleTime);
	}
	
	return true;
}

UE::PoseSearch::TLabelBuilder& UPoseSearchFeatureChannel_TimeToEvent::GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat) const
{
	GetOuterLabel(LabelBuilder, LabelFormat);
	AppendLabelSeparator(LabelBuilder, LabelFormat);
	LabelBuilder.Appendf(TEXT("TimeToEvent_%d"), SamplingAttributeId);
	return LabelBuilder;
}
#endif