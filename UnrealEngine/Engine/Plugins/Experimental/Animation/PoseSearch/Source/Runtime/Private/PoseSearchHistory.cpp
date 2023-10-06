// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchHistory.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/SkeletonRemappingRegistry.h"
#include "BonePose.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"

IMPLEMENT_ANIMGRAPH_MESSAGE(UE::PoseSearch::IPoseHistoryProvider);

namespace UE::PoseSearch
{

/**
* Algo::LowerBound adapted to TIndexedContainerIterator for use with indexable but not necessarily contiguous containers. Used here with TRingBuffer.
*
* Performs binary search, resulting in position of the first element >= Value using predicate
*
* @param First TIndexedContainerIterator beginning of range to search through, must be already sorted by SortPredicate
* @param Last TIndexedContainerIterator end of range
* @param Value Value to look for
* @param SortPredicate Predicate for sort comparison, defaults to <
*
* @returns Position of the first element >= Value, may be position after last element in range
*/
template <typename IteratorType, typename ValueType, typename ProjectionType, typename SortPredicateType>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value, ProjectionType Projection, SortPredicateType SortPredicate) -> decltype(First.GetIndex())
{
	using SizeType = decltype(First.GetIndex());

	check(First.GetIndex() <= Last.GetIndex());

	// Current start of sequence to check
	SizeType Start = First.GetIndex();

	// Size of sequence to check
	SizeType Size = Last.GetIndex() - Start;

	// With this method, if Size is even it will do one more comparison than necessary, but because Size can be predicted by the CPU it is faster in practice
	while (Size > 0)
	{
		const SizeType LeftoverSize = Size % 2;
		Size = Size / 2;

		const SizeType CheckIndex = Start + Size;
		const SizeType StartIfLess = CheckIndex + LeftoverSize;

		auto&& CheckValue = Invoke(Projection, *(First + CheckIndex));
		Start = SortPredicate(CheckValue, Value) ? StartIfLess : Start;
	}
	return Start;
}

template <typename IteratorType, typename ValueType, typename SortPredicateType = TLess<>()>
FORCEINLINE auto LowerBound(IteratorType First, IteratorType Last, const ValueType& Value, SortPredicateType SortPredicate) -> decltype(First.GetIndex())
{
	return LowerBound(First, Last, Value, FIdentityFunctor(), SortPredicate);
}

//////////////////////////////////////////////////////////////////////////
// FPoseHistoryEntry
void FPoseHistoryEntry::Update(float InTime, FCSPose<FCompactPose>& ComponentSpacePose, const FTransform& ComponentTransform, const FBoneToTransformMap& BoneToTransformMap)
{
	// @todo: optimize this math by initializing FCSPose<FCompactPose> root to identity
	const FTransform ComponentToRootTransform = ComponentSpacePose.GetComponentSpaceTransform(FCompactPoseBoneIndex(RootBoneIndexType));
	const FTransform ComponentToRootTransformInv = ComponentToRootTransform.Inverse();

	Time = InTime;
	RootTransform = ComponentToRootTransform * ComponentTransform;

	const FBoneContainer& BoneContainer = ComponentSpacePose.GetPose().GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	check(SkeletonAsset);
	const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
	const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();

	if (BoneToTransformMap.IsEmpty())
	{
		// no mapping: we add all the transforms
		ComponentSpaceTransforms.SetNum(NumSkeletonBones);
		for (FSkeletonPoseBoneIndex SkeletonBoneIdx(0); SkeletonBoneIdx != NumSkeletonBones; ++SkeletonBoneIdx)
		{
			const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
			ComponentSpaceTransforms[SkeletonBoneIdx.GetInt()] = 
				(CompactBoneIdx.IsValid() ? ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIdx) : RefBonePose[SkeletonBoneIdx.GetInt()]) * ComponentToRootTransformInv;
		}
	}
	else
	{
		ComponentSpaceTransforms.SetNum(BoneToTransformMap.Num());
		for (const FBoneToTransformPair& BoneToTransformPair : BoneToTransformMap)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIdx(BoneToTransformPair.Key);
			const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
			ComponentSpaceTransforms[BoneToTransformPair.Value] = 
				(CompactBoneIdx.IsValid() ? ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIdx) : RefBonePose[SkeletonBoneIdx.GetInt()]) * ComponentToRootTransformInv;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FPoseHistory
void FPoseHistory::Init(int32 InNumPoses, float InTimeHorizon, const TArray<FBoneIndexType>& RequiredBones)
{
	TimeHorizon = InTimeHorizon;

	BoneToTransformMap.Reset();
	for (int32 i = 0; i < RequiredBones.Num(); ++i)
	{
		BoneToTransformMap.Add(RequiredBones[i]) = i;
	}

	Entries.Reset();
	Entries.Reserve(InNumPoses);
}

FBoneIndexType FPoseHistory::GetRemappedBoneIndexType(FBoneIndexType BoneIndexType, const USkeleton* BoneIndexSkeleton) const
{
	// remapping BoneIndexType in case the skeleton used to store history (LastUpdateSkeleton) is different from BoneIndexSkeleton
	if (LastUpdateSkeleton != nullptr && LastUpdateSkeleton != BoneIndexSkeleton)
	{
		const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(BoneIndexSkeleton, LastUpdateSkeleton.Get());
		if (SkeletonRemapping.IsValid())
		{
			BoneIndexType = SkeletonRemapping.GetTargetSkeletonBoneIndex(BoneIndexType);
		}
	}

	return BoneIndexType;
}

bool FPoseHistory::GetComponentSpaceTransformAtTime(float Time, FBoneIndexType BoneIndexType, const USkeleton* BoneIndexSkeleton, FTransform& OutBoneTransform, bool bExtrapolate) const
{
	BoneIndexType = GetRemappedBoneIndexType(BoneIndexType, BoneIndexSkeleton);

	FComponentSpaceTransformIndex TransformIndex = FComponentSpaceTransformIndex(BoneIndexType);
	if (!BoneToTransformMap.IsEmpty())
	{
		if (const FComponentSpaceTransformIndex* FoundTransformIndex = BoneToTransformMap.Find(BoneIndexType))
		{
			TransformIndex = *FoundTransformIndex;
		}
		else
		{
			GetRootTransformAtTime(Time, OutBoneTransform, bExtrapolate);
			return false;
		}
	}

	const int32 Num = Entries.Num();
	if (Num > 1)
	{
		const float SecondsAgo = -Time;
		const int32 LowerBoundIdx = LowerBound(Entries.begin(), Entries.end(), SecondsAgo, [](const FPoseHistoryEntry& Entry, float Value) { return Value < Entry.Time; });
		const int32 NextIdx = FMath::Clamp(LowerBoundIdx, 1, Num - 1);
		const int32 PrevIdx = NextIdx - 1;

		const FPoseHistoryEntry& PrevEntry = Entries[PrevIdx];
		const FPoseHistoryEntry& NextEntry = Entries[NextIdx];

		const float Denominator = NextEntry.Time - PrevEntry.Time;
		if (!FMath::IsNearlyZero(Denominator))
		{
			const float Numerator = SecondsAgo - PrevEntry.Time;
			const float LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
			OutBoneTransform.Blend(PrevEntry.ComponentSpaceTransforms[TransformIndex], NextEntry.ComponentSpaceTransforms[TransformIndex], LerpValue);
		}
		else
		{
			OutBoneTransform = PrevEntry.ComponentSpaceTransforms[TransformIndex];
		}
	}
	else if (Num > 0)
	{
		OutBoneTransform = Entries[0].ComponentSpaceTransforms[TransformIndex];
	}
	else
	{
		GetRootTransformAtTime(Time, OutBoneTransform, bExtrapolate);
		return false;
	}

	return true;
}

void FPoseHistory::GetRootTransformAtTime(float Time, FTransform& OutRootTransform, bool bExtrapolate) const
{
	const int32 Num = Entries.Num();
	if (Num > 1)
	{
		const float SecondsAgo = -Time;
		const int32 LowerBoundIdx = LowerBound(Entries.begin(), Entries.end(), SecondsAgo, [](const FPoseHistoryEntry& Entry, float Value) { return Value < Entry.Time; });
		const int32 NextIdx = FMath::Clamp(LowerBoundIdx, 1, Num - 1);
		const int32 PrevIdx = NextIdx - 1;

		const FPoseHistoryEntry& PrevEntry = Entries[PrevIdx];
		const FPoseHistoryEntry& NextEntry = Entries[NextIdx];

		const float Denominator = NextEntry.Time - PrevEntry.Time;
		if (!FMath::IsNearlyZero(Denominator))
		{
			const float Numerator = SecondsAgo - PrevEntry.Time;
			const float LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
			OutRootTransform.Blend(PrevEntry.RootTransform, NextEntry.RootTransform, LerpValue);
		}
		else
		{
			OutRootTransform = PrevEntry.RootTransform;
		}
	}
	else if (Num > 0)
	{
		OutRootTransform = Entries[0].RootTransform;
	}
	else
	{
		OutRootTransform = FTransform::Identity;
	}
}

bool FPoseHistory::IsEmpty() const
{
	return Entries.IsEmpty();
}

void FPoseHistory::Update(float SecondsElapsed, FCSPose<FCompactPose>& ComponentSpacePose, const FTransform& ComponentTransform)
{
	const USkeleton* Skeleton = ComponentSpacePose.GetPose().GetBoneContainer().GetSkeletonAsset();
	if (LastUpdateSkeleton != Skeleton)
	{
		// @todo: support a different USkeleton per FPoseHistoryEntry if required
		Entries.Reset();
		LastUpdateSkeleton = Skeleton;
	}

	// Age our elapsed times
	for (FPoseHistoryEntry& Entry : Entries)
	{
		Entry.Time += SecondsElapsed;
	}

	if (Entries.Num() != Entries.Max())
	{
		// Consume every pose until the queue is full
		Entries.Emplace();
	}
	else
	{
		// Exercise pose retention policy. We must guarantee there is always one additional pose
		// beyond the time horizon so we can compute derivatives at the time horizon. We also
		// want to evenly distribute poses across the entire history buffer so we only push additional
		// poses when enough time has elapsed.

		const float SampleInterval = GetSampleTimeInterval();

		bool bCanEvictOldest = Entries[1].Time >= TimeHorizon + SampleInterval;
		bool bShouldPushNewest = Entries[Entries.Num() - 2].Time >= SampleInterval;

		if (bCanEvictOldest && bShouldPushNewest)
		{
			FPoseHistoryEntry EntryTemp = MoveTemp(Entries.First());
			Entries.PopFront();
			Entries.Emplace(MoveTemp(EntryTemp));
		}
	}

	// Regardless of the retention policy, we always update the most recent Entry
	Entries.Last().Update(0.f, ComponentSpacePose, ComponentTransform, BoneToTransformMap);
}

float FPoseHistory::GetSampleTimeInterval() const
{
	// Reserve one pose for computing derivatives at the time horizon
	return TimeHorizon / (Entries.Max() - 1);
}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy) const
{
	auto LerpColor = [](FColor A, FColor B, float T) -> FColor
	{
		return FColor(
			FMath::RoundToInt(float(A.R) * (1.f - T) + float(B.R) * T),
			FMath::RoundToInt(float(A.G) * (1.f - T) + float(B.G) * T),
			FMath::RoundToInt(float(A.B) * (1.f - T) + float(B.B) * T),
			FMath::RoundToInt(float(A.A) * (1.f - T) + float(B.A) * T));
	};

	TArray<FTransform> PrevGlobalTransforms;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FPoseHistoryEntry& Entry = Entries[EntryIndex];
		if (Entry.ComponentSpaceTransforms.IsEmpty())
		{
			PrevGlobalTransforms.Reset();
		}
		else if (PrevGlobalTransforms.Num() != Entry.ComponentSpaceTransforms.Num())
		{
			PrevGlobalTransforms.SetNum(Entry.ComponentSpaceTransforms.Num());
			for (int32 i = 0; i < Entry.ComponentSpaceTransforms.Num(); ++i)
			{
				PrevGlobalTransforms[i] = Entry.ComponentSpaceTransforms[i] * Entry.RootTransform;
			}
		}
		else
		{
			const float LerpFactor = float(EntryIndex - 1) / float(Entries.Num() - 1);
			const FColor Color = LerpColor(FColorList::Red, FColorList::Orange, LerpFactor);

			for (int32 i = 0; i < Entry.ComponentSpaceTransforms.Num(); ++i)
			{
				const FTransform GlobalTransforms = Entry.ComponentSpaceTransforms[i] * Entry.RootTransform;

				AnimInstanceProxy.AnimDrawDebugLine(PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FExtendedPoseHistory
void FExtendedPoseHistory::Init(const FPoseHistory* InPoseHistory)
{
	check(InPoseHistory);
	PoseHistory = InPoseHistory;
}

bool FExtendedPoseHistory::IsInitialized() const
{
	return PoseHistory != nullptr;
}

float FExtendedPoseHistory::GetSampleTimeInterval() const
{
	check(PoseHistory);
	return PoseHistory->GetSampleTimeInterval();
}

bool FExtendedPoseHistory::GetComponentSpaceTransformAtTime(float Time, FBoneIndexType BoneIndexType, const USkeleton* BoneIndexSkeleton, FTransform& OutBoneTransform, bool bExtrapolate) const
{
	check(PoseHistory);

	const FBoneIndexType OriginalBoneIndex = BoneIndexType;

	BoneIndexType = PoseHistory->GetRemappedBoneIndexType(BoneIndexType, BoneIndexSkeleton);

	if (Time > 0.f)
	{
 		FComponentSpaceTransformIndex TransformIndex = FComponentSpaceTransformIndex(BoneIndexType);
		const FBoneToTransformMap& BoneToTransformMap = PoseHistory->GetBoneToTransformMap();
		if (!BoneToTransformMap.IsEmpty())
		{
			if (const FComponentSpaceTransformIndex* FoundTransformIndex = BoneToTransformMap.Find(BoneIndexType))
			{
				TransformIndex = *FoundTransformIndex;
			}
			else
			{
				GetRootTransformAtTime(Time, OutBoneTransform, bExtrapolate);
				return false;
			}
		}

		const int32 Num = FutureEntries.Num();
		if (Num > 0)
		{
			const float SecondsAgo = -Time;
			const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, SecondsAgo, [](const FPoseHistoryEntry& Entry, float Value) { return Value < Entry.Time; });
			const int32 NextIdx = FMath::Min(LowerBoundIdx, Num - 1);
			const FPoseHistoryEntries& PastEntries = PoseHistory->GetEntries();
			const FPoseHistoryEntry& NextEntry = FutureEntries[NextIdx];
			const FPoseHistoryEntry& PrevEntry = NextIdx > 0 ? FutureEntries[NextIdx - 1] : !PastEntries.IsEmpty() ? PastEntries.First() : NextEntry;

			const float Denominator = NextEntry.Time - PrevEntry.Time;
			if (!FMath::IsNearlyZero(Denominator))
			{
				const float Numerator = SecondsAgo - PrevEntry.Time;
				const float LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
				OutBoneTransform.Blend(PrevEntry.ComponentSpaceTransforms[TransformIndex], NextEntry.ComponentSpaceTransforms[TransformIndex], LerpValue);
			}
			else
			{
				OutBoneTransform = NextEntry.ComponentSpaceTransforms[TransformIndex];
			}
			return true;
		}

		// we've no FutureEntries, so let's clamp the time to zero and query PoseHistory
		Time = 0.f;
	}

	return PoseHistory->GetComponentSpaceTransformAtTime(Time, OriginalBoneIndex, BoneIndexSkeleton, OutBoneTransform, bExtrapolate);
}

void FExtendedPoseHistory::GetRootTransformAtTime(float Time, FTransform& OutRootTransform, bool bExtrapolate) const
{
	check(PoseHistory);

	if (Time > 0.f)
	{
		const int32 Num = FutureEntries.Num();
		if (Num > 0)
		{
			const float SecondsAgo = -Time;
			const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, SecondsAgo, [](const FPoseHistoryEntry& Entry, float Value) { return Value < Entry.Time; });
			const int32 NextIdx = FMath::Min(LowerBoundIdx, Num - 1);
			const FPoseHistoryEntries& PastEntries = PoseHistory->GetEntries();
			const FPoseHistoryEntry& NextEntry = FutureEntries[NextIdx];
			const FPoseHistoryEntry& PrevEntry = NextIdx > 0 ? FutureEntries[NextIdx - 1] : !PastEntries.IsEmpty() ? PastEntries.First() : NextEntry;

			const float Denominator = NextEntry.Time - PrevEntry.Time;
			if (!FMath::IsNearlyZero(Denominator))
			{
				const float Numerator = SecondsAgo - PrevEntry.Time;
				const float LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
				OutRootTransform.Blend(PrevEntry.RootTransform, NextEntry.RootTransform, LerpValue);
			}
			else
			{
				OutRootTransform = NextEntry.RootTransform;
			}

			return;
		}

		// we've no FutureEntries, so let's clamp the time to zero and query PoseHistory
		Time = 0.f;
	}

	PoseHistory->GetRootTransformAtTime(Time, OutRootTransform, bExtrapolate);
}

bool FExtendedPoseHistory::IsEmpty() const
{
	check(PoseHistory);
	return PoseHistory->IsEmpty() && FutureEntries.IsEmpty();
}

void FExtendedPoseHistory::ResetFuturePoses()
{
	FutureEntries.Reset();
}

void FExtendedPoseHistory::AddFuturePose(float SecondsInTheFuture, FCSPose<FCompactPose>& ComponentSpacePose, const FTransform& ComponentTransform)
{
	// we don't allow to add "past" or "present" poses to FutureEntries
	check(SecondsInTheFuture > 0.f);
	check(PoseHistory);	
	const float SecondsAgo = -SecondsInTheFuture;
	const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, SecondsAgo, [](const FPoseHistoryEntry& Entry, float Value) { return Value < Entry.Time; });
	FutureEntries.InsertDefaulted_GetRef(LowerBoundIdx).Update(SecondsAgo, ComponentSpacePose, ComponentTransform, PoseHistory->GetBoneToTransformMap());
}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FExtendedPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy) const
{
	check(PoseHistory);

	auto LerpColor = [](FColor A, FColor B, float T) -> FColor
	{
		return FColor(
			FMath::RoundToInt(float(A.R) * (1.f - T) + float(B.R) * T),
			FMath::RoundToInt(float(A.G) * (1.f - T) + float(B.G) * T),
			FMath::RoundToInt(float(A.B) * (1.f - T) + float(B.B) * T),
			FMath::RoundToInt(float(A.A) * (1.f - T) + float(B.A) * T));
	};

	TArray<FTransform> PrevGlobalTransforms;
	for (int32 EntryIndex = 0; EntryIndex < FutureEntries.Num(); ++EntryIndex)
	{
		const FPoseHistoryEntry& Entry = FutureEntries[EntryIndex];
		if (Entry.ComponentSpaceTransforms.IsEmpty())
		{
			PrevGlobalTransforms.Reset();
		}
		else if (PrevGlobalTransforms.Num() != Entry.ComponentSpaceTransforms.Num())
		{
			PrevGlobalTransforms.SetNum(Entry.ComponentSpaceTransforms.Num());
			for (int32 i = 0; i < Entry.ComponentSpaceTransforms.Num(); ++i)
			{
				PrevGlobalTransforms[i] = Entry.ComponentSpaceTransforms[i] * Entry.RootTransform;
			}
		}
		else
		{
			const float LerpFactor = float(EntryIndex - 1) / float(FutureEntries.Num() - 1);
			const FColor Color = LerpColor(FColorList::Green, FColorList::Violet, LerpFactor);

			for (int32 i = 0; i < Entry.ComponentSpaceTransforms.Num(); ++i)
			{
				const FTransform GlobalTransforms = Entry.ComponentSpaceTransforms[i] * Entry.RootTransform;

				AnimInstanceProxy.AnimDrawDebugLine(PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}
	}

	PoseHistory->DebugDraw(AnimInstanceProxy);
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FPoseIndicesHistory
void FPoseIndicesHistory::Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime)
{
	if (MaxTime > 0.f)
	{
		for (auto It = IndexToTime.CreateIterator(); It; ++It)
		{
			It.Value() += DeltaTime;
			if (It.Value() > MaxTime)
			{
				It.RemoveCurrent();
			}
		}

		if (SearchResult.IsValid())
		{
			FHistoricalPoseIndex HistoricalPoseIndex;
			HistoricalPoseIndex.PoseIndex = SearchResult.PoseIdx;
			HistoricalPoseIndex.DatabaseKey = FObjectKey(SearchResult.Database.Get());
			IndexToTime.Add(HistoricalPoseIndex, 0.f);
		}
	}
	else
	{
		IndexToTime.Reset();
	}
}

} // namespace UE::PoseSearch
