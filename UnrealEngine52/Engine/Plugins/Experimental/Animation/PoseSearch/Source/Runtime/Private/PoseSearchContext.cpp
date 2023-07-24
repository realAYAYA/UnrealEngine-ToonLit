// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchContext.h"
#include "AnimationRuntime.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::PoseSearch
{
	
//////////////////////////////////////////////////////////////////////////
// FDebugDrawParams
bool FDebugDrawParams::CanDraw() const
{
#if ENABLE_DRAW_DEBUG
	return World && Database && Database->Schema && Database->Schema->IsValid();
#else // ENABLE_DRAW_DEBUG
	return false;
#endif // ENABLE_DRAW_DEBUG
}

FColor FDebugDrawParams::GetColor(int32 ColorPreset) const
{
#if ENABLE_DRAW_DEBUG
	FLinearColor Color = FLinearColor::Red;

	const UPoseSearchSchema* Schema = GetSchema();
	if (!Schema || !Schema->IsValid())
	{
		Color = FLinearColor::Red;
	}
	else if (ColorPreset < 0 || ColorPreset >= Schema->ColorPresets.Num())
	{
		if (EnumHasAnyFlags(Flags, EDebugDrawFlags::DrawQuery))
		{
			Color = FLinearColor::Blue;
		}
		else
		{
			Color = FLinearColor::Green;
		}
	}
	else
	{
		if (EnumHasAnyFlags(Flags, EDebugDrawFlags::DrawQuery))
		{
			Color = Schema->ColorPresets[ColorPreset].Query;
		}
		else
		{
			Color = Schema->ColorPresets[ColorPreset].Result;
		}
	}

	return Color.ToFColor(true);
#else // ENABLE_DRAW_DEBUG
	return FColor::Black;
#endif // ENABLE_DRAW_DEBUG
}

const FPoseSearchIndex* FDebugDrawParams::GetSearchIndex() const
{
	return Database ? &Database->GetSearchIndex() : nullptr;
}

const UPoseSearchSchema* FDebugDrawParams::GetSchema() const
{
	return Database ? Database->Schema : nullptr;
}

void FDebugDrawParams::ClearCachedPositions()
{
	CachedPositions.Reset();
}

void FDebugDrawParams::AddCachedPosition(float TimeOffset, int8 SchemaBoneIdx, const FVector& Position)
{
	if (const UPoseSearchSchema* Schema = GetSchema())
	{
		CachedPositions.Add(TimeOffset, Schema->GetBoneIndexType(SchemaBoneIdx), Position);
	}
}

FVector FDebugDrawParams::GetCachedPosition(float TimeOffset, int8 SchemaBoneIdx) const
{
	if (const UPoseSearchSchema* Schema = GetSchema())
	{
		if (auto CachedPosition = CachedPositions.Find(TimeOffset, Schema->GetBoneIndexType(SchemaBoneIdx)))
		{
			return CachedPosition->Transform;
		}

		if (Mesh.IsValid() && SchemaBoneIdx >= 0)
		{
			return Mesh->GetSocketTransform(Schema->BoneReferences[SchemaBoneIdx].BoneName).GetLocation();
		}
	}
	return RootTransform.GetTranslation();
}

void DrawFeatureVector(FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector)
{
#if ENABLE_DRAW_DEBUG
	DrawParams.ClearCachedPositions();

	if (DrawParams.CanDraw())
	{
		const UPoseSearchSchema* Schema = DrawParams.GetSchema();
		check(Schema);

		if (PoseVector.Num() == Schema->SchemaCardinality)
		{
			for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema->Channels)
			{
				if (ChannelPtr)
				{
					ChannelPtr->PreDebugDraw(DrawParams, PoseVector);
				}
			}

			for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema->Channels)
			{
				if (ChannelPtr)
				{
					ChannelPtr->DebugDraw(DrawParams, PoseVector);
				}
			}
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

void DrawFeatureVector(FDebugDrawParams& DrawParams, int32 PoseIdx)
{
#if ENABLE_DRAW_DEBUG
	// if we're editing the schema while in PIE with Rewind Debugger active, PoseIdx could be out of bound / stale
	if (DrawParams.CanDraw() && PoseIdx >= 0 && PoseIdx < DrawParams.GetSearchIndex()->NumPoses)
	{
		DrawFeatureVector(DrawParams, DrawParams.GetSearchIndex()->GetPoseValues(PoseIdx));
	}
#endif // ENABLE_DRAW_DEBUG
}

//////////////////////////////////////////////////////////////////////////
// FSearchContext
FTransform FSearchContext::TryGetTransformAndCacheResults(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaBoneIdx)
{
	check(History && Schema);

	const FBoneIndexType BoneIndexType = Schema->GetBoneIndexType(SchemaBoneIdx);
	if (const FCachedTransform<FTransform>* CachedTransform = CachedTransforms.Find(SampleTime, BoneIndexType))
	{
		return CachedTransform->Transform;
	}

	if (BoneIndexType != UPoseSearchSchema::RootBoneIdx)
	{
		TArray<FTransform> SampledLocalPose;
		if (History->TrySampleLocalPose(-SampleTime, &Schema->BoneIndicesWithParents, &SampledLocalPose, nullptr))
		{
			TArray<FTransform> SampledComponentPose;
			FAnimationRuntime::FillUpComponentSpaceTransforms(Schema->Skeleton->GetReferenceSkeleton(), SampledLocalPose, SampledComponentPose);

			// adding bunch of entries, without caring about adding eventual duplicates
			for (const FBoneIndexType NewEntryBoneIndexType : Schema->BoneIndicesWithParents)
			{
				CachedTransforms.Add(SampleTime, NewEntryBoneIndexType, SampledComponentPose[NewEntryBoneIndexType]);
			}

			return SampledComponentPose[BoneIndexType];
		}

		return FTransform::Identity;
	}
	
	FTransform SampledRootTransform;
	if (History->TrySampleLocalPose(-SampleTime, nullptr, nullptr, &SampledRootTransform))
	{
		CachedTransforms.Add(SampleTime, BoneIndexType, SampledRootTransform);
		return SampledRootTransform;
	}
	
	return FTransform::Identity;
}

void FSearchContext::ClearCachedEntries()
{
	CachedTransforms.Reset();
}

void FSearchContext::ResetCurrentBestCost()
{
	CurrentBestTotalCost = MAX_flt;
}

void FSearchContext::UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost)
{
	check(PoseSearchCost.IsValid());

	if (PoseSearchCost.GetTotalCost() < CurrentBestTotalCost)
	{
		CurrentBestTotalCost = PoseSearchCost.GetTotalCost();
	};
}

const FPoseSearchFeatureVectorBuilder* FSearchContext::GetCachedQuery(const UPoseSearchDatabase* Database) const
{
	const FSearchContext::FCachedQuery* CachedQuery = CachedQueries.FindByPredicate([Database](const FSearchContext::FCachedQuery& CachedQuery)
	{
		return CachedQuery.Database == Database;
	});

	if (CachedQuery)
	{
		return &CachedQuery->FeatureVectorBuilder;
	}
	return nullptr;
}

void FSearchContext::GetOrBuildQuery(const UPoseSearchDatabase* Database, FPoseSearchFeatureVectorBuilder& FeatureVectorBuilder)
{
	const FPoseSearchFeatureVectorBuilder* CachedFeatureVectorBuilder = GetCachedQuery(Database);
	if (CachedFeatureVectorBuilder)
	{
		FeatureVectorBuilder = *CachedFeatureVectorBuilder;
	}
	else
	{
		FSearchContext::FCachedQuery& NewCachedQuery = CachedQueries[CachedQueries.AddDefaulted()];
		NewCachedQuery.Database = Database;
		Database->BuildQuery(*this, NewCachedQuery.FeatureVectorBuilder);
		FeatureVectorBuilder = NewCachedQuery.FeatureVectorBuilder;
	}
}

bool FSearchContext::IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const
{
	return CurrentResult.IsValid() && CurrentResult.Database == Database;
}

TConstArrayView<float> FSearchContext::GetCurrentResultPrevPoseVector() const
{
	check(CurrentResult.IsValid());
	const FPoseSearchIndex& SearchIndex = CurrentResult.Database->GetSearchIndex();
	return SearchIndex.GetPoseValues(CurrentResult.PrevPoseIdx);
}

TConstArrayView<float> FSearchContext::GetCurrentResultPoseVector() const
{
	check(CurrentResult.IsValid());
	const FPoseSearchIndex& SearchIndex = CurrentResult.Database->GetSearchIndex();
	return SearchIndex.GetPoseValues(CurrentResult.PoseIdx);
}

TConstArrayView<float> FSearchContext::GetCurrentResultNextPoseVector() const
{
	check(CurrentResult.IsValid());
	const FPoseSearchIndex& SearchIndex = CurrentResult.Database->GetSearchIndex();
	return SearchIndex.GetPoseValues(CurrentResult.NextPoseIdx);
}

} // namespace UE::PoseSearch