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

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDrawPose(TEXT("a.AnimNode.PoseHistory.DebugDrawPose"), false, TEXT("Enable / Disable Pose History Pose DebugDraw"));
TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDrawTrajectory(TEXT("a.AnimNode.PoseHistory.DebugDrawTrajectory"), false, TEXT("Enable / Disable Pose History Trajectory DebugDraw"));
TAutoConsoleVariable<float> CVarAnimPoseHistoryDebugDrawTrajectoryThickness(TEXT("a.AnimNode.PoseHistory.DebugDrawTrajectoryThickness"), 0.0f, TEXT("Thickness of the trajectory debug draw (Default 0.0f)"));
TAutoConsoleVariable<int> CVarAnimPoseHistoryDebugDrawTrajectoryMaxNumOfHistorySamples(TEXT("a.AnimNode.PoseHistory.DebugDrawMaxNumOfHistorySamples"), -1, TEXT("Max number of history samples to debug draw. All history samples will be drawn if value is negative. (Default -1)"));
TAutoConsoleVariable<int> CVarAnimPoseHistoryDebugDrawTrajectoryMaxNumOfPredictionSamples(TEXT("a.AnimNode.PoseHistory.DebugDrawMaxNumOfPredictionSamples"), -1, TEXT("Max number of prediction samples to debug draw. All prediction samples will be drawn if value is negative. (Default -1)"));
#endif

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

static FBoneIndexType GetRemappedBoneIndexType(FBoneIndexType BoneIndexType, const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton)
{
	// remapping BoneIndexType in case the skeleton used to store history (LastUpdateSkeleton) is different from BoneIndexSkeleton
	if (LastUpdateSkeleton != nullptr && LastUpdateSkeleton != BoneIndexSkeleton)
	{
		const FSkeletonRemapping& SkeletonRemapping = UE::Anim::FSkeletonRemappingRegistry::Get().GetRemapping(BoneIndexSkeleton, LastUpdateSkeleton);
		if (SkeletonRemapping.IsValid())
		{
			BoneIndexType = SkeletonRemapping.GetTargetSkeletonBoneIndex(BoneIndexType);
		}
	}

	return BoneIndexType;
}

static FComponentSpaceTransformIndex GetRemappedComponentSpaceTransformIndex(const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton, const FBoneToTransformMap& BoneToTransformMap, FBoneIndexType BoneIndexType, bool& bSuccess)
{
	check(BoneIndexType != WorldSpaceIndexType);

	FComponentSpaceTransformIndex BoneTransformIndex = FComponentSpaceTransformIndex(BoneIndexType);
	if (BoneTransformIndex != ComponentSpaceIndexType)
	{
		BoneTransformIndex = GetRemappedBoneIndexType(BoneTransformIndex, BoneIndexSkeleton, LastUpdateSkeleton);

		if (!BoneToTransformMap.IsEmpty())
		{
			if (const FComponentSpaceTransformIndex* FoundBoneTransformIndex = BoneToTransformMap.Find(BoneTransformIndex))
			{
				BoneTransformIndex = *FoundBoneTransformIndex;
			}
			else
			{
				BoneTransformIndex = RootBoneIndexType;
				bSuccess = false;
			}
		}
	}
	return BoneTransformIndex;
}

static bool LerpEntries(float Time, bool bExtrapolate, const FPoseHistoryEntry& PrevEntry, const FPoseHistoryEntry& NextEntry, const USkeleton* BoneIndexSkeleton, const USkeleton* LastUpdateSkeleton,
	const FBoneToTransformMap& BoneToTransformMap, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, FTransform& OutBoneTransform)
{
	bool bSuccess = true;

	const float Denominator = NextEntry.AccumulatedSeconds - PrevEntry.AccumulatedSeconds;
	float LerpValue = 0.f;
	if (!FMath::IsNearlyZero(Denominator))
	{
		const float Numerator = Time - PrevEntry.AccumulatedSeconds;
		LerpValue = bExtrapolate ? Numerator / Denominator : FMath::Clamp(Numerator / Denominator, 0.f, 1.f);
	}

	const FComponentSpaceTransformIndex BoneTransformIndex = GetRemappedComponentSpaceTransformIndex(BoneIndexSkeleton, LastUpdateSkeleton, BoneToTransformMap, BoneIndexType, bSuccess);
	const FComponentSpaceTransformIndex ReferenceBoneTransformIndex = GetRemappedComponentSpaceTransformIndex(BoneIndexSkeleton, LastUpdateSkeleton, BoneToTransformMap, ReferenceBoneIndexType, bSuccess);

	if (BoneTransformIndex != ComponentSpaceIndexType)
	{
		if (ReferenceBoneTransformIndex == ComponentSpaceIndexType)
		{
			if (FMath::IsNearlyZero(LerpValue, ZERO_ANIMWEIGHT_THRESH))
			{
				OutBoneTransform = PrevEntry.GetComponentSpaceTransform(BoneTransformIndex);
			}
			else if (FMath::IsNearlyZero(LerpValue - 1.f, ZERO_ANIMWEIGHT_THRESH))
			{
				OutBoneTransform = NextEntry.GetComponentSpaceTransform(BoneTransformIndex);
			}
			else
			{
				OutBoneTransform.Blend(
					PrevEntry.GetComponentSpaceTransform(BoneTransformIndex),
					NextEntry.GetComponentSpaceTransform(BoneTransformIndex),
					LerpValue);
			}
		}
		else
		{
			if (FMath::IsNearlyZero(LerpValue, ZERO_ANIMWEIGHT_THRESH))
			{
				OutBoneTransform = PrevEntry.GetComponentSpaceTransform(BoneTransformIndex) * PrevEntry.GetComponentSpaceTransform(ReferenceBoneTransformIndex).Inverse();
			}
			else if (FMath::IsNearlyZero(LerpValue - 1.f, ZERO_ANIMWEIGHT_THRESH))
			{
				OutBoneTransform = NextEntry.GetComponentSpaceTransform(BoneTransformIndex) * NextEntry.GetComponentSpaceTransform(ReferenceBoneTransformIndex).Inverse();
			}
			else
			{
				OutBoneTransform.Blend(
					PrevEntry.GetComponentSpaceTransform(BoneTransformIndex) * PrevEntry.GetComponentSpaceTransform(ReferenceBoneTransformIndex).Inverse(),
					NextEntry.GetComponentSpaceTransform(BoneTransformIndex) * NextEntry.GetComponentSpaceTransform(ReferenceBoneTransformIndex).Inverse(),
					LerpValue);
			}
		}
	}
	else
	{
		OutBoneTransform = FTransform::Identity;
		bSuccess = false;
		unimplemented();
	}

	return bSuccess;
}

//////////////////////////////////////////////////////////////////////////
// FPoseHistoryEntry
void FPoseHistoryEntry::Update(float Time, FCSPose<FCompactPose>& ComponentSpacePose, const FBoneToTransformMap& BoneToTransformMap, bool bStoreScales)
{
	AccumulatedSeconds = Time;

	const FBoneContainer& BoneContainer = ComponentSpacePose.GetPose().GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	check(SkeletonAsset);
	const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
	const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();

	if (BoneToTransformMap.IsEmpty())
	{
		// no mapping: we add all the transforms
		SetNum(NumSkeletonBones, bStoreScales);
		for (FSkeletonPoseBoneIndex SkeletonBoneIdx(0); SkeletonBoneIdx != NumSkeletonBones; ++SkeletonBoneIdx)
		{
			const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
			SetComponentSpaceTransform(SkeletonBoneIdx.GetInt(), (CompactBoneIdx.IsValid() ? ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIdx) : RefBonePose[SkeletonBoneIdx.GetInt()]));
		}
	}
	else
	{
		SetNum(BoneToTransformMap.Num(), true);
		for (const FBoneToTransformPair& BoneToTransformPair : BoneToTransformMap)
		{
			const FSkeletonPoseBoneIndex SkeletonBoneIdx(BoneToTransformPair.Key);
			const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
			SetComponentSpaceTransform(BoneToTransformPair.Value, (CompactBoneIdx.IsValid() ? ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIdx) : RefBonePose[SkeletonBoneIdx.GetInt()]));
		}
	}
}

void FPoseHistoryEntry::SetNum(int32 Num, bool bStoreScales)
{
	ComponentSpaceRotations.SetNum(Num);
	ComponentSpacePositions.SetNum(Num);
	ComponentSpaceScales.SetNum(bStoreScales ? Num : 0);
}

int32 FPoseHistoryEntry::Num() const
{
	return ComponentSpaceRotations.Num();
}

void FPoseHistoryEntry::SetComponentSpaceTransform(int32 Index, const FTransform& Transform)
{
	ComponentSpaceRotations[Index] = FQuat4f(Transform.GetRotation());
	ComponentSpacePositions[Index] = Transform.GetTranslation();
	
	if (!ComponentSpaceScales.IsEmpty())
	{
		ComponentSpaceScales[Index] = FVector3f(Transform.GetScale3D());
	}
}

FTransform FPoseHistoryEntry::GetComponentSpaceTransform(int32 Index) const
{
#if WITH_EDITOR
	if (Index < 0 || Index >= ComponentSpaceRotations.Num())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseHistoryEntry::GetComponentSpaceTransform - Index %d out of bound [0, %d)"), Index, ComponentSpaceRotations.Num());
		return FTransform::Identity;
	}
#endif // WITH_EDITOR

	check(ComponentSpaceScales.IsEmpty() || ComponentSpaceRotations.Num() == ComponentSpaceScales.Num());

	const FQuat Quat(ComponentSpaceRotations[Index]);
	const FVector Scale(ComponentSpaceScales.IsEmpty() ? FVector3f::OneVector : ComponentSpaceScales[Index]);
	return FTransform(Quat, ComponentSpacePositions[Index], Scale);
}

FArchive& operator<<(FArchive& Ar, FPoseHistoryEntry& Entry)
{
	Ar << Entry.ComponentSpaceRotations;
	Ar << Entry.ComponentSpacePositions;
	Ar << Entry.ComponentSpaceScales;
	Ar << Entry.AccumulatedSeconds;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// IPoseHistory
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void IPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color, float Time, float PointSize, bool bExtrapolate) const
{
	const FBoneContainer& BoneContainer = AnimInstanceProxy.GetRequiredBones();
	if (Color.A > 0 && BoneContainer.IsValid())
	{
		const USkeleton* Skeleton = BoneContainer.GetSkeletonAsset();
		FTransform OutBoneTransform;

		const FBoneToTransformMap& BoneToTransformMap = GetBoneToTransformMap();
		if (BoneToTransformMap.IsEmpty())
		{
			for (FSkeletonPoseBoneIndex SkeletonBoneIdx(0); SkeletonBoneIdx != BoneContainer.GetNumBones(); ++SkeletonBoneIdx)
			{
				const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
				if (GetTransformAtTime(Time, OutBoneTransform, Skeleton, CompactBoneIdx.GetInt(), WorldSpaceIndexType, bExtrapolate))
				{
					AnimInstanceProxy.AnimDrawDebugPoint(OutBoneTransform.GetTranslation(), PointSize, Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}
			}
		}
		else
		{
			for (const FBoneToTransformPair& BoneToTransformPair : BoneToTransformMap)
			{
				const FSkeletonPoseBoneIndex SkeletonBoneIdx(BoneToTransformPair.Key);
				const FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
				if (GetTransformAtTime(Time, OutBoneTransform, Skeleton, CompactBoneIdx.GetInt(), WorldSpaceIndexType, bExtrapolate))
				{
					AnimInstanceProxy.AnimDrawDebugPoint(OutBoneTransform.GetTranslation(), PointSize, Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FArchivedPoseHistory
void FArchivedPoseHistory::InitFrom(const IPoseHistory* PoseHistory)
{
	Trajectory.Samples.Reset();
	BoneToTransformMap.Reset();
	Entries.Reset();

	if (PoseHistory)
	{
		Trajectory = PoseHistory->GetTrajectory();
		BoneToTransformMap = PoseHistory->GetBoneToTransformMap();
			
		const int32 NumEntries = PoseHistory->GetNumEntries();
		Entries.SetNum(NumEntries);

		for (int32 EntryIndex = 0; EntryIndex < NumEntries; ++EntryIndex)
		{
			Entries[EntryIndex] = PoseHistory->GetEntry(EntryIndex);
			// validating input PoseHistory to have Entries properly sorted by time
			check(EntryIndex == 0 || (Entries[EntryIndex - 1].AccumulatedSeconds <= Entries[EntryIndex].AccumulatedSeconds));
		}
	}
}

bool FArchivedPoseHistory::GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, bool bExtrapolate) const
{
	static_assert(RootBoneIndexType == 0 && ComponentSpaceIndexType == FBoneIndexType(-1) && WorldSpaceIndexType == FBoneIndexType(-2)); // some assumptions
	check(BoneIndexType != ComponentSpaceIndexType && BoneIndexType != WorldSpaceIndexType);
	
	bool bSuccess = false;
	
	const bool bApplyComponentToWorld = ReferenceBoneIndexType == WorldSpaceIndexType;
	FTransform ComponentToWorld = FTransform::Identity;
	if (bApplyComponentToWorld)
	{
		ComponentToWorld = Trajectory.GetSampleAtTime(Time, bExtrapolate).GetTransform();
		ReferenceBoneIndexType = ComponentSpaceIndexType;
	}

	const int32 NumEntries = Entries.Num();
	if (NumEntries > 0)
	{
		int32 NextIdx = 0;
		int32 PrevIdx = 0;

		if (NumEntries > 1)
		{
			const int32 LowerBoundIdx = Algo::LowerBound(Entries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
			NextIdx = FMath::Clamp(LowerBoundIdx, 1, NumEntries - 1);
			PrevIdx = NextIdx - 1;
		}
	
		const FPoseHistoryEntry& PrevEntry = Entries[PrevIdx];
		const FPoseHistoryEntry& NextEntry = Entries[NextIdx];

		bSuccess = LerpEntries(Time, bExtrapolate, PrevEntry, NextEntry, BoneIndexSkeleton, nullptr, BoneToTransformMap, BoneIndexType, ReferenceBoneIndexType, OutBoneTransform);
		if (bApplyComponentToWorld)
		{
			OutBoneTransform *= ComponentToWorld;
		}
	}
	else
	{
		OutBoneTransform = ComponentToWorld;
	}
	
	return bSuccess;
}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FArchivedPoseHistory::DebugDraw(const UWorld* World, FColor Color) const
{
	if (Color.A > 0 && !Trajectory.Samples.IsEmpty())
	{
		TArray<FTransform, TInlineAllocator<128>> PrevGlobalTransforms;

		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			const FPoseHistoryEntry& Entry = Entries[EntryIndex];

			const int32 PrevGlobalTransformsNum = PrevGlobalTransforms.Num();
			const int32 Max = FMath::Max(PrevGlobalTransformsNum, Entry.Num());

			PrevGlobalTransforms.SetNum(Max, EAllowShrinking::No);

			const bool bIsCurrentTimeEntry = FMath::IsNearlyZero(Entry.AccumulatedSeconds);

			for (int32 i = 0; i < Entry.Num(); ++i)
			{
				const FTransform RootTransform = Trajectory.GetSampleAtTime(Entry.AccumulatedSeconds).GetTransform();
				const FTransform GlobalTransforms = Entry.GetComponentSpaceTransform(i) * RootTransform;

				if (i < PrevGlobalTransformsNum)
				{
					DrawDebugLine(World, PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, -1.f, SDPG_Foreground);
				}

				if (bIsCurrentTimeEntry)
				{
					DrawDebugPoint(World, GlobalTransforms.GetTranslation(), 6.f, Color, false, -1.f, SDPG_Foreground);

					if (i == 0)
					{
						DrawDebugLine(World, GlobalTransforms.GetTranslation(), GlobalTransforms.GetTranslation() + RootTransform.GetUnitAxis(EAxis::Type::X) * 25.f, FColor::Black, false, -1.f, SDPG_Foreground);
						DrawDebugLine(World, GlobalTransforms.GetTranslation(), GlobalTransforms.GetTranslation() + GlobalTransforms.GetUnitAxis(EAxis::Type::X) * 20.f, FColor::White, false, -1.f, SDPG_Foreground);
					}
				}

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

FArchive& operator<<(FArchive& Ar, FArchivedPoseHistory& Entry)
{
	Ar << Entry.BoneToTransformMap;
	Ar << Entry.Entries;
	Ar << Entry.Trajectory;
	return Ar;
}

//////////////////////////////////////////////////////////////////////////
// FPoseHistory
FPoseHistory::FPoseHistory(const FPoseHistory& Other)
	: FPoseHistory()
{
	*this = Other;
}

FPoseHistory::FPoseHistory(FPoseHistory&& Other)
	: FPoseHistory()
{
	*this = MoveTemp(Other);
}

FPoseHistory& FPoseHistory::operator=(const FPoseHistory& Other)
{
	CheckThreadSafetyWrite(ReadDataThreadSafeCounter);
	CheckThreadSafetyWrite(WriteDataThreadSafeCounter);

	ReadData = Other.ReadData;
	WriteData = Other.WriteData;
	return *this;
}

FPoseHistory& FPoseHistory::operator=(FPoseHistory&& Other)
{
	CheckThreadSafetyWrite(ReadDataThreadSafeCounter);
	CheckThreadSafetyWrite(WriteDataThreadSafeCounter);

	ReadData = MoveTemp(Other.ReadData);
	WriteData = MoveTemp(Other.WriteData);
	return *this;
}

void FPoseHistory::Initialize_AnyThread(int32 InNumPoses, float InSamplingInterval)
{
	CheckThreadSafetyWrite(WriteDataThreadSafeCounter);
	check(InNumPoses >= 2);

	MaxNumPoses = InNumPoses;
	SamplingInterval = InSamplingInterval;
}

void FPoseHistory::CacheBones_AnyThread(const TArray<FBoneIndexType>& RequiredBones)
{
	CheckThreadSafetyWrite(WriteDataThreadSafeCounter);

	check(MaxNumPoses >= 2);

	WriteData.BoneToTransformMap.Reset();
	if (!RequiredBones.IsEmpty())
	{
		// making sure we always collect the root bone transform (by construction BoneToTransformMap[0] = 0)
		WriteData.BoneToTransformMap.Add(RootBoneIndexType) = WriteData.BoneToTransformMap.Num();

		for (int32 i = 0; i < RequiredBones.Num(); ++i)
		{
			// adding only unique RequiredBones to avoid oversizing Entries::ComponentSpaceTransforms
			if (!WriteData.BoneToTransformMap.Find(RequiredBones[i]))
			{
				WriteData.BoneToTransformMap.Add(RequiredBones[i]) = WriteData.BoneToTransformMap.Num();
			}
		}
	}

	WriteData.Entries.Reset();
	WriteData.Entries.Reserve(MaxNumPoses);
}

bool FPoseHistory::GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, bool bExtrapolate) const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);

	static_assert(RootBoneIndexType == 0 && ComponentSpaceIndexType == FBoneIndexType(-1) && WorldSpaceIndexType == FBoneIndexType(-2)); // some assumptions
	check(BoneIndexType != ComponentSpaceIndexType && BoneIndexType != WorldSpaceIndexType);
	
	bool bSuccess = false;
	
	const bool bApplyComponentToWorld = ReferenceBoneIndexType == WorldSpaceIndexType;
	FTransform ComponentToWorld = FTransform::Identity;
	if (bApplyComponentToWorld)
	{
		ComponentToWorld = Trajectory.GetSampleAtTime(Time, bExtrapolate).GetTransform();
		ReferenceBoneIndexType = ComponentSpaceIndexType;
	}

	const int32 NumEntries = ReadData.Entries.Num();
	if (NumEntries > 0)
	{
		int32 NextIdx = 0;
		int32 PrevIdx = 0;

		if (NumEntries > 1)
		{
			const int32 LowerBoundIdx = LowerBound(ReadData.Entries.begin(), ReadData.Entries.end(), Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
			NextIdx = FMath::Clamp(LowerBoundIdx, 1, NumEntries - 1);
			PrevIdx = NextIdx - 1;
		}
	
		const FPoseHistoryEntry& PrevEntry = ReadData.Entries[PrevIdx];
		const FPoseHistoryEntry& NextEntry = ReadData.Entries[NextIdx];

		bSuccess = LerpEntries(Time, bExtrapolate, PrevEntry, NextEntry, BoneIndexSkeleton, ReadData.LastUpdateSkeleton.Get(), ReadData.BoneToTransformMap, BoneIndexType, ReferenceBoneIndexType, OutBoneTransform);
		if (bApplyComponentToWorld)
		{
			OutBoneTransform *= ComponentToWorld;
		}
	}
	else
	{
		OutBoneTransform = ComponentToWorld;
	}
	
	return bSuccess;
}

const FPoseSearchQueryTrajectory& FPoseHistory::GetTrajectory() const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);
	return Trajectory;
}

float FPoseHistory::GetTrajectorySpeedMultiplier() const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);
	return TrajectorySpeedMultiplier;
}

bool FPoseHistory::IsEmpty() const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);
	return ReadData.Entries.IsEmpty();
}

const FBoneToTransformMap& FPoseHistory::GetBoneToTransformMap() const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);
	return ReadData.BoneToTransformMap;
}

int32 FPoseHistory::GetNumEntries() const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);
	return ReadData.Entries.Num();
}

const FPoseHistoryEntry& FPoseHistory::GetEntry(int32 EntryIndex) const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);
	return ReadData.Entries[EntryIndex];
}

void FPoseHistory::PreUpdate(const UAnimInstance* AnimInstance, float DeltaTime, bool bGenerateTrajectory, const FPoseSearchTrajectoryData& TrajectoryData, const FPoseSearchTrajectoryData::FSampling& TrajectoryDataSampling, bool bNeedsReset)
{
	// we're writing both ReadData, WriteData. checking for thread safety
	CheckThreadSafetyWrite(ReadDataThreadSafeCounter);
	CheckThreadSafetyWrite(WriteDataThreadSafeCounter);

	check(IsInGameThread());

	if (bNeedsReset)
	{
		WriteData.Entries.Reset();
	}

	if (bGenerateTrajectory)
	{
		// @todo: Synchronize the FPoseSearchQueryTrajectorySample::AccumulatedSeconds of the generated trajectory with the FPoseHistoryEntry::AccumulatedSeconds of the captured poses
		FPoseSearchTrajectoryData::FDerived TrajectoryDataDerived;
		TrajectoryData.UpdateData(DeltaTime, AnimInstance, TrajectoryDataDerived, TrajectoryDataState);
		UPoseSearchTrajectoryLibrary::InitTrajectorySamples(Trajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, DeltaTime);
		UPoseSearchTrajectoryLibrary::UpdateHistory_TransformHistory(Trajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, DeltaTime);
		UPoseSearchTrajectoryLibrary::UpdatePrediction_SimulateCharacterMovement(Trajectory, TrajectoryData, TrajectoryDataDerived, TrajectoryDataSampling, DeltaTime);

		// @todo: support TrajectorySpeedMultiplier
		//TrajectorySpeedMultiplier = 1.f;
	}
	
	ReadData = WriteData;
}

void FPoseHistory::SetTrajectory(const FPoseSearchQueryTrajectory& InTrajectory, float InTrajectorySpeedMultiplier)
{
	if (!InTrajectory.Samples.IsEmpty())
	{
		CheckThreadSafetyWrite(ReadDataThreadSafeCounter);

		// @todo: THIS IS NOT THREAD SAFE! in the contex of multi character motion matching (CheckThreadSafetyWrite will assert in case of improper usage)
		Trajectory = InTrajectory;

		TrajectorySpeedMultiplier = InTrajectorySpeedMultiplier;

		if (!FMath::IsNearlyEqual(TrajectorySpeedMultiplier, 1.f))
		{
			const float TrajectorySpeedMultiplierInv = FMath::IsNearlyZero(TrajectorySpeedMultiplier) ? 1.f : 1.f / TrajectorySpeedMultiplier;
			for (FPoseSearchQueryTrajectorySample& Sample : Trajectory.Samples)
			{
				Sample.AccumulatedSeconds *= TrajectorySpeedMultiplierInv;
			}
		}
	}
}

void FPoseHistory::EvaluateComponentSpace_AnyThread(float DeltaTime, FCSPose<FCompactPose>& ComponentSpacePose, bool bStoreScales, float RootBoneRecoveryTime)
{
	CheckThreadSafetyWrite(WriteDataThreadSafeCounter);

	check(MaxNumPoses >= 2);

	const USkeleton* Skeleton = ComponentSpacePose.GetPose().GetBoneContainer().GetSkeletonAsset();
	check(Skeleton);

	if (WriteData.LastUpdateSkeleton != Skeleton)
	{
		// @todo: support a different USkeleton per FPoseHistoryEntry if required
		WriteData.Entries.Reset();
		WriteData.LastUpdateSkeleton = Skeleton;
	}

	FPoseHistoryEntry FutureEntryTemp;
	if (!WriteData.Entries.IsEmpty() && WriteData.Entries.Last().AccumulatedSeconds > 0.f)
	{
		// removing the "future" root bone Entry
		FutureEntryTemp = MoveTemp(WriteData.Entries.Last());
		WriteData.Entries.Pop();
	}

	// Age our elapsed times
	for (FPoseHistoryEntry& Entry : WriteData.Entries)
	{
		Entry.AccumulatedSeconds -= DeltaTime;
	}

	if (WriteData.Entries.Num() != MaxNumPoses)
	{
		// Consume every pose until the queue is full
		WriteData.Entries.Emplace();
	}
	else if (SamplingInterval <= 0.f || WriteData.Entries[WriteData.Entries.Num() - 2].AccumulatedSeconds <= -SamplingInterval)
	{
		FPoseHistoryEntry EntryTemp = MoveTemp(WriteData.Entries.First());
		WriteData.Entries.PopFront();
		WriteData.Entries.Emplace(MoveTemp(EntryTemp));
	}

	// Regardless of the retention policy, we always update the most recent Entry
	WriteData.Entries.Last().Update(0.f, ComponentSpacePose, WriteData.BoneToTransformMap, bStoreScales);

	if (RootBoneRecoveryTime > 0.f)
	{
		// adding the updated "future" root bone Entry
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
		const FTransform& RefRootBone = RefBonePose[RootBoneIndexType];

		FutureEntryTemp.SetNum(1, bStoreScales);
		FutureEntryTemp.SetComponentSpaceTransform(RootBoneIndexType, RefRootBone);
		FutureEntryTemp.AccumulatedSeconds = RootBoneRecoveryTime;
		WriteData.Entries.Emplace(MoveTemp(FutureEntryTemp));
	}
}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const
{
	CheckThreadSafetyRead(ReadDataThreadSafeCounter);

	if (CVarAnimPoseHistoryDebugDrawTrajectory.GetValueOnAnyThread())
	{
		const float DebugThickness = CVarAnimPoseHistoryDebugDrawTrajectoryThickness.GetValueOnAnyThread();
		const int MaxHistorySamples = CVarAnimPoseHistoryDebugDrawTrajectoryMaxNumOfHistorySamples.GetValueOnAnyThread();
		const int MaxPredictionSamples = CVarAnimPoseHistoryDebugDrawTrajectoryMaxNumOfPredictionSamples.GetValueOnAnyThread();
		Trajectory.DebugDrawTrajectory(AnimInstanceProxy, DebugThickness, 0, MaxHistorySamples, MaxPredictionSamples);
	}

	if (Color.A > 0 && CVarAnimPoseHistoryDebugDrawPose.GetValueOnAnyThread())
	{
		const bool bValidTrajectory = !Trajectory.Samples.IsEmpty();
		TArray<FTransform, TInlineAllocator<128>> PrevGlobalTransforms;

		for (int32 EntryIndex = 0; EntryIndex < ReadData.Entries.Num(); ++EntryIndex)
		{
			const FPoseHistoryEntry& Entry = ReadData.Entries[EntryIndex];

			const int32 PrevGlobalTransformsNum = PrevGlobalTransforms.Num();
			const int32 Max = FMath::Max(PrevGlobalTransformsNum, Entry.Num());

			PrevGlobalTransforms.SetNum(Max, EAllowShrinking::No);

			for (int32 i = 0; i < Entry.Num(); ++i)
			{
				const FTransform RootTransform = bValidTrajectory ? Trajectory.GetSampleAtTime(Entry.AccumulatedSeconds).GetTransform() : AnimInstanceProxy.GetComponentTransform();
				const FTransform GlobalTransforms = Entry.GetComponentSpaceTransform(i) * RootTransform;

				if (i < PrevGlobalTransformsNum)
				{
					AnimInstanceProxy.AnimDrawDebugLine(PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}
	}
}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

//////////////////////////////////////////////////////////////////////////
// FMemStackPoseHistory
void FMemStackPoseHistory::Init(const IPoseHistory* InPoseHistory)
{
	check(InPoseHistory);
	PoseHistory = InPoseHistory;
}

bool FMemStackPoseHistory::GetTransformAtTime(float Time, FTransform& OutBoneTransform, const USkeleton* BoneIndexSkeleton, FBoneIndexType BoneIndexType, FBoneIndexType ReferenceBoneIndexType, bool bExtrapolate) const
{
	check(PoseHistory);
	if (Time > 0.f)
	{
		const int32 Num = FutureEntries.Num();
		if (Num > 0)
		{
			const bool bApplyComponentToWorld = ReferenceBoneIndexType == WorldSpaceIndexType;
			FTransform ComponentToWorld = FTransform::Identity;
			if (bApplyComponentToWorld)
			{
				ComponentToWorld = GetTrajectory().GetSampleAtTime(Time, bExtrapolate).GetTransform();
				ReferenceBoneIndexType = ComponentSpaceIndexType;
			}

			const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
			const int32 NextIdx = FMath::Min(LowerBoundIdx, Num - 1);
			const FPoseHistoryEntry& NextEntry = FutureEntries[NextIdx];
			const FPoseHistoryEntry& PrevEntry = NextIdx > 0 ? FutureEntries[NextIdx - 1] : PoseHistory->GetNumEntries() > 0 ? PoseHistory->GetEntry(PoseHistory->GetNumEntries() - 1) : NextEntry;
						
			const bool bSuccess = LerpEntries(Time, bExtrapolate, PrevEntry, NextEntry, BoneIndexSkeleton, nullptr, GetBoneToTransformMap(), BoneIndexType, ReferenceBoneIndexType, OutBoneTransform);
			if (bApplyComponentToWorld)
			{
				OutBoneTransform *= ComponentToWorld;
			}
			return bSuccess;
		}
	}
	
	return PoseHistory->GetTransformAtTime(Time, OutBoneTransform, BoneIndexSkeleton, BoneIndexType, ReferenceBoneIndexType, bExtrapolate);
}

void FMemStackPoseHistory::AddFutureRootBone(float Time, const FTransform& FutureRootBoneTransform, bool bStoreScales)
{
	// we don't allow to add "past" or "present" poses to FutureEntries
	check(Time > 0.f);

	const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
	FPoseHistoryEntry& FutureEntry = FutureEntries.InsertDefaulted_GetRef(LowerBoundIdx);
	FutureEntry.SetNum(1, bStoreScales);
	FutureEntry.SetComponentSpaceTransform(RootBoneIndexType, FutureRootBoneTransform);
	FutureEntry.AccumulatedSeconds = Time;
}

void FMemStackPoseHistory::AddFuturePose(float Time, FCSPose<FCompactPose>& ComponentSpacePose)
{
	// we don't allow to add "past" or "present" poses to FutureEntries
	check(Time > 0.f);
	check(PoseHistory);	
	const int32 LowerBoundIdx = Algo::LowerBound(FutureEntries, Time, [](const FPoseHistoryEntry& Entry, float Value) { return Value > Entry.AccumulatedSeconds; });
	FutureEntries.InsertDefaulted_GetRef(LowerBoundIdx).Update(Time, ComponentSpacePose, GetBoneToTransformMap(), true);
}

int32 FMemStackPoseHistory::GetNumEntries() const
{
	check(PoseHistory);
	return PoseHistory->GetNumEntries() + FutureEntries.Num();
}

const FPoseHistoryEntry& FMemStackPoseHistory::GetEntry(int32 EntryIndex) const
{
	check(PoseHistory);

	const int32 PoseHistoryNumEntries = PoseHistory->GetNumEntries();
	if (EntryIndex < PoseHistoryNumEntries)
	{
		return PoseHistory->GetEntry(EntryIndex);
	}
	return FutureEntries[EntryIndex - PoseHistoryNumEntries];
}


#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
void FMemStackPoseHistory::DebugDraw(FAnimInstanceProxy& AnimInstanceProxy, FColor Color) const
{
	check(PoseHistory);

	if (Color.A > 0 && !FutureEntries.IsEmpty() && CVarAnimPoseHistoryDebugDrawPose.GetValueOnAnyThread())
	{
		const FPoseSearchQueryTrajectory& Trajectory = GetTrajectory();
		const bool bValidTrajectory = !Trajectory.Samples.IsEmpty();
		TArray<FTransform, TInlineAllocator<128>> PrevGlobalTransforms;

		int32 EntriesNum = FutureEntries.Num();
		if (PoseHistory->GetNumEntries() > 0)
		{
			// connecting the future entries with the past entries
			++EntriesNum;
		}

		for (int32 EntryIndex = 0; EntryIndex < EntriesNum; ++EntryIndex)
		{
			const FPoseHistoryEntry& Entry = (EntryIndex == FutureEntries.Num()) ? PoseHistory->GetEntry(PoseHistory->GetNumEntries() - 1) : FutureEntries[EntryIndex];

			const int32 PrevGlobalTransformsNum = PrevGlobalTransforms.Num();
			const int32 Max = FMath::Max(PrevGlobalTransformsNum, Entry.Num());

			PrevGlobalTransforms.SetNum(Max, EAllowShrinking::No);

			for (int32 i = 0; i < Entry.Num(); ++i)
			{
				const FTransform RootTransform = bValidTrajectory ? Trajectory.GetSampleAtTime(Entry.AccumulatedSeconds).GetTransform() : AnimInstanceProxy.GetComponentTransform();
				const FTransform GlobalTransforms = Entry.GetComponentSpaceTransform(i) * RootTransform;

				if (i < PrevGlobalTransformsNum)
				{
					AnimInstanceProxy.AnimDrawDebugLine(PrevGlobalTransforms[i].GetTranslation(), GlobalTransforms.GetTranslation(), Color, false, 0.f, ESceneDepthPriorityGroup::SDPG_Foreground);
				}

				PrevGlobalTransforms[i] = GlobalTransforms;
			}
		}

		// no need to DebugDraw PoseHistory since it'll be drawn anyways by the history collectors
		//PoseHistory->DebugDraw(AnimInstanceProxy, Color);
	}
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
