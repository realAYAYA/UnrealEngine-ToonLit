// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchResult.h"
#include "Animation/BlendSpace.h"
#include "InstancedStruct.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// FFeatureVectorBuilder
FFeatureVectorBuilder::FFeatureVectorBuilder(const UPoseSearchSchema* InSchema)
{
	check(InSchema && InSchema->IsValid());
	Schema = InSchema;
	Values.SetNumZeroed(Schema->SchemaCardinality);
}

//////////////////////////////////////////////////////////////////////////
// FSearchResult
void FSearchResult::Update(float NewAssetTime)
{
	if (!IsValid())
	{
		Reset();
	}
	else
	{
		const FSearchIndexAsset& SearchIndexAsset = Database->GetSearchIndex().GetAssetForPose(PoseIdx);
		const FInstancedStruct& DatabaseAsset = Database->GetAnimationAssetStruct(SearchIndexAsset);
		if (DatabaseAsset.GetPtr<FPoseSearchDatabaseSequence>() || DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimComposite>())
		{
			PoseIdx = Database->GetPoseIndexFromTime(NewAssetTime, SearchIndexAsset);
			AssetTime = NewAssetTime;
		}
		else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
		{
			TArray<FBlendSampleData> BlendSamples;
			int32 TriangulationIndex = 0;
			DatabaseBlendSpace->BlendSpace->GetSamplesFromBlendInput(SearchIndexAsset.BlendParameters, BlendSamples, TriangulationIndex, true);

			const float PlayLength = DatabaseBlendSpace->BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

			// Asset player time for blendspaces is normalized [0, 1] so we need to convert 
			// to a real time before we advance it
			check(NewAssetTime >= 0.f && NewAssetTime <= 1.f);
			const float RealTime = NewAssetTime * PlayLength;

			PoseIdx = Database->GetPoseIndexFromTime(RealTime, SearchIndexAsset);
			AssetTime = NewAssetTime;
		}
		else
		{
			Reset();
		}
	}
}

bool FSearchResult::IsValid() const
{
	return PoseIdx != INDEX_NONE && Database != nullptr;
}

void FSearchResult::Reset()
{
	PoseIdx = INDEX_NONE;
	Database = nullptr;
	AssetTime = 0.0f;
}

const FSearchIndexAsset* FSearchResult::GetSearchIndexAsset(bool bMandatory) const
{
	if (bMandatory)
	{
		check(IsValid());
	}
	else if (!IsValid())
	{
		return nullptr;
	}

	return &Database->GetSearchIndex().GetAssetForPose(PoseIdx);
}

bool FSearchResult::CanAdvance(float DeltaTime) const
{
	bool bCanAdvance = false;
	if (IsValid())
	{
		float SteppedTime = AssetTime;
		const FSearchIndexAsset* SearchIndexAsset = GetSearchIndexAsset(true);
		const FInstancedStruct& DatabaseAsset = Database->GetAnimationAssetStruct(*SearchIndexAsset);
		if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
		{
			TArray<FBlendSampleData> BlendSamples;
			int32 TriangulationIndex = 0;
			DatabaseBlendSpace->BlendSpace->GetSamplesFromBlendInput(SearchIndexAsset->BlendParameters, BlendSamples, TriangulationIndex, true);

			const float PlayLength = DatabaseBlendSpace->BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

			// Asset player time for blend spaces is normalized [0, 1] so we need to convert it back to real time before we advance it
			SteppedTime = AssetTime * PlayLength;
			bCanAdvance = ETAA_Finished != FAnimationRuntime::AdvanceTime(DatabaseBlendSpace->IsLooping(), DeltaTime, SteppedTime, PlayLength);
		}
		else if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
		{
			const float AssetLength = DatabaseAnimationAssetBase->GetAnimationAsset()->GetPlayLength();
			bCanAdvance = ETAA_Finished != FAnimationRuntime::AdvanceTime(DatabaseAnimationAssetBase->IsLooping(), DeltaTime, SteppedTime, AssetLength);
		}
	}
	return bCanAdvance;
}

} // namespace UE::PoseSearch
