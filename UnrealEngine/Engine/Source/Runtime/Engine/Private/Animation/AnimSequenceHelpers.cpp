// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"

#include "UObject/NameTypes.h"

#include "BonePose.h"
#include "BoneContainer.h"
#include "Animation/AnimTypes.h"

#include "AnimationRuntime.h"
#include "AnimEncoding.h"
#include "AnimationUtils.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Misc/MessageDialog.h"
#include "Animation/AnimSequenceHelpers.h"

#define LOCTEXT_NAMESPACE "AnimSequenceHelpers"

namespace UE
{

namespace Anim
{

struct FRetargetTracking
{
	const FCompactPoseBoneIndex PoseBoneIndex;
	const int32 SkeletonBoneIndex;

	FRetargetTracking(const FCompactPoseBoneIndex InPoseBoneIndex, const int32 InSkeletonBoneIndex)
		: PoseBoneIndex(InPoseBoneIndex), SkeletonBoneIndex(InSkeletonBoneIndex)
	{
	}
};

struct FBuildRawPoseScratchArea : public TThreadSingleton<FBuildRawPoseScratchArea>
{
	TArray<FRetargetTracking> RetargetTracking;
	TArray<FVirtualBoneCompactPoseData> VirtualBoneCompactPoseData;
};


#if WITH_EDITOR
FTransform ExtractTransformForKey(int32 Key, const FRawAnimSequenceTrack& TrackToExtract)
{
	static const FVector DefaultScale3D = FVector(1.f);
	const bool bHasScaleKey = TrackToExtract.ScaleKeys.Num() > 0;

	const int32 PosKeyIndex = FMath::Min(Key, TrackToExtract.PosKeys.Num() - 1);
	const int32 RotKeyIndex = FMath::Min(Key, TrackToExtract.RotKeys.Num() - 1);
	if (bHasScaleKey)
	{
		const int32 ScaleKeyIndex = FMath::Min(Key, TrackToExtract.ScaleKeys.Num() - 1);
		return FTransform(FQuat(TrackToExtract.RotKeys[RotKeyIndex]), FVector(TrackToExtract.PosKeys[PosKeyIndex]), FVector(TrackToExtract.ScaleKeys[ScaleKeyIndex]));
	}
	else
	{
		return FTransform(FQuat(TrackToExtract.RotKeys[RotKeyIndex]), FVector(TrackToExtract.PosKeys[PosKeyIndex]), FVector(DefaultScale3D));
	}
}

template<bool bInterpolateT>
void ExtractPoseFromModel(const TArray<FBoneAnimationTrack>& BoneAnimationTracks, const TMap<FName, FTransform>& OverrideBoneTransforms, FCompactPose& InOutPose, const int32 KeyIndex1, const int32 KeyIndex2, float Alpha, const USkeleton* SourceSkeleton)
{
	const int32 NumAnimationTracks = BoneAnimationTracks.Num();
	const FBoneContainer& RequiredBones = InOutPose.GetBoneContainer();

	TArray<UE::Anim::FRetargetTracking>& RetargetTracking = UE::Anim::FBuildRawPoseScratchArea::Get().RetargetTracking;
	RetargetTracking.Reset(NumAnimationTracks);

	TArray<FVirtualBoneCompactPoseData>& VBCompactPoseData = UE::Anim::FBuildRawPoseScratchArea::Get().VirtualBoneCompactPoseData;
	VBCompactPoseData = RequiredBones.GetVirtualBoneCompactPoseData();

	FCompactPose Key2Pose;
	if (bInterpolateT)
	{
		Key2Pose.CopyBonesFrom(InOutPose);
	}

	const USkeleton* TargetSkeleton = RequiredBones.GetSkeletonAsset();
	const FSkeletonRemapping* SkeletonRemapping = TargetSkeleton->GetSkeletonRemapping(SourceSkeleton);

	for (const FBoneAnimationTrack& AnimationTrack : BoneAnimationTracks)
	{
		const int32 SourceSkeletonBoneIndex = AnimationTrack.BoneTreeIndex;
		const int32 SkeletonBoneIndex = SkeletonRemapping ? SkeletonRemapping->GetTargetSkeletonBoneIndex(SourceSkeletonBoneIndex) : SourceSkeletonBoneIndex;

		// not sure it's safe to assume that SkeletonBoneIndex can never be INDEX_NONE
		if ((SkeletonBoneIndex != INDEX_NONE) && (SkeletonBoneIndex < MAX_BONES))
		{
			const FCompactPoseBoneIndex PoseBoneIndex = RequiredBones.GetCompactPoseIndexFromSkeletonIndex(SkeletonBoneIndex);
			if (PoseBoneIndex != INDEX_NONE)
			{
				for (int32 Idx = 0; Idx < VBCompactPoseData.Num(); ++Idx)
				{
					FVirtualBoneCompactPoseData& VB = VBCompactPoseData[Idx];
					if (PoseBoneIndex == VB.VBIndex)
					{
						// Remove this bone as we have written data for it (false so we dont resize allocation)
						VBCompactPoseData.RemoveAtSwap(Idx, 1, false);
						break; //Modified TArray so must break here
					}
				}
				
				// extract animation
				const FRawAnimSequenceTrack& TrackToExtract = AnimationTrack.InternalTrackData;
				const FTransform* OverrideTransform = OverrideBoneTransforms.Find(AnimationTrack.Name);
				{
					// Bail out (with rather wacky data) if data is empty for some reason.
					if (TrackToExtract.PosKeys.Num() == 0 || TrackToExtract.RotKeys.Num() == 0)
					{
						InOutPose[PoseBoneIndex].SetIdentity();

						if (bInterpolateT)
						{
							Key2Pose[PoseBoneIndex].SetIdentity();
						}
					}
					else
					{
						InOutPose[PoseBoneIndex] = UE::Anim::ExtractTransformForKey(KeyIndex1, TrackToExtract);

						if (bInterpolateT)
						{
							Key2Pose[PoseBoneIndex] = UE::Anim::ExtractTransformForKey(KeyIndex2, TrackToExtract);
						}
					}
				}

				if (OverrideTransform)
				{
					InOutPose[PoseBoneIndex].SetRotation(InOutPose[PoseBoneIndex].GetRotation() * OverrideTransform->GetRotation());
					InOutPose[PoseBoneIndex].TransformPosition(OverrideTransform->GetTranslation());
					InOutPose[PoseBoneIndex].SetScale3D(InOutPose[PoseBoneIndex].GetScale3D() * OverrideTransform->GetScale3D());

					if (bInterpolateT)
					{
						Key2Pose[PoseBoneIndex].SetRotation(Key2Pose[PoseBoneIndex].GetRotation() * OverrideTransform->GetRotation());
						Key2Pose[PoseBoneIndex].TransformPosition(OverrideTransform->GetTranslation());
						Key2Pose[PoseBoneIndex].SetScale3D(Key2Pose[PoseBoneIndex].GetScale3D() * OverrideTransform->GetScale3D());
					}
				}

				RetargetTracking.Add(UE::Anim::FRetargetTracking(PoseBoneIndex, SkeletonBoneIndex));
			}
		}
	}

	//Build Virtual Bones
	if (VBCompactPoseData.Num() > 0)
	{
		FCSPose<FCompactPose> CSPose1;
		CSPose1.InitPose(InOutPose);

		FCSPose<FCompactPose> CSPose2;
		if (bInterpolateT)
		{
			CSPose2.InitPose(Key2Pose);
		}

		for (FVirtualBoneCompactPoseData& VB : VBCompactPoseData)
		{
			FTransform Source = CSPose1.GetComponentSpaceTransform(VB.SourceIndex);
			FTransform Target = CSPose1.GetComponentSpaceTransform(VB.TargetIndex);
			InOutPose[VB.VBIndex] = Target.GetRelativeTransform(Source);

			if (bInterpolateT)
			{
				FTransform Source2 = CSPose2.GetComponentSpaceTransform(VB.SourceIndex);
				FTransform Target2 = CSPose2.GetComponentSpaceTransform(VB.TargetIndex);
				Key2Pose[VB.VBIndex] = Target2.GetRelativeTransform(Source2);
			}
		}
	}

	if (bInterpolateT)
	{
		for (FCompactPoseBoneIndex BoneIndex : InOutPose.ForEachBoneIndex())
		{
			InOutPose[BoneIndex].Blend(InOutPose[BoneIndex], Key2Pose[BoneIndex], Alpha);
		}
	}
}

void BuildPoseFromModel(const UAnimDataModel* Model, FCompactPose& OutPose, const float Time, const EAnimInterpolationType& InterpolationType, const FName& RetargetSource, const TArray<FTransform>& RetargetTransforms)
{
	check(Model);

	const int32 NumberOfKeys = Model->GetNumberOfKeys();
	const float SequenceLength = Model->GetPlayLength();
	const double FramesPerSecond = Model->GetFrameRate().AsDecimal();

	// Generate keys to interpolate between
	int32 KeyIndex1, KeyIndex2;
	float Alpha;
	FAnimationRuntime::GetKeyIndicesFromTime(KeyIndex1, KeyIndex2, Alpha, Time, NumberOfKeys, SequenceLength, FramesPerSecond);

	if (InterpolationType == EAnimInterpolationType::Step)
	{
		// Force stepping between keys
		Alpha = 0.f;
	}

	bool bShouldInterpolate = true;

	if (Alpha < UE_KINDA_SMALL_NUMBER)
	{
		Alpha = 0.f;
		bShouldInterpolate = false;
	}
	else if (Alpha > 1.f - UE_KINDA_SMALL_NUMBER)
	{
		bShouldInterpolate = false;
		KeyIndex1 = KeyIndex2;
	}

	TMap<FName, FTransform> ActiveCurves;
	if (!OutPose.GetBoneContainer().ShouldUseSourceData())
	{
		EvaluateTransformCurvesFromModel(Model, ActiveCurves, Time, 1.f);
	}

	const USkeleton* SourceSkeleton = Model->GetAnimationSequence()->GetSkeleton();
	if (bShouldInterpolate)
	{
		ExtractPoseFromModel<true>(Model->GetBoneAnimationTracks(), ActiveCurves, OutPose, KeyIndex1, KeyIndex2, Alpha, SourceSkeleton);
	}
	else
	{
		ExtractPoseFromModel<false>(Model->GetBoneAnimationTracks(), ActiveCurves, OutPose, KeyIndex1, KeyIndex2, Alpha, SourceSkeleton);
	}

	Retargeting::RetargetPose(OutPose, RetargetSource, RetargetTransforms);
}

void EvaluateFloatCurvesFromModel(const UAnimDataModel* Model, FBlendedCurve& OutCurves, float Time)
{
	check(Model);

	//SCOPE_CYCLE_COUNTER(STAT_EvalRawCurveData);
	if (OutCurves.NumValidCurveCount > 0)
	{
		// evaluate the curve data at the Time and add to Instance
		for (const FFloatCurve& Curve : Model->GetFloatCurves())
		{
			if (OutCurves.IsEnabled(Curve.Name.UID))
			{
				float Value = Curve.Evaluate(Time);
				OutCurves.Set(Curve.Name.UID, Value);
			}
		}
	}
}

void EvaluateTransformCurvesFromModel(const UAnimDataModel* Model, TMap<FName, FTransform>& OutCurves, float Time, float BlendWeight)
{
	if (Model)
	{
		for (const FTransformCurve& Curve : Model->GetTransformCurves())
		{
			// if disabled, do not handle
			if (Curve.GetCurveTypeFlag(AACF_Disabled))
			{
				continue;
			}

			// Add or retrieve curve
			const FName& CurveName = Curve.Name.DisplayName;

			// note we're not checking Curve.GetCurveTypeFlags() yet
			FTransform& Value = OutCurves.FindOrAdd(CurveName);
			Value = Curve.Evaluate(Time, BlendWeight);
		}
	}
}

void GetBoneTransformFromModel(const UAnimDataModel* Model, FTransform& OutTransform, int32 TrackIndex, float Time, const EAnimInterpolationType& Interpolation)
{
	const FBoneAnimationTrack& TrackData = Model->GetBoneTrackByIndex(TrackIndex);
	const FRawAnimSequenceTrack& RawTrack = TrackData.InternalTrackData;
	
	FAnimationUtils::ExtractTransformFromTrack(Time, Model->GetNumberOfKeys(), Model->GetPlayLength(), RawTrack, Interpolation, OutTransform);
}

void GetBoneTransformFromModel(const UAnimDataModel* Model, FTransform& OutTransform, int32 TrackIndex, int32 KeyIndex)
{
	const FBoneAnimationTrack& TrackData = Model->GetBoneTrackByIndex(TrackIndex);
	const FRawAnimSequenceTrack& RawTrack = TrackData.InternalTrackData;

	// Bail out (with rather wacky data) if data is empty for some reason.
	if (RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::GetBoneTransform : No anim data in AnimSequence!"));
		OutTransform.SetIdentity();
		return;
	}

	const int32 PosKeyIndex = FMath::Min(KeyIndex, RawTrack.PosKeys.Num() - 1);
	const int32 RotKeyIndex = FMath::Min(KeyIndex, RawTrack.RotKeys.Num() - 1);
	static const FVector DefaultScale3D = FVector(1.f);

	OutTransform.SetTranslation(FVector(RawTrack.PosKeys[PosKeyIndex]));
	OutTransform.SetRotation(FQuat(RawTrack.RotKeys[RotKeyIndex]));
	if (RawTrack.ScaleKeys.Num() > 0)
	{
		const int32 ScaleKeyIndex = FMath::Min(KeyIndex, RawTrack.ScaleKeys.Num() - 1);
		OutTransform.SetScale3D(FVector(RawTrack.ScaleKeys[ScaleKeyIndex]));
	}
	else
	{
		OutTransform.SetScale3D(DefaultScale3D);
	}

	for (const FTransformCurve& AdditiveTransformCurve : Model->GetTransformCurves())
	{
		if (AdditiveTransformCurve.Name.DisplayName == TrackData.Name)
		{
			const float TimeInterval = Model->GetFrameRate().AsSeconds(KeyIndex);
			const FTransform AdditiveTransform = AdditiveTransformCurve.Evaluate(TimeInterval, 1.f);
			const FTransform LocalTransform = OutTransform;
			OutTransform.SetRotation(LocalTransform.GetRotation() * AdditiveTransform.GetRotation());
			OutTransform.SetTranslation(LocalTransform.TransformPosition(AdditiveTransform.GetTranslation()));
			OutTransform.SetScale3D(LocalTransform.GetScale3D() * AdditiveTransform.GetScale3D());			
			break;
		}
	}
}

void CopyCurveDataToModel(const FRawCurveTracks& CurveData, const USkeleton* Skeleton, IAnimationDataController& Controller)
{
	// Populate float curve data
	for (const FFloatCurve& FloatCurve : CurveData.FloatCurves)
	{
		const FAnimationCurveIdentifier CurveId = UAnimationCurveIdentifierExtensions::FindCurveIdentifier(Skeleton, FloatCurve.Name.DisplayName, ERawCurveTrackTypes::RCT_Float);
		if (CurveId.IsValid())
		{
			Controller.AddCurve(CurveId, FloatCurve.GetCurveTypeFlags());
			Controller.SetCurveKeys(CurveId, FloatCurve.FloatCurve.GetConstRefOfKeys());
		}
	}

	// Populate transform curve data
	for (const FTransformCurve& TransformCurve : CurveData.TransformCurves)
	{
		const FAnimationCurveIdentifier CurveId = UAnimationCurveIdentifierExtensions::FindCurveIdentifier(Skeleton, TransformCurve.Name.DisplayName, ERawCurveTrackTypes::RCT_Transform);
		if (CurveId.IsValid())
		{
			Controller.AddCurve(CurveId, TransformCurve.GetCurveTypeFlags());

			// Set each individual channel rich curve keys, to account for any custom tangents etc.
			for (int32 SubCurveIndex = 0; SubCurveIndex < 3; ++SubCurveIndex)
			{
				const ETransformCurveChannel Channel = (ETransformCurveChannel)SubCurveIndex;
				const FVectorCurve* VectorCurve = TransformCurve.GetVectorCurveByIndex(SubCurveIndex);
				for (int32 ChannelIndex = 0; ChannelIndex < 3; ++ChannelIndex)
				{
					const EVectorCurveChannel Axis = (EVectorCurveChannel)ChannelIndex;
					FAnimationCurveIdentifier TargetCurveIdentifier = CurveId;
					UAnimationCurveIdentifierExtensions::GetTransformChildCurveIdentifier(TargetCurveIdentifier, Channel, Axis);
					Controller.SetCurveKeys(TargetCurveIdentifier, VectorCurve->FloatCurves[ChannelIndex].GetConstRefOfKeys());
				}
			}
		}
	}
}
#endif // WITH_EDITOR
 
 
 void ExtractBoneTransform(const FRawAnimSequenceTrack& RawTrack, FTransform& OutTransform, int32 KeyIndex)
{
	// Bail out (with rather wacky data) if data is empty for some reason.
	if (RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("GetBoneTransform : No anim data in track!"));
		OutTransform.SetIdentity();
		return;
	}

	const int32 PosKeyIndex = FMath::Min(KeyIndex, RawTrack.PosKeys.Num() - 1);
	const int32 RotKeyIndex = FMath::Min(KeyIndex, RawTrack.RotKeys.Num() - 1);
	static const FVector DefaultScale3D = FVector(1.f);

	OutTransform.SetTranslation(FVector(RawTrack.PosKeys[PosKeyIndex]));
	OutTransform.SetRotation(FQuat(RawTrack.RotKeys[RotKeyIndex]));
	if (RawTrack.ScaleKeys.Num() > 0)
	{
		const int32 ScaleKeyIndex = FMath::Min(KeyIndex, RawTrack.ScaleKeys.Num() - 1);
		OutTransform.SetScale3D(FVector(RawTrack.ScaleKeys[ScaleKeyIndex]));
	}
	else
	{
		OutTransform.SetScale3D(DefaultScale3D);
	}
}

void Retargeting::RetargetPose(FCompactPose& InOutPose, const FName& RetargetSource, const TArray<FTransform>& RetargetTransforms)
{
	const FBoneContainer& RequiredBones = InOutPose.GetBoneContainer();
	const bool bDisableRetargeting = RequiredBones.GetDisableRetargeting();

	if (!bDisableRetargeting && RetargetTransforms.Num())
	{
		const TArray<UE::Anim::FRetargetTracking>& RetargetTracking = UE::Anim::FBuildRawPoseScratchArea::Get().RetargetTracking;

		USkeleton* Skeleton = RequiredBones.GetSkeletonAsset();

		for (const UE::Anim::FRetargetTracking& RT : RetargetTracking)
		{
			FAnimationRuntime::RetargetBoneTransform(Skeleton, RetargetSource, RetargetTransforms, InOutPose[RT.PoseBoneIndex], RT.SkeletonBoneIndex, RT.PoseBoneIndex, RequiredBones, false);
		}
	}
}


#if WITH_EDITOR
bool CopyNotifies(const UAnimSequenceBase* SourceAnimSeq, UAnimSequenceBase* DestAnimSeq, bool bShowDialogs, bool bDeleteNotifies)
{
	// Abort if source == destination.
	if (SourceAnimSeq == DestAnimSeq)
	{
		return true;
	}

	// If the destination sequence is shorter than the source sequence, we'll be dropping notifies that
	// occur at later times than the dest sequence is long.  Give the user a chance to abort if we
	// find any notifies that won't be copied over.
	if (DestAnimSeq->GetPlayLength() < SourceAnimSeq->GetPlayLength())
	{
		for (int32 NotifyIndex = 0; NotifyIndex < SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
		{
			// If a notify is found which occurs off the end of the destination sequence, prompt the user to continue.
			const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies[NotifyIndex];
			if (SrcNotifyEvent.GetTriggerTime() > DestAnimSeq->GetPlayLength())
			{
				UE_LOG(LogAnimation, Warning, TEXT("Animation Notify trigger time %f falls outside of the destination animation sequence its length %f, notify will not be copied."), SrcNotifyEvent.GetTriggerTime(), DestAnimSeq->GetPlayLength());
				
				const bool bProceed = !bShowDialogs || EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "SomeNotifiesWillNotBeCopiedQ", "Some notifies will not be copied because the destination sequence is not long enough.  Proceed?"));
				if (!bProceed)
				{
					return false;
				}
				else
				{
					break;
				}
			}
		}
	}

	// If the destination sequence contains any notifies, ask the user if they'd like
	// to delete the existing notifies before copying over from the source sequence.
	if (DestAnimSeq->Notifies.Num() > 0)
	{
		const bool bDeleteExistingNotifies = bDeleteNotifies || (bShowDialogs && EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(
			NSLOCTEXT("UnrealEd", "DestSeqAlreadyContainsNotifiesMergeQ", "The destination sequence already contains {0} notifies.  Delete these before copying?"), FText::AsNumber(DestAnimSeq->Notifies.Num()))));

		if (bDeleteExistingNotifies)
		{
			DestAnimSeq->Notifies.Empty();
			DestAnimSeq->MarkPackageDirty();
			DestAnimSeq->RefreshCacheData();
		}
	}

	// Do the copy.
	int32 NumNotifiesThatWereNotCopied = 0;

	for (int32 NotifyIndex = 0; NotifyIndex < SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies[NotifyIndex];

		// Skip notifies which occur at times later than the destination sequence is long.
		if (SrcNotifyEvent.GetTriggerTime() > DestAnimSeq->GetPlayLength())
		{
			++NumNotifiesThatWereNotCopied;
			continue;
		}

		// Copy notify tracks from src to dest if they are missing
		if (SrcNotifyEvent.TrackIndex >= DestAnimSeq->AnimNotifyTracks.Num())
		{
			for (int32 TrackIndex = DestAnimSeq->AnimNotifyTracks.Num(); TrackIndex <= SrcNotifyEvent.TrackIndex; ++TrackIndex)
			{
				DestAnimSeq->AnimNotifyTracks.Add(FAnimNotifyTrack(SourceAnimSeq->AnimNotifyTracks[TrackIndex].TrackName, SourceAnimSeq->AnimNotifyTracks[TrackIndex].TrackColor));
			}
		}

		// Track the location of the new notify.
		int32 NewNotifyIndex = DestAnimSeq->Notifies.AddDefaulted();
		FAnimNotifyEvent& NotifyEvent = DestAnimSeq->Notifies[NewNotifyIndex];

		// Copy properties of the NotifyEvent
		NotifyEvent.TrackIndex = SrcNotifyEvent.TrackIndex;
		NotifyEvent.NotifyName = SrcNotifyEvent.NotifyName;
		NotifyEvent.Duration = SrcNotifyEvent.Duration;

		// Copy the notify itself, and point the new one at it.
		if (SrcNotifyEvent.Notify)
		{
			DestAnimSeq->Notifies[NewNotifyIndex].Notify = static_cast<UAnimNotify*>(StaticDuplicateObject(SrcNotifyEvent.Notify, DestAnimSeq, NAME_None, RF_AllFlags, nullptr, EDuplicateMode::Normal, ~EInternalObjectFlags::RootSet));
		}
		else
		{
			DestAnimSeq->Notifies[NewNotifyIndex].Notify = nullptr;
		}

		if (SrcNotifyEvent.NotifyStateClass)
		{
			DestAnimSeq->Notifies[NewNotifyIndex].NotifyStateClass = static_cast<UAnimNotifyState*>(StaticDuplicateObject(SrcNotifyEvent.NotifyStateClass, DestAnimSeq, NAME_None, RF_AllFlags, nullptr, EDuplicateMode::Normal, ~EInternalObjectFlags::RootSet));
		}
		else
		{
			DestAnimSeq->Notifies[NewNotifyIndex].NotifyStateClass = nullptr;
		}

		// Copy notify timing
		NotifyEvent.Link(DestAnimSeq, SrcNotifyEvent.GetTriggerTime());
		NotifyEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(DestAnimSeq->CalculateOffsetForNotify(NotifyEvent.GetTriggerTime()));

		// Make sure editor knows we've changed something.
		DestAnimSeq->MarkPackageDirty();
		DestAnimSeq->RefreshCacheData();
	}

	// Inform the user if some notifies weren't copied.
	if (bShowDialogs && NumNotifiesThatWereNotCopied > 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			NSLOCTEXT("UnrealEd", "SomeNotifiesWereNotCopiedF", "Because the destination sequence was shorter, {0} notifies were not copied."), FText::AsNumber(NumNotifiesThatWereNotCopied)));
	}

	return true;
}

#endif // WITH_EDITOR

bool Compression::CompressAnimationDataTracks(TArray<FRawAnimSequenceTrack>& RawAnimationData, int32 NumberOfKeys, FName ErrorName, float MaxPosDiff /*= 0.0001f*/, float MaxAngleDiff /*= 0.0003f*/)
{
	bool bRemovedKeys = false;

#if WITH_EDITORONLY_DATA
	if (ensureMsgf(RawAnimationData.Num() > 0, TEXT("%s is trying to compress while raw animation is missing"), * ErrorName.ToString()))
	{
		// This removes trivial keys, and this has to happen before the removing tracks
		for (int32 TrackIndex = 0; TrackIndex < RawAnimationData.Num(); TrackIndex++)
		{
			bRemovedKeys |= CompressRawAnimSequenceTrack(RawAnimationData[TrackIndex], NumberOfKeys, ErrorName, MaxPosDiff, MaxAngleDiff);
		}

		bool bCompressScaleKeys = false;
		// go through remove keys if not needed
		for (int32 TrackIndex = 0; TrackIndex < RawAnimationData.Num(); TrackIndex++)
		{
			FRawAnimSequenceTrack const& RawData = RawAnimationData[TrackIndex];
			if (RawData.ScaleKeys.Num() > 0)
			{
				// if scale key exists, see if we can just empty it
				if ((RawData.ScaleKeys.Num() > 1) || (RawData.ScaleKeys[0].Equals(FVector3f::OneVector) == false))
				{
					bCompressScaleKeys = true;
					break;
				}
			}
		}

		// if we don't have scale, we should delete all scale keys
		// if you have one track that has scale, we still should support scale, so compress scale
		if (!bCompressScaleKeys)
		{
			// then remove all scale keys
			for (int32 TrackIndex = 0; TrackIndex < RawAnimationData.Num(); TrackIndex++)
			{
				FRawAnimSequenceTrack& RawData = RawAnimationData[TrackIndex];
				RawData.ScaleKeys.Empty();
			}
		}
	}
#endif
	return bRemovedKeys;
}


bool Compression::CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, int32 NumberOfKeys, FName ErrorName, float MaxPosDiff, float MaxAngleDiff)
{
	bool bRemovedKeys = false;

	// First part is to make sure we have valid input
	bool const bPosTrackIsValid = (RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumberOfKeys);
	if (!bPosTrackIsValid)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid position track for %s, %d frames, instead of %d. Chopping!"), *ErrorName.ToString(), RawTrack.PosKeys.Num(), NumberOfKeys);
		bRemovedKeys = true;
		RawTrack.PosKeys.RemoveAt(1, RawTrack.PosKeys.Num() - 1);
		RawTrack.PosKeys.Shrink();
		check(RawTrack.PosKeys.Num() == 1);
	}

	bool const bRotTrackIsValid = (RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumberOfKeys);
	if (!bRotTrackIsValid)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid rotation track for %s, %d frames, instead of %d. Chopping!"), *ErrorName.ToString(), RawTrack.RotKeys.Num(), NumberOfKeys);
		bRemovedKeys = true;
		RawTrack.RotKeys.RemoveAt(1, RawTrack.RotKeys.Num() - 1);
		RawTrack.RotKeys.Shrink();
		check(RawTrack.RotKeys.Num() == 1);
	}

	// scale keys can be empty, and that is valid 
	bool const bScaleTrackIsValid = (RawTrack.ScaleKeys.Num() == 0 || RawTrack.ScaleKeys.Num() == 1 || RawTrack.ScaleKeys.Num() == NumberOfKeys);
	if (!bScaleTrackIsValid)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid scaling track for %s, %d frames, instead of %d. Chopping!"), *ErrorName.ToString(), RawTrack.ScaleKeys.Num(), NumberOfKeys);
		bRemovedKeys = true;
		RawTrack.ScaleKeys.RemoveAt(1, RawTrack.ScaleKeys.Num() - 1);
		RawTrack.ScaleKeys.Shrink();
		check(RawTrack.ScaleKeys.Num() == 1);
	}

	// Second part is actual compression.

	// Check variation of position keys
	if ((RawTrack.PosKeys.Num() > 1) && (MaxPosDiff >= 0.0f))
	{
		FVector3f FirstPos = RawTrack.PosKeys[0];
		bool bFramesIdentical = true;
		for (int32 j = 1; j < RawTrack.PosKeys.Num() && bFramesIdentical; j++)
		{
			if ((FirstPos - RawTrack.PosKeys[j]).SizeSquared() > FMath::Square(MaxPosDiff))
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if (bFramesIdentical)
		{
			bRemovedKeys = true;
			RawTrack.PosKeys.RemoveAt(1, RawTrack.PosKeys.Num() - 1);
			RawTrack.PosKeys.Shrink();
			check(RawTrack.PosKeys.Num() == 1);
		}
	}

	// Check variation of rotational keys
	if ((RawTrack.RotKeys.Num() > 1) && (MaxAngleDiff >= 0.0f))
	{
		FQuat4f FirstRot = RawTrack.RotKeys[0];
		bool bFramesIdentical = true;
		for (int32 j = 1; j < RawTrack.RotKeys.Num() && bFramesIdentical; j++)
		{
			if (FQuat4f::Error(FirstRot, RawTrack.RotKeys[j]) > MaxAngleDiff)
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if (bFramesIdentical)
		{
			bRemovedKeys = true;
			RawTrack.RotKeys.RemoveAt(1, RawTrack.RotKeys.Num() - 1);
			RawTrack.RotKeys.Shrink();
			check(RawTrack.RotKeys.Num() == 1);
		}
	}

	float MaxScaleDiff = 0.0001f;

	// Check variation of Scaleition keys
	if ((RawTrack.ScaleKeys.Num() > 1) && (MaxScaleDiff >= 0.0f))
	{
		FVector3f FirstScale = RawTrack.ScaleKeys[0];
		bool bFramesIdentical = true;
		for (int32 j = 1; j < RawTrack.ScaleKeys.Num() && bFramesIdentical; j++)
		{
			if ((FirstScale - RawTrack.ScaleKeys[j]).SizeSquared() > FMath::Square(MaxScaleDiff))
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if (bFramesIdentical)
		{
			bRemovedKeys = true;
			RawTrack.ScaleKeys.RemoveAt(1, RawTrack.ScaleKeys.Num() - 1);
			RawTrack.ScaleKeys.Shrink();
			check(RawTrack.ScaleKeys.Num() == 1);
		}
	}

	return bRemovedKeys;
}

void Compression::SanitizeRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack)
{
	// if scale is too small, zero it out. Cause it hard to retarget when compress
	// inverse scale is applied to translation, and causing translation to be huge to retarget, but
	// compression can't handle that much precision. 
	for (auto ScaleIter = RawTrack.ScaleKeys.CreateIterator(); ScaleIter; ++ScaleIter)
	{
		FVector3f& Scale3D = *ScaleIter;
		if (FMath::IsNearlyZero(Scale3D.X))
		{
			Scale3D.X = 0.f;
		}
		if (FMath::IsNearlyZero(Scale3D.Y))
		{
			Scale3D.Y = 0.f;
		}
		if (FMath::IsNearlyZero(Scale3D.Z))
		{
			Scale3D.Z = 0.f;
		}
	}

	// make sure Rotation part is normalized before compress
	for (auto RotIter = RawTrack.RotKeys.CreateIterator(); RotIter; ++RotIter)
	{
		FQuat4f& Rotation = *RotIter;
		if (!Rotation.IsNormalized())
		{
			Rotation.Normalize();
		}
	}
}

#if WITH_EDITOR
Compression::FScopedCompressionGuard::FScopedCompressionGuard(UAnimSequence* InAnimSequence) : AnimSequence(InAnimSequence)
{
	if(AnimSequence)
	{
		AnimSequence->bBlockCompressionRequests = true;		
	}
}

Compression::FScopedCompressionGuard::~FScopedCompressionGuard()
{
	if(AnimSequence)
	{
		AnimSequence->bBlockCompressionRequests = false;		
	}
}

bool AnimationData::AddLoopingInterpolation(UAnimSequence* InSequence)
{
	const UAnimDataModel* DataModel = InSequence->GetDataModel();
	IAnimationDataController& Controller = InSequence->GetController();

	const int32 NumTracks = DataModel->GetNumBoneTracks();
	const int32 NumKeys = DataModel->GetNumberOfKeys();

	if (NumTracks > 0 && NumKeys > 0)
	{
		// now I need to calculate back to new animation data
		auto LoopKeyData = [&](auto& KeyData)
		{
			// Need at least a single 
			if (KeyData.Num() > 1)
			{
				auto FirstKey = KeyData[0];
				KeyData.Add(FirstKey);
			}
		};
		
		IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("AddLoopingInterpolation_Bracket", "Adding looping interpolation"));

		const TArray<FBoneAnimationTrack>& BoneAnimationTracks = DataModel->GetBoneAnimationTracks();
		for (const FBoneAnimationTrack& AnimationTrack : BoneAnimationTracks)
		{
			auto PositionalKeys = AnimationTrack.InternalTrackData.PosKeys;
			LoopKeyData(PositionalKeys);

			auto RotationalKeys = AnimationTrack.InternalTrackData.RotKeys;
			LoopKeyData(RotationalKeys);

			auto ScaleKeys = AnimationTrack.InternalTrackData.ScaleKeys;
			LoopKeyData(ScaleKeys);

			Controller.SetBoneTrackKeys(AnimationTrack.Name, PositionalKeys, RotationalKeys, ScaleKeys);
		}

		const float Interval = DataModel->GetFrameRate().AsInterval();
		Controller.SetPlayLength(Interval + DataModel->GetPlayLength());

		return true;
	}

	return false;
}

bool AnimationData::Trim(UAnimSequence* InSequence, float TrimStart, float TrimEnd, bool bInclusiveEnd /*=false*/ )
{
	const UAnimDataModel* DataModel = InSequence->GetDataModel();
	IAnimationDataController& Controller = InSequence->GetController();

	const int32 NumTracks = DataModel->GetNumBoneTracks();
	const int32 NumKeys = DataModel->GetNumberOfKeys();

	if (NumTracks > 0 && NumKeys > 0)
	{
		const FFrameRate& FrameRate = DataModel->GetFrameRate();

		// if there is only one key, there is nothing to trim away
		if (NumKeys <= 1)
		{
			return false;
		}

		const FFrameNumber StartFrameTrim = FrameRate.AsFrameTime(TrimStart).RoundToFrame();
		const FFrameNumber EndFrameTrim = FrameRate.AsFrameTime(TrimEnd).RoundToFrame();

		const int32 StartTrimKeyIndex = StartFrameTrim.Value;
		const int32 NumTrimmedFrames = (EndFrameTrim.Value - StartFrameTrim.Value) + (bInclusiveEnd ? 1 : 0);

		if (StartTrimKeyIndex == 0 && NumTrimmedFrames == NumKeys)
		{
			return false;
		}

		if (NumTrimmedFrames == 0)
		{
			return false;
		}
		
		IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("TrimRawAnimation_Bracket", "Trimming Animation Track Data"));
		RemoveKeys(InSequence, StartTrimKeyIndex, NumTrimmedFrames);
		
		return true;
	}

	return false;
}

void AnimationData::DuplicateKeys(UAnimSequence* InSequence, int32 StartKeyIndex, int32 NumDuplicates, int32 SourceKeyIndex /*= INDEX_NONE */)
{
	const UAnimDataModel* Model = InSequence->GetDataModel();
	IAnimationDataController& Controller = InSequence->GetController();

	const int32 NumberOfKeys = Model->GetNumberOfKeys();
	const FFrameRate& FrameRate = Model->GetFrameRate();

	// Ensure that the index at which keys will be inserted, and the source key index for the duplicates is valid as well
	if (StartKeyIndex >= 0 && StartKeyIndex <= NumberOfKeys && NumDuplicates >= 1)
	{
		const int32 CopyKeyIndex = SourceKeyIndex == INDEX_NONE ? StartKeyIndex : SourceKeyIndex;
		if (CopyKeyIndex >= 0 && CopyKeyIndex < NumberOfKeys)
		{
			IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("DuplicateKeys_Bracket", "Inserting Duplicate Animation Track Keys"));

			const int32 NumFramesToInsert = NumDuplicates;
			const int32 EndFrameIndex = StartKeyIndex + NumDuplicates;

			TArray<FBoneAnimationTrack> BoneAnimationTracks = Model->GetBoneAnimationTracks();
			auto InsertFrames = [&](auto& KeyData)
			{
				if (KeyData.Num() >= 1 && KeyData.IsValidIndex(CopyKeyIndex))
				{
					auto SourceKeyData = KeyData[CopyKeyIndex];
					KeyData.InsertZeroed(StartKeyIndex, NumFramesToInsert);

					for (int32 FrameIndex = StartKeyIndex; FrameIndex < EndFrameIndex; ++FrameIndex)
					{
						KeyData[FrameIndex] = SourceKeyData;
					}
				}
			};

			for (FBoneAnimationTrack& AnimationTrack : BoneAnimationTracks)
			{
				FRawAnimSequenceTrack& TrackData = AnimationTrack.InternalTrackData;

				InsertFrames(TrackData.PosKeys);
				InsertFrames(TrackData.RotKeys);
				InsertFrames(TrackData.ScaleKeys);

				Controller.SetBoneTrackKeys(AnimationTrack.Name, TrackData.PosKeys, TrackData.RotKeys, TrackData.ScaleKeys);
			}

			// The number of keys has changed, which means that the sequence length and number of frames should be updated as well
			const int32 NewNumKeys = NumberOfKeys + NumDuplicates;
			const int32 NewNumFrames = NewNumKeys - 1;
			const float NewSequenceLength = FrameRate.AsSeconds(NewNumFrames);

			const float StartTime = FrameRate.AsSeconds(StartKeyIndex);
			const float InsertedTime = FrameRate.AsInterval() * NumDuplicates;

			// Notify will happen with time slice that was inserted
			Controller.Resize(NewSequenceLength, StartTime, StartTime + InsertedTime);
		}
	}
}

void AnimationData::RemoveKeys(UAnimSequence* InSequence, int32 StartKeyIndex, int32 NumKeysToRemove)
{
	const UAnimDataModel* Model = InSequence->GetDataModel();
	IAnimationDataController& Controller = InSequence->GetController();

	const int32 NumberOfKeys = Model->GetNumberOfKeys();
	const FFrameRate& FrameRate = Model->GetFrameRate();

	const int32 EndKeyIndex = StartKeyIndex + NumKeysToRemove;
	if (StartKeyIndex >= 0 && StartKeyIndex < NumberOfKeys && NumKeysToRemove > 0 && EndKeyIndex <= NumberOfKeys)
	{
		IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("RemoveKeys_Bracket", "Removing Animation Track Keys"));

		auto ShrinkKeys = [&](auto& KeyData)
		{
			if (KeyData.Num() >= (StartKeyIndex + NumKeysToRemove))
			{
				KeyData.RemoveAt(StartKeyIndex, NumKeysToRemove);
				check(KeyData.Num() > 0);
				KeyData.Shrink();
			}
		};

		const int32 NewNumberOfKeys = NumberOfKeys - NumKeysToRemove;

		TArray<FBoneAnimationTrack> BoneAnimationTracks = Model->GetBoneAnimationTracks();
		for (FBoneAnimationTrack& AnimationTrack : BoneAnimationTracks)
		{
			FRawAnimSequenceTrack& TrackData = AnimationTrack.InternalTrackData;

			ShrinkKeys(TrackData.PosKeys);
			ShrinkKeys(TrackData.RotKeys);
			ShrinkKeys(TrackData.ScaleKeys);
			
			Controller.SetBoneTrackKeys(AnimationTrack.Name, TrackData.PosKeys, TrackData.RotKeys, TrackData.ScaleKeys);
		}

		const int32 NewNumberOfFrames = FMath::Max(NewNumberOfKeys - 1, 1);
		const float NewSequenceLength = FrameRate.AsSeconds(NewNumberOfFrames);

		const float StartFrameTime = FrameRate.AsSeconds(FMath::Max(StartKeyIndex, 0));
		const float RemovedTime = NumKeysToRemove * FrameRate.AsInterval();
		const float EndTime = StartFrameTime + RemovedTime;

		// Notify will happen with time slice that was removed
		Controller.Resize(NewSequenceLength, StartFrameTime, EndTime);
	}
}

int32 AnimationData::FindFirstChildTrackIndex(const UAnimSequence* InSequence, const USkeleton* Skeleton, const FName& BoneName)
{
	const UAnimDataModel* DataModel = InSequence->GetDataModel();
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		// get out, nothing to do
		return INDEX_NONE;
	}

	// find children
	TArray<int32> Childs;
	if (Skeleton->GetChildBones(BoneIndex, Childs) > 0)
	{
		// first look for direct children
		for (auto ChildIndex : Childs)
		{
			FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);

			int32 ChildTrackIndex = DataModel->GetBoneTrackIndexByName(ChildBoneName);
			if (ChildTrackIndex != INDEX_NONE)
			{
				// found the new track
				return ChildTrackIndex;
			}
		}

		int32 BestGrandChildIndex = INDEX_NONE;
		// if you didn't find yet, now you have to go through all children
		for (auto ChildIndex : Childs)
		{
			FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);
			// now I have to go through all childrewn and find who is earliest since I don't know which one might be the closest one
			int32 GrandChildIndex = FindFirstChildTrackIndex(InSequence, Skeleton, ChildBoneName);
			if (GrandChildIndex != INDEX_NONE)
			{
				if (BestGrandChildIndex == INDEX_NONE)
				{
					BestGrandChildIndex = GrandChildIndex;
				}
				else if (BestGrandChildIndex > GrandChildIndex)
				{
					// best should be earlier track index
					BestGrandChildIndex = GrandChildIndex;
				}
			}
		}

		return BestGrandChildIndex;
	}

	// there is no child, just add at the end
	return DataModel->GetNumBoneTracks();
}

#endif // WITH_EDITOR


} // Namespace Anim

} // Namespace UE

#undef LOCTEXT_NAMESPACE 