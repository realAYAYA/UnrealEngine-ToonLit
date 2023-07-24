// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Group.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchSchema.h"

void UPoseSearchFeatureChannel_GroupBase::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	for (TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->Finalize(Schema);
		}
	}
	ChannelCardinality = Schema->SchemaCardinality - ChannelDataOffset;
}

void UPoseSearchFeatureChannel_GroupBase::FillWeights(TArray<float>& Weights) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->FillWeights(Weights);
		}
	}
}

void UPoseSearchFeatureChannel_GroupBase::IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, TArrayView<float> FeatureVectorTable) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->IndexAsset(Indexer, FeatureVectorTable);
		}
	}
}

void UPoseSearchFeatureChannel_GroupBase::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->BuildQuery(SearchContext, InOutQuery);
		}
	}
}

void UPoseSearchFeatureChannel_GroupBase::PreDebugDraw(UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
#if ENABLE_DRAW_DEBUG
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->PreDebugDraw(DrawParams, PoseVector);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

void UPoseSearchFeatureChannel_GroupBase::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
#if ENABLE_DRAW_DEBUG
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->DebugDraw(DrawParams, PoseVector);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

// IPoseFilter interface
bool UPoseSearchFeatureChannel_GroupBase::IsPoseFilterActive() const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			if (SubChannel->IsPoseFilterActive())
			{
				return true;
			}
		}
	}

	return false;
}

bool UPoseSearchFeatureChannel_GroupBase::IsPoseValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			if (!SubChannel->IsPoseValid(PoseValues, QueryValues, PoseIdx, Metadata))
			{
				return false;
			}
		}
	}

	return true;
}

#if WITH_EDITOR
FString UPoseSearchFeatureChannel_Group::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}
	Label.Append(TEXT("Group"));
	return Label.ToString();
}
#endif // WITH_EDITOR
