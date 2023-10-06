// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Padding.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchSchema.h"

void UPoseSearchFeatureChannel_Padding::AddToSchema(UPoseSearchSchema* Schema, int32 PaddingSize)
{
	UPoseSearchFeatureChannel_Padding* Padding = NewObject<UPoseSearchFeatureChannel_Padding>(Schema, NAME_None, RF_Transient);
	Padding->PaddingSize = PaddingSize;
	Schema->AddTemporaryChannel(Padding);
}

void UPoseSearchFeatureChannel_Padding::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = PaddingSize;
	Schema->SchemaCardinality += ChannelCardinality;
}

void UPoseSearchFeatureChannel_Padding::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const
{
	// nothing to do here
}

#if WITH_EDITOR
void UPoseSearchFeatureChannel_Padding::FillWeights(TArrayView<float> Weights) const
{
	for (int32 i = 0; i < ChannelCardinality; ++i)
	{
		Weights[ChannelDataOffset + i] = 0.f;
	}
}

void UPoseSearchFeatureChannel_Padding::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	UE_LOG(LogPoseSearch, Log, TEXT("UPoseSearchFeatureChannel_Padding::IndexAsset: padding data with '%d' additional floats per pose in UPoseSearchSchema '%s'"), PaddingSize, *GetNameSafe(GetSchema()));
}

FString UPoseSearchFeatureChannel_Padding::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}

	Label.Appendf(TEXT("Pad_%d_%d"), ChannelDataOffset, ChannelDataOffset + ChannelCardinality);
	return Label.ToString();
}
#endif