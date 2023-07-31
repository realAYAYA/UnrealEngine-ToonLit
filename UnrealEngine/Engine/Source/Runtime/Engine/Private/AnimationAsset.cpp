// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimationAsset.h"
#include "Engine/AssetUserData.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AssetMappingTable.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "Animation/AnimInstance.h"
#include "UObject/LinkerLoad.h"
#include "Animation/BlendSpace.h"
#include "Animation/PoseAsset.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimNodeBase.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimationAsset)

#define LOCTEXT_NAMESPACE "AnimationAsset"

#define LEADERSCORE_ALWAYSLEADER  	2.f
#define LEADERSCORE_MONTAGE			3.f

FVector FRootMotionMovementParams::RootMotionScale(1.0f, 1.0f, 1.0f);
const TArray<FName> FMarkerTickContext::DefaultMarkerNames;

//////////////////////////////////////////////////////////////////////////
// FAnimGroupInstance

void FAnimGroupInstance::TestTickRecordForLeadership(EAnimGroupRole::Type MembershipType)
{
	check(ActivePlayers.Num() > 0);

	// always set leader score if you have potential to be leader
	// that way if the top leader fails, we'll continue to search next available leader
	int32 TestIndex = ActivePlayers.Num() - 1;
	FAnimTickRecord& Candidate = ActivePlayers[TestIndex];

	if(Candidate.SourceAsset->IsA<UAnimMontage>())
	{
		// if the candidate has higher weight
		if (Candidate.EffectiveBlendWeight > MontageLeaderWeight)
		{
			// if this is going to be leader, I'll clean ActivePlayers because we don't sync multi montages
			const int32 LastIndex = TestIndex - 1;
			if (LastIndex >= 0)
			{
				ActivePlayers.RemoveAt(TestIndex - 1, 1);
			}

			// at this time, it should only have one
			ensure(ActivePlayers.Num() == 1);

			// then override
			// @note : leader weight doesn't applied WITHIN montages
			// we still only contain one montage at a time, if this montage fails, next candidate will get the chance, not next weight montage
			MontageLeaderWeight = Candidate.EffectiveBlendWeight;
			Candidate.LeaderScore = LEADERSCORE_MONTAGE;
		}
		else
		{
			if (TestIndex != 0)
			{
				// we delete the later ones because we only have one montage for leader. 
				// this can happen if there was already active one with higher weight. 
				ActivePlayers.RemoveAt(TestIndex, 1);
			}
		}

		ensureAlways(ActivePlayers.Num() == 1);
	}
	else
	{
		switch (MembershipType)
		{
		case EAnimGroupRole::CanBeLeader:
		case EAnimGroupRole::TransitionLeader:
			Candidate.LeaderScore = Candidate.EffectiveBlendWeight;
			break;
		case EAnimGroupRole::AlwaysLeader:
			// Always set the leader index
			Candidate.LeaderScore = LEADERSCORE_ALWAYSLEADER;
			break;
		default:
		case EAnimGroupRole::AlwaysFollower:
		case EAnimGroupRole::TransitionFollower:
			// Never set the leader index; the actual tick code will handle the case of no leader by using the first element in the array
			break;
		}
	}
}

void FAnimGroupInstance::Finalize(const FAnimGroupInstance* PreviousGroup)
{
	if (!PreviousGroup || PreviousGroup->GroupLeaderIndex != GroupLeaderIndex
		|| (PreviousGroup->MontageLeaderWeight > 0.f && MontageLeaderWeight == 0.f/*if montage disappears, we should reset as well*/))
	{
		UE_LOG(LogAnimMarkerSync, Log, TEXT("Resetting Marker Sync Groups"));

		for (int32 RecordIndex = GroupLeaderIndex + 1; RecordIndex < ActivePlayers.Num(); ++RecordIndex)
		{
			ActivePlayers[RecordIndex].MarkerTickRecord->Reset();
		}
	}
}

void FAnimGroupInstance::Prepare(const FAnimGroupInstance* PreviousGroup)
{
	ActivePlayers.Sort();

	TArray<FName>* MarkerNames = ActivePlayers[0].SourceAsset->GetUniqueMarkerNames();
	if (MarkerNames)
	{
		// Group leader has markers, off to a good start
		ValidMarkers = *MarkerNames;
		ActivePlayers[0].bCanUseMarkerSync = true;
		bCanUseMarkerSync = true;

		//filter markers based on what exists in the other animations
		for ( int32 ActivePlayerIndex = 0; ActivePlayerIndex < ActivePlayers.Num(); ++ActivePlayerIndex )
		{
			FAnimTickRecord& Candidate = ActivePlayers[ActivePlayerIndex];

			if (PreviousGroup)
			{
				bool bCandidateFound = false;
				for (const FAnimTickRecord& PrevRecord : PreviousGroup->ActivePlayers)
				{
					if (PrevRecord.MarkerTickRecord == Candidate.MarkerTickRecord)
					{
						// Found previous record for "us"
						if (PrevRecord.SourceAsset != Candidate.SourceAsset)
						{
							Candidate.MarkerTickRecord->Reset(); // Changed animation, clear our cached data
						}
						bCandidateFound = true;
						break;
					}
				}
				if (!bCandidateFound)
				{
					Candidate.MarkerTickRecord->Reset(); // we weren't active last frame, reset
				}
			}

			if (ActivePlayerIndex != 0 && ValidMarkers.Num() > 0)
			{
				TArray<FName>* PlayerMarkerNames = Candidate.SourceAsset->GetUniqueMarkerNames();
				if ( PlayerMarkerNames ) // Let anims with no markers set use length scaling sync
				{
					Candidate.bCanUseMarkerSync = true;
					for ( int32 ValidMarkerIndex = ValidMarkers.Num() - 1; ValidMarkerIndex >= 0; --ValidMarkerIndex )
					{
						FName& MarkerName = ValidMarkers[ValidMarkerIndex];
						if ( !PlayerMarkerNames->Contains(MarkerName) )
						{
							ValidMarkers.RemoveAtSwap(ValidMarkerIndex, 1, false);
						}
					}
				}
			}
		}

		bCanUseMarkerSync = ValidMarkers.Num() > 0;

		ValidMarkers.Sort(FNameLexicalLess());

		if (!PreviousGroup || (ValidMarkers != PreviousGroup->ValidMarkers))
		{
			for (int32 InternalActivePlayerIndex = 0; InternalActivePlayerIndex < ActivePlayers.Num(); ++InternalActivePlayerIndex)
			{
				ActivePlayers[InternalActivePlayerIndex].MarkerTickRecord->Reset();
			}
		}
	}
	else
	{
		// Leader has no markers, we can't use SyncMarkers.
		bCanUseMarkerSync = false;
		ValidMarkers.Reset();
		for (FAnimTickRecord& AnimTickRecord : ActivePlayers)
		{
			AnimTickRecord.MarkerTickRecord->Reset();
		}
	}
}

void FAnimTickRecord::AllocateContextDataContainer()
{
	ContextData = MakeShared<TArray<TUniquePtr<const UE::Anim::IAnimNotifyEventContextDataInterface>>>();
}

FAnimTickRecord::FAnimTickRecord(UAnimSequenceBase* InSequence, bool bInLooping, float InPlayRate, float InFinalBlendWeight, float& InCurrentTime, FMarkerTickRecord& InMarkerTickRecord)
{
	SourceAsset = InSequence;
	TimeAccumulator = &InCurrentTime;
	MarkerTickRecord = &InMarkerTickRecord;
	PlayRateMultiplier = InPlayRate;
	EffectiveBlendWeight = InFinalBlendWeight;
	bLooping = bInLooping;
	BlendSpace.bIsEvaluator = false;	// HACK for 5.1.1 do allow us to fix UE-170739 without altering public API
}

FAnimTickRecord::FAnimTickRecord(
	UBlendSpace* InBlendSpace, const FVector& InBlendInput, TArray<FBlendSampleData>& InBlendSampleDataCache, FBlendFilter& InBlendFilter, bool bInLooping, 
	float InPlayRate, bool bTeleportToTime, bool bIsEvaluator, float InFinalBlendWeight, float& InCurrentTime, FMarkerTickRecord& InMarkerTickRecord)
{
	SourceAsset = InBlendSpace;
	BlendSpace.BlendSpacePositionX = InBlendInput.X;
	BlendSpace.BlendSpacePositionY = InBlendInput.Y;
	BlendSpace.BlendSampleDataCache = &InBlendSampleDataCache;
	BlendSpace.BlendFilter = &InBlendFilter;
	BlendSpace.bTeleportToTime = bTeleportToTime;
	BlendSpace.bIsEvaluator = bIsEvaluator;
	TimeAccumulator = &InCurrentTime;
	MarkerTickRecord = &InMarkerTickRecord;
	PlayRateMultiplier = InPlayRate;
	EffectiveBlendWeight = InFinalBlendWeight;
	bLooping = bInLooping;
}

FAnimTickRecord::FAnimTickRecord(UAnimMontage* InMontage, float InCurrentPosition, float, float, float InWeight, TArray<FPassedMarker>& InMarkersPassedThisTick, FMarkerTickRecord& InMarkerTickRecord)
	: FAnimTickRecord(InMontage, InCurrentPosition, InWeight, InMarkersPassedThisTick, InMarkerTickRecord)
{
}

FAnimTickRecord::FAnimTickRecord(UAnimMontage* InMontage, float InCurrentPosition, float InWeight, TArray<FPassedMarker>& InMarkersPassedThisTick, FMarkerTickRecord& InMarkerTickRecord)
{
	SourceAsset = InMontage;
	Montage.CurrentPosition = InCurrentPosition;
	Montage.MarkersPassedThisTick = &InMarkersPassedThisTick;
	MarkerTickRecord = &InMarkerTickRecord;
	PlayRateMultiplier = 1.f; // we don't care here, this is alreayd applied in the montageinstance::Advance
	EffectiveBlendWeight = InWeight;
	bLooping = false;
}

FAnimTickRecord::FAnimTickRecord(UPoseAsset* InPoseAsset, float InFinalBlendWeight)
{
	SourceAsset = InPoseAsset;
	EffectiveBlendWeight = InFinalBlendWeight;
}

void FAnimTickRecord::GatherContextData(const FAnimationUpdateContext& InContext)
{
	if(InContext.GetSharedContext())
	{
		TArray<TUniquePtr<const UE::Anim::IAnimNotifyEventContextDataInterface>> NewContextData;
		InContext.GetSharedContext()->MessageStack.MakeEventContextData(NewContextData);
		if(NewContextData.Num())
		{
			if (!ContextData.IsValid())
			{
				AllocateContextDataContainer();
			}

			ContextData->Append(MoveTemp(NewContextData));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// UAnimationAsset

void UAnimationAsset::PostLoad()
{
	LLM_SCOPE(ELLMTag::Animation);

	Super::PostLoad();

	// Load skeleton, to make sure anything accessing from PostLoad
	// skeleton is ready
	if (Skeleton)
	{
		if (FLinkerLoad* SkeletonLinker = Skeleton->GetLinker())
		{
			SkeletonLinker->Preload(Skeleton);
		}
		Skeleton->ConditionalPostLoad();
	}

#if WITH_EDITORONLY_DATA
	// Load Parent Asset, to make sure anything accessing from PostLoad has valid data to access
	if (ParentAsset)
	{
		if (FLinkerLoad* ParentAssetLinker = ParentAsset->GetLinker())
		{
			ParentAssetLinker->Preload(ParentAsset);
		}
		ParentAsset->ConditionalPostLoad();
	}
#endif

	ValidateSkeleton();

	check( Skeleton==NULL || SkeletonGuid.IsValid() );

#if WITH_EDITOR
	UpdateParentAsset();
#endif // WITH_EDITOR
}

void UAnimationAsset::ResetSkeleton(USkeleton* NewSkeleton)
{
// @TODO LH, I'd like this to work outside of editor, but that requires unlocking track names data in game
#if WITH_EDITOR
	Skeleton = NULL;
	ReplaceSkeleton(NewSkeleton);
#endif
}

void UAnimationAsset::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Animation);

	Super::Serialize(Ar);

	if (Ar.UEVer() >= VER_UE4_SKELETON_GUID_SERIALIZATION)
	{
		Ar << SkeletonGuid;
	}
}

UAnimMetaData* UAnimationAsset::FindMetaDataByClass(const TSubclassOf<UAnimMetaData> MetaDataClass) const
{
	UAnimMetaData* FoundMetaData = nullptr;

	if (UClass* TargetClass = MetaDataClass.Get())
	{
		for (UAnimMetaData* MetaDataInstance : MetaData)
		{
			if (MetaDataInstance && MetaDataInstance->IsA(TargetClass))
			{
				FoundMetaData = MetaDataInstance;
				break;
			}
		}
	}

	return FoundMetaData;
}

void UAnimationAsset::AddMetaData(UAnimMetaData* MetaDataInstance)
{
	MetaData.Add(MetaDataInstance);
}

void UAnimationAsset::RemoveMetaData(UAnimMetaData* MetaDataInstance)
{
	MetaData.Remove(MetaDataInstance);
}

void UAnimationAsset::RemoveMetaData(TArrayView<UAnimMetaData*> MetaDataInstances)
{
	MetaData.RemoveAll(
		[&](UAnimMetaData* MetaDataInstance)
	{
		return MetaDataInstances.Contains(MetaDataInstance);
	});
}

void UAnimationAsset::SetSkeleton(USkeleton* NewSkeleton)
{
	if (NewSkeleton && NewSkeleton != Skeleton)
	{
		Skeleton = NewSkeleton;
		SkeletonGuid = NewSkeleton->GetGuid();
	}
}

USkeletalMesh* UAnimationAsset::GetPreviewMesh(bool bFindIfNotSet)
{
#if WITH_EDITORONLY_DATA
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.LoadSynchronous();
	// if somehow skeleton changes, just nullify it. 
	if (PreviewMesh && PreviewMesh->GetSkeleton() != Skeleton)
	{
		PreviewMesh = nullptr;
		SetPreviewMesh(nullptr);
	}

	return PreviewMesh;
#else
	return nullptr;
#endif
}

USkeletalMesh* UAnimationAsset::GetPreviewMesh() const
{
#if WITH_EDITORONLY_DATA
	if (!PreviewSkeletalMesh.IsValid())
	{
		PreviewSkeletalMesh.LoadSynchronous();
	}
	return PreviewSkeletalMesh.Get();
#else
	return nullptr;
#endif
}

void UAnimationAsset::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty/*=true*/)
{
#if WITH_EDITORONLY_DATA
	if(bMarkAsDirty)
	{
		Modify();
	}
	PreviewSkeletalMesh = PreviewMesh;
#endif
}

#if WITH_EDITOR
void UAnimationAsset::RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces)
{
	SetSkeleton(NewSkeleton);
}

bool UAnimationAsset::ReplaceSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces/*=false*/)
{
	// if it's not same 
	if (NewSkeleton != Skeleton)
	{
		// get all sequences that need to change
		TArray<UAnimationAsset*> AnimAssetsToReplace;

		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(this))
		{
			AnimAssetsToReplace.AddUnique(AnimSequence);
		}
		if (GetAllAnimationSequencesReferred(AnimAssetsToReplace))
		{
			//Firstly need to remap
			for (UAnimationAsset* AnimAsset : AnimAssetsToReplace)
			{
				//Make sure animation has finished loading before we start messing with it
				if (FLinkerLoad* AnimLinker = AnimAsset->GetLinker())
				{
					AnimLinker->Preload(AnimAsset);
				}
				AnimAsset->ConditionalPostLoad();

				// This ensure that in subsequent behaviour the RawData GUID is never 'new-ed' but always calculated from the 
				// raw animation data itself.
				if (UAnimSequence* Sequence = Cast<UAnimSequence>(AnimAsset))
				{
					Sequence->GetController().OpenBracket(LOCTEXT("ReplaceSkeleton_Bracket", "Replacing USkeleton"));
				}

				// these two are different functions for now
				// technically if you have implementation for Remap, it will also set skeleton 
				AnimAsset->RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);
			}

			//Second need to process anim sequences themselves. This is done in two stages as additives can rely on other animations.
			for (UAnimationAsset* AnimAsset : AnimAssetsToReplace)
			{
				if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
				{
					Seq->GetController().CloseBracket();
				}
			}
		}

		UAnimSequence* Seq = Cast<UAnimSequence>(this);
		{			
			// This ensure that in subsequent behaviour the RawData GUID is never 'new-ed' but always calculated from the 
			// raw animation data itself.
			if (Seq)
			{
				Seq->GetController().OpenBracket(LOCTEXT("ReplaceSkeleton_Bracket", "Replacing USkeleton"));
			}
  
			RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);

			if (Seq)
			{
				Seq->GetController().CloseBracket();
			}
		}

		return true;
	}

	return false;
}

bool UAnimationAsset::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationSequences, bool bRecursive /*= true*/) 
{
	//@todo:@fixme: this doesn't work for retargeting because postload gets called after duplication, mixing up the mapping table
	// because skeleton changes, for now we don't support retargeting for parent asset, it will disconnect, and just duplicate everything else
// 	if (ParentAsset)
// 	{
// 		ParentAsset->HandleAnimReferenceCollection(AnimationSequences, bRecursive);
// 	}
// 
// 	if (AssetMappingTable)
// 	{
// 		AssetMappingTable->GetAllAnimationSequencesReferred(AnimationSequences, bRecursive);
// 	}

	return (AnimationSequences.Num() > 0);
}

void UAnimationAsset::HandleAnimReferenceCollection(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive)
{
	AnimationAssets.AddUnique(this);
	if (bRecursive)
	{
		// anim sequence still should call this. since bRecursive is true, I'm not sending the parameter with it
		GetAllAnimationSequencesReferred(AnimationAssets);
	}
}

void UAnimationAsset::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	//@todo:@fixme: this doesn't work for retargeting because postload gets called after duplication, mixing up the mapping table
	// because skeleton changes, for now we don't support retargeting for parent asset, it will disconnect, and just duplicate everything else
	if (ParentAsset)
	{
		// clear up, so that it doesn't try to use from other asset
		ParentAsset = nullptr;
		AssetMappingTable = nullptr;
	}

// 	if (ParentAsset)
// 	{
// 		// now fix everythign else
// 		UAnimationAsset* const* ReplacementAsset = ReplacementMap.Find(ParentAsset);
// 		if (ReplacementAsset)
// 		{
// 			ParentAsset = *ReplacementAsset;
// 			ParentAsset->ReplaceReferredAnimations(ReplacementMap);
// 		}
// 	}
// 
// 	if (AssetMappingTable)
// 	{
// 		AssetMappingTable->ReplaceReferredAnimations(ReplacementMap);
// 	}
}

void UAnimationAsset::UpdateParentAsset()
{
	ValidateParentAsset();

	if (ParentAsset)
	{
		TArray<UAnimationAsset*> AnimAssetsReferencedDirectly;
		if (ParentAsset->GetAllAnimationSequencesReferred(AnimAssetsReferencedDirectly, false))
		{
			AssetMappingTable->RefreshAssetList(AnimAssetsReferencedDirectly);
		}
	}
	else
	{
		// if somehow source data is gone, there is nothing much to do here
		ParentAsset = nullptr;
		AssetMappingTable = nullptr;
	}

	if (ParentAsset)
	{
		RefreshParentAssetData();
	}
}

void UAnimationAsset::ValidateParentAsset()
{
	if (ParentAsset)
	{
		if (ParentAsset->GetSkeleton() != GetSkeleton())
		{
			// parent asset chnaged skeleton, so we'll have to discard parent asset
			UE_LOG(LogAnimation, Warning, TEXT("%s: ParentAsset %s linked to different skeleton. Removing the reference."), *GetName(), *GetNameSafe(ParentAsset));
			ParentAsset = nullptr;
			Modify();
		}
		else if (ParentAsset->StaticClass() != StaticClass())
		{
			// parent asset chnaged skeleton, so we'll have to discard parent asset
			UE_LOG(LogAnimation, Warning, TEXT("%s: ParentAsset %s class type doesn't match. Removing the reference."), *GetName(), *GetNameSafe(ParentAsset));
			ParentAsset = nullptr;
			Modify();
		}
	}
}

void UAnimationAsset::RefreshParentAssetData()
{
	// should only allow within the same skeleton
	ParentAsset->ChildrenAssets.AddUnique(this);
	MetaData = ParentAsset->MetaData;
	PreviewPoseAsset = ParentAsset->PreviewPoseAsset;
	PreviewSkeletalMesh = ParentAsset->PreviewSkeletalMesh;
}

void UAnimationAsset::SetParentAsset(UAnimationAsset* InParentAsset)
{
	// only same class and same skeleton
	if (InParentAsset && InParentAsset->HasParentAsset() == false && 
		InParentAsset->StaticClass() == StaticClass() && InParentAsset->GetSkeleton() == GetSkeleton())
	{
		ParentAsset = InParentAsset;

		// if ParentAsset exists, just create mapping table.
		// it becomes messy if we only created when we have assets to map
		if (AssetMappingTable == nullptr)
		{
			AssetMappingTable = NewObject<UAssetMappingTable>(this);
		}
		else
		{
			AssetMappingTable->Clear();
		}

		UpdateParentAsset();
	}
	else
	{
		// otherwise, clear it
		ParentAsset = nullptr;
		AssetMappingTable = nullptr;
	}
}

bool UAnimationAsset::RemapAsset(UAnimationAsset* SourceAsset, UAnimationAsset* TargetAsset)
{
	if (AssetMappingTable)
	{
		if (AssetMappingTable->RemapAsset(SourceAsset, TargetAsset))
		{
			RefreshParentAssetData();
			return true;
		}
	}

	return false;
}
#endif

void UAnimationAsset::ValidateSkeleton()
{
	if (Skeleton && Skeleton->GetGuid() != SkeletonGuid)
	{
		// reset Skeleton
		ResetSkeleton(Skeleton);
		UE_LOG(LogAnimation, Verbose, TEXT("Needed to reset skeleton. Resave this asset to speed up load time: %s"), *GetPathNameSafe(this));
	}
}

void UAnimationAsset::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UAnimationAsset::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UAnimationAsset::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UAnimationAsset::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}

#if WITH_EDITOR
void UAnimationAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	for (UAssetUserData* Datum : AssetUserData)
	{
		if (Datum != nullptr)
		{
			Datum->PostEditChangeOwner();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

EDataValidationResult UAnimationAsset::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = UObject::IsDataValid(ValidationErrors);
	for (UAssetUserData* Datum : AssetUserData)
	{
		if(Datum != nullptr && Datum->IsDataValid(ValidationErrors) == EDataValidationResult::Invalid)
		{
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////
//
// FBlendSampleData
//
////////////////////////////////////////////////////////////////////////////////////////
void FBlendSampleData::NormalizeDataWeight(TArray<FBlendSampleData>& SampleDataList)
{
	float TotalSum = 0.f;

	check(SampleDataList.Num() > 0);
	const int32 NumBones = SampleDataList[0].PerBoneBlendData.Num();

	TArray<float> PerBoneTotalSums;
	PerBoneTotalSums.AddZeroed(NumBones);

	for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
	{
		checkf(SampleDataList[PoseIndex].PerBoneBlendData.Num() == NumBones, TEXT("Attempted to normalise a blend sample list, but the samples have differing numbers of bones."));

		TotalSum += SampleDataList[PoseIndex].TotalWeight;

		if (SampleDataList[PoseIndex].PerBoneBlendData.Num() > 0)
		{
			// now interpolate the per bone weights
			for (int32 BoneIndex = 0; BoneIndex<NumBones; BoneIndex++)
			{
				PerBoneTotalSums[BoneIndex] += SampleDataList[PoseIndex].PerBoneBlendData[BoneIndex];
			}
		}
	}

	// Re-normalize Pose weight
	if (TotalSum > ZERO_ANIMWEIGHT_THRESH)
	{
		if (FMath::Abs<float>(TotalSum - 1.f) > ZERO_ANIMWEIGHT_THRESH)
		{
			for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
			{
				SampleDataList[PoseIndex].TotalWeight /= TotalSum;
			}
		}
	}
	else
	{
		for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
		{
			SampleDataList[PoseIndex].TotalWeight = 1.0f / SampleDataList.Num();
		}
	}

	// Re-normalize per bone weights.
	for (int32 BoneIndex = 0; BoneIndex < NumBones; BoneIndex++)
	{
		if (PerBoneTotalSums[BoneIndex] > ZERO_ANIMWEIGHT_THRESH)
		{
			if (FMath::Abs<float>(PerBoneTotalSums[BoneIndex] - 1.f) > ZERO_ANIMWEIGHT_THRESH)
			{
				for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
				{
					SampleDataList[PoseIndex].PerBoneBlendData[BoneIndex] /= PerBoneTotalSums[BoneIndex];
				}
			}
		}
		else
		{
			for (int32 PoseIndex = 0; PoseIndex < SampleDataList.Num(); PoseIndex++)
			{
				SampleDataList[PoseIndex].PerBoneBlendData[BoneIndex] = 1.0f / SampleDataList.Num();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE // "AnimationAsset"
