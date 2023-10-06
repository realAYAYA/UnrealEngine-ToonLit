// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Group.h"
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

void UPoseSearchFeatureChannel_GroupBase::AddDependentChannels(UPoseSearchSchema* Schema) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (SubChannelPtr)
		{
			SubChannelPtr->AddDependentChannels(Schema);
		}
	}
}

#if WITH_EDITOR
void UPoseSearchFeatureChannel_GroupBase::FillWeights(TArrayView<float> Weights) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->FillWeights(Weights);
		}
	}
}

void UPoseSearchFeatureChannel_GroupBase::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->IndexAsset(Indexer);
		}
	}
}
#endif // WITH_EDITOR

void UPoseSearchFeatureChannel_GroupBase::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->BuildQuery(SearchContext, InOutQuery);
		}
	}
}

#if ENABLE_DRAW_DEBUG
void UPoseSearchFeatureChannel_GroupBase::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			SubChannel->DebugDraw(DrawParams, PoseVector);
		}
	}
}
#endif // ENABLE_DRAW_DEBUG

// IPoseSearchFilter interface
bool UPoseSearchFeatureChannel_GroupBase::IsFilterActive() const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			if (SubChannel->IsFilterActive())
			{
				return true;
			}
		}
	}

	return false;
}

bool UPoseSearchFeatureChannel_GroupBase::IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const UE::PoseSearch::FPoseMetadata& Metadata) const
{
	for (const TObjectPtr<UPoseSearchFeatureChannel>& SubChannelPtr : GetSubChannels())
	{
		if (const UPoseSearchFeatureChannel* SubChannel = SubChannelPtr.Get())
		{
			if (!SubChannel->IsFilterValid(PoseValues, QueryValues, PoseIdx, Metadata))
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
