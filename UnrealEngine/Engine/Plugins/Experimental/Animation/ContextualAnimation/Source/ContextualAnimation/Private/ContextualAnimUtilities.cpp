// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimUtilities.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimSceneAsset.h"
#include "Misc/MemStack.h"
#include "GameFramework/Character.h"
#include "ContextualAnimActorInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "AnimNotifyState_IKWindow.h"
#include "SceneManagement.h"
#include "MotionWarpingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContextualAnimUtilities)

void UContextualAnimUtilities::ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose)
{
	OutPose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context(Time, bExtractRootMotion);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(OutPose, Curve, Attributes);
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		AnimSequence->GetBonePose(AnimationPoseData, Context);
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		const FAnimTrack& AnimTrack = AnimMontage->SlotAnimTracks[0].AnimTrack;
		AnimTrack.GetAnimationPose(AnimationPoseData, Context);
	}
}

void UContextualAnimUtilities::ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose)
{
	FCompactPose Pose;
	ExtractLocalSpacePose(Animation, BoneContainer, Time, bExtractRootMotion, Pose);
	OutPose.InitPose(MoveTemp(Pose));
}

FTransform UContextualAnimUtilities::ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime)
{
	if (const UAnimMontage* Anim = Cast<UAnimMontage>(Animation))
	{
		return Anim->ExtractRootMotionFromTrackRange(StartTime, EndTime);
	}

	if (const UAnimSequence* Anim = Cast<UAnimSequence>(Animation))
	{
		return Anim->ExtractRootMotionFromRange(StartTime, EndTime);
	}

	return FTransform::Identity;
}

FTransform UContextualAnimUtilities::ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time)
{
	if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		if (const FAnimSegment* Segment = AnimMontage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(Time))
		{
			if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Segment->GetAnimReference()))
			{
				const float AnimSequenceTime = Segment->ConvertTrackPosToAnimPos(Time);
				return AnimSequence->ExtractRootTrackTransform(AnimSequenceTime, nullptr);
			}
		}
	}
	else if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		return AnimSequence->ExtractRootTrackTransform(Time, nullptr);
	}

	return FTransform::Identity;
}

void UContextualAnimUtilities::BP_DrawDebugPose(const UObject* WorldContextObject, const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float LifeTime, float Thickness)
{
	if(GEngine)
	{
		if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
		{
			DrawPose(World, Animation, Time, LocalToWorldTransform, Color, LifeTime, Thickness);
		}
	}
}

void UContextualAnimUtilities::DrawPose(const UWorld* World, const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float LifeTime, float Thickness)
{
	if (World)
	{
		auto DrawFunction = [World](const FVector& LineStart, const FVector& LineEnd, const FColor& Color, float LifeTime, float Thickness) {
			DrawDebugLine(World, LineStart, LineEnd, Color, false, LifeTime, 0, Thickness);
		};

		DrawPose(Animation, Time, LocalToWorldTransform, Color, LifeTime, Thickness, DrawFunction);
	}
}

void UContextualAnimUtilities::DrawPose(FPrimitiveDrawInterface* PDI, const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float Thickness)
{
	if (PDI)
	{
		auto DrawFunction = [PDI](const FVector& LineStart, const FVector& LineEnd, const FColor& Color, float LifeTime, float Thickness) {
			PDI->DrawLine(LineStart, LineEnd, Color, 0, Thickness);
		};

		DrawPose(Animation, Time, LocalToWorldTransform, Color, 0, Thickness, DrawFunction);
	}
}

void UContextualAnimUtilities::DrawPose(const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float LifeTime, float Thickness, FDrawLineFunction DrawFunction)
{
	FMemMark Mark(FMemStack::Get());

	Time = FMath::Clamp(Time, 0.f, Animation->GetPlayLength());

	const int32 TotalBones = Animation->GetSkeleton()->GetReferenceSkeleton().GetNum();
	TArray<FBoneIndexType> RequiredBoneIndexArray;
	RequiredBoneIndexArray.Reserve(TotalBones);
	for (int32 Idx = 0; Idx < TotalBones; Idx++)
	{
		RequiredBoneIndexArray.Add(Idx);
	}

	FBoneContainer BoneContainer(RequiredBoneIndexArray, FCurveEvaluationOption(false), *Animation->GetSkeleton());
	FCSPose<FCompactPose> ComponentSpacePose;
	UContextualAnimUtilities::ExtractComponentSpacePose(Animation, BoneContainer, Time, true, ComponentSpacePose);

	for (int32 Index = 0; Index < ComponentSpacePose.GetPose().GetNumBones(); ++Index)
	{
		const FCompactPoseBoneIndex CompactPoseBoneIndex = FCompactPoseBoneIndex(Index);
		const FCompactPoseBoneIndex ParentIndex = ComponentSpacePose.GetPose().GetParentBoneIndex(CompactPoseBoneIndex);
		FVector Start, End;

		const FTransform Transform = ComponentSpacePose.GetComponentSpaceTransform(CompactPoseBoneIndex) * LocalToWorldTransform;

		if (ParentIndex.GetInt() >= 0)
		{
			Start = (ComponentSpacePose.GetComponentSpaceTransform(ParentIndex) * LocalToWorldTransform).GetLocation();
			End = Transform.GetLocation();
		}
		else
		{
			Start = LocalToWorldTransform.GetLocation();
			End = Transform.GetLocation();
		}

		DrawFunction(Start, End, Color.ToFColor(false), LifeTime, Thickness);
	}
}

void UContextualAnimUtilities::DrawDebugAnimSet(const UWorld* World, const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimSet& AnimSet, float Time, const FTransform& ToWorldTransform, const FColor& Color, float LifeTime, float Thickness)
{
	if (World)
	{
		for(const FContextualAnimTrack& AnimTrack : AnimSet.Tracks)
		{
			const FTransform Transform = (SceneAsset.GetMeshToComponentForRole(AnimTrack.Role) * AnimTrack.GetAlignmentTransformAtTime(Time)) * ToWorldTransform;

			if (const UAnimSequenceBase* Animation = AnimTrack.Animation)
			{
				DrawPose(World, Animation, Time, Transform, Color, LifeTime, Thickness);
			}
			else
			{
				DrawDebugCoordinateSystem(World, Transform.GetLocation(), Transform.Rotator(), 50.f, false, LifeTime, 0, Thickness);
			}
		}
	}
}

USkeletalMeshComponent* UContextualAnimUtilities::TryGetSkeletalMeshComponent(const AActor* Actor)
{
	USkeletalMeshComponent* SkelMeshComp = nullptr;
	if (Actor)
	{
		if (const ACharacter* Character = Cast<const ACharacter>(Actor))
		{
			SkelMeshComp = Character->GetMesh();
		}
		else if (Actor->GetClass()->ImplementsInterface(UContextualAnimActorInterface::StaticClass()))
		{
			SkelMeshComp = IContextualAnimActorInterface::Execute_GetMesh(Actor);
		}
		else
		{
			SkelMeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
		}
	}

	return SkelMeshComp;
}

UAnimInstance* UContextualAnimUtilities::TryGetAnimInstance(const AActor* Actor)
{
	if (USkeletalMeshComponent* SkelMeshComp = UContextualAnimUtilities::TryGetSkeletalMeshComponent(Actor))
	{
		return SkelMeshComp->GetAnimInstance();
	}

	return nullptr;
}

FAnimMontageInstance* UContextualAnimUtilities::TryGetActiveAnimMontageInstance(const AActor* Actor)
{
	if(UAnimInstance* AnimInstance = UContextualAnimUtilities::TryGetAnimInstance(Actor))
	{
		return AnimInstance->GetActiveMontageInstance();
	}

	return nullptr;
}

void UContextualAnimUtilities::BP_Montage_GetSectionStartAndEndTime(const UAnimMontage* Montage, int32 SectionIndex, float& OutStartTime, float& OutEndTime)
{
	if(Montage)
	{
		Montage->GetSectionStartAndEndTime(SectionIndex, OutStartTime, OutEndTime);
	}
}

float UContextualAnimUtilities::BP_Montage_GetSectionTimeLeftFromPos(const UAnimMontage* Montage, float Position)
{
	//UAnimMontage::GetSectionTimeLeftFromPos is not const :(
	return Montage ? (const_cast<UAnimMontage*>(Montage))->GetSectionTimeLeftFromPos(Position) : -1.f;
}

float UContextualAnimUtilities::BP_Montage_GetSectionLength(const UAnimMontage* Montage, int32 SectionIndex)
{
	return Montage ? Montage->GetSectionLength(SectionIndex) : -1.f;
}

void UContextualAnimUtilities::DrawSector(FPrimitiveDrawInterface& PDI, const FVector& Origin, const FVector& Direction, float MinDistance, float MaxDistance, float MinAngle, float MaxAngle, const FLinearColor& Color, uint8 DepthPriority, float Thickness, bool bDashedLine)
{
	if(MinAngle == 0 && MaxAngle == 0)
	{
		DrawCircle(&PDI, Origin, FVector(1, 0, 0), FVector(0, 1, 0), Color, 30.f, 12, SDPG_World, 1.f);
		return;
	}

	// Draw Cone lines
	const FVector LeftDirection = Direction.RotateAngleAxis(MinAngle, FVector::UpVector);
	const FVector RightDirection = Direction.RotateAngleAxis(MaxAngle, FVector::UpVector);

	if(bDashedLine)
	{
		DrawDashedLine(&PDI, Origin + (LeftDirection * MinDistance), Origin + (LeftDirection * MaxDistance), Color, 10.f, DepthPriority);
		DrawDashedLine(&PDI, Origin + (RightDirection * MinDistance), Origin + (RightDirection * MaxDistance), Color, 10.f, DepthPriority);
	}
	else
	{
		PDI.DrawLine(Origin + (LeftDirection * MinDistance), Origin + (LeftDirection * MaxDistance), Color, DepthPriority, Thickness);
		PDI.DrawLine(Origin + (RightDirection * MinDistance), Origin + (RightDirection * MaxDistance), Color, DepthPriority, Thickness);
	}

	// Draw Near Arc
	FVector LastDirection = LeftDirection;
	float Angle = MinAngle;
	while (Angle < MaxAngle)
	{
		Angle = FMath::Clamp<float>(Angle + 10, MinAngle, MaxAngle);

		const float Length = MinDistance;
		const FVector NewDirection = Direction.RotateAngleAxis(Angle, FVector::UpVector);
		const FVector LineStart = Origin + (LastDirection * Length);
		const FVector LineEnd = Origin + (NewDirection * Length);
		
		if (bDashedLine)
		{
			DrawDashedLine(&PDI, LineStart, LineEnd, Color, 10.f, DepthPriority);
		}
		else
		{
			PDI.DrawLine(LineStart, LineEnd, Color, DepthPriority, Thickness);
		}
		
		LastDirection = NewDirection;
	}

	// Draw Far Arc
	LastDirection = LeftDirection;
	Angle = MinAngle;
	while (Angle < MaxAngle)
	{
		Angle = FMath::Clamp<float>(Angle + 10, MinAngle, MaxAngle);

		const float Length = MaxDistance;
		const FVector NewDirection = Direction.RotateAngleAxis(Angle, FVector::UpVector);
		const FVector LineStart = Origin + (LastDirection * Length);
		const FVector LineEnd = Origin + (NewDirection * Length);

		if (bDashedLine)
		{
			DrawDashedLine(&PDI, LineStart, LineEnd, Color, 10.f, DepthPriority);
		}
		else
		{
			PDI.DrawLine(LineStart, LineEnd, Color, DepthPriority, Thickness);
		}

		LastDirection = NewDirection;
	}
}

bool UContextualAnimUtilities::BP_CreateContextualAnimSceneBindings(const UContextualAnimSceneAsset* SceneAsset, const TMap<FName, FContextualAnimSceneBindingContext>& Params, FContextualAnimSceneBindings& OutBindings)
{
	if(SceneAsset == nullptr || !SceneAsset->HasValidData())
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimUtilities::BP_CreateContextualAnimSceneBindings Failed. Reason: Invalid or Empty SceneAsset (%s)"), *GetNameSafe(SceneAsset));
		return false;
	}

	const int32 SectionIdx = 0; // Always start from the first section
	return FContextualAnimSceneBindings::TryCreateBindings(*SceneAsset, SectionIdx, Params, OutBindings);
}

bool UContextualAnimUtilities::BP_CreateContextualAnimSceneBindingsForTwoActors(const UContextualAnimSceneAsset* SceneAsset, const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Secondary, FContextualAnimSceneBindings& OutBindings)
{
	if (SceneAsset == nullptr || !SceneAsset->HasValidData())
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimUtilities::BP_CreateContextualAnimSceneBindingsForTwoActors Failed. Reason: Invalid or Empty SceneAsset (%s)"), *GetNameSafe(SceneAsset));
		return false;
	}

	const int32 SectionIdx = 0; // Always start from the first section
	return FContextualAnimSceneBindings::TryCreateBindings(*SceneAsset, SectionIdx, Primary, Secondary, OutBindings);
}

// SceneBindings Blueprint Interface
//------------------------------------------------------------------------------------------

void UContextualAnimUtilities::BP_SceneBindings_GetSectionAndAnimSetIndices(const FContextualAnimSceneBindings& Bindings, int32& SectionIdx, int32& AnimSetIdx)
{
	SectionIdx = Bindings.GetSectionIdx();
	AnimSetIdx = Bindings.GetAnimSetIdx();
}

const FContextualAnimSceneBinding& UContextualAnimUtilities::BP_SceneBindings_GetBindingByRole(const FContextualAnimSceneBindings& Bindings, FName Role)
{
	if(const FContextualAnimSceneBinding* SceneActorData = Bindings.FindBindingByRole(Role))
	{
		return *SceneActorData;
	}

	return FContextualAnimSceneBinding::InvalidBinding;
}

const FContextualAnimSceneBinding& UContextualAnimUtilities::BP_SceneBindings_GetBindingByActor(const FContextualAnimSceneBindings& Bindings, const AActor* Actor)
{
	if (const FContextualAnimSceneBinding* SceneActorData = Bindings.FindBindingByActor(Actor))
	{
		return *SceneActorData;
	}

	return FContextualAnimSceneBinding::InvalidBinding;
}

void UContextualAnimUtilities::BP_SceneBindings_CalculateAnimSetPivots(const FContextualAnimSceneBindings& Bindings, TArray<FContextualAnimSetPivot>& OutPivots)
{
	Bindings.CalculateAnimSetPivots(OutPivots);
}

void UContextualAnimUtilities::BP_SceneBindings_AddOrUpdateWarpTargetsForBindings(const FContextualAnimSceneBindings& Bindings)
{
	if(!Bindings.IsValid())
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimUtilities::BP_SceneBindings_AddOrUpdateWarpTargetsForBindings Failed. Reason: Invalid Bindings. SceneAsset: %s"), *GetNameSafe(Bindings.GetSceneAsset()));
		return;
	}

	const UContextualAnimSceneAsset* SceneAsset = Bindings.GetSceneAsset();
	const int32 SectionIdx = Bindings.GetSectionIdx();
	for (const FContextualAnimSetPivotDefinition& PivotDef : SceneAsset->GetAnimSetPivotDefinitionsInSection(SectionIdx))
	{
		FContextualAnimSetPivot ScenePivot;
		if (Bindings.CalculateAnimSetPivot(PivotDef, ScenePivot))
		{
			for (const FContextualAnimSceneBinding& Binding : Bindings)
			{
				if (UMotionWarpingComponent* MotionWarpComp = Binding.GetActor()->FindComponentByClass<UMotionWarpingComponent>())
				{
					//@TODO: Cache this
					const FContextualAnimTrack& AnimTrack = Bindings.GetAnimTrackFromBinding(Binding);
					const float Time = AnimTrack.GetSyncTimeForWarpSection(PivotDef.Name);
					const FTransform TransformRelativeToScenePivot = Bindings.GetAlignmentTransformFromBinding(Binding, PivotDef.Name, Time);
					const FTransform WarpTarget = (TransformRelativeToScenePivot * ScenePivot.Transform);
					MotionWarpComp->AddOrUpdateWarpTargetFromTransform(PivotDef.Name, WarpTarget);
				}
			}
		}
	}
}

FTransform UContextualAnimUtilities::BP_SceneBindings_GetAlignmentTransformForRoleRelativeToOtherRole(const FContextualAnimSceneBindings& Bindings, FName Role, FName RelativeToRole, float Time = 0.f)
{
	FTransform Result = FTransform::Identity;

	if(const UContextualAnimSceneAsset* SceneAsset = Bindings.GetSceneAsset())
	{
		Result = SceneAsset->GetAlignmentTransformForRoleRelativeToOtherRoleInSection(Bindings.GetSectionIdx(), Bindings.GetAnimSetIdx(), Role, RelativeToRole, Time);
	}

	return Result;
}

FTransform UContextualAnimUtilities::BP_SceneBindings_GetAlignmentTransformForRoleRelativeToPivot(const FContextualAnimSceneBindings& Bindings, FName Role, const FContextualAnimSetPivot& Pivot, float Time)
{
	FTransform Result = FTransform::Identity;

	if (const UContextualAnimSceneAsset* SceneAsset = Bindings.GetSceneAsset())
	{
		if (const FContextualAnimSceneSection* Section = SceneAsset->GetSection(Bindings.GetSectionIdx()))
		{
			return Section->GetAlignmentTransformForRoleRelativeToPivot(Bindings.GetAnimSetIdx(), Role, Time) * Pivot.Transform;
		}
	}

	return Result;
}

const UAnimSequenceBase* UContextualAnimUtilities::BP_SceneBinding_GetAnimationFromBinding(const FContextualAnimSceneBindings& Bindings, const FContextualAnimSceneBinding& Binding)
{
	if (!Bindings.IsValid())
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimUtilities::BP_SceneBinding_GetAnimationFromBinding Failed. Reason: Invalid Bindings. SceneAsset: %s"), *GetNameSafe(Bindings.GetSceneAsset()));
		return nullptr;
	}

	return Bindings.GetAnimTrackFromBinding(Binding).Animation;
}

FName UContextualAnimUtilities::BP_SceneBinding_GetRoleFromBinding(const FContextualAnimSceneBindings& Bindings, const FContextualAnimSceneBinding& Binding)
{
	if (!Bindings.IsValid())
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimUtilities::BP_SceneBinding_GetRoleFromBinding Failed. Reason: Invalid Bindings. SceneAsset: %s"), *GetNameSafe(Bindings.GetSceneAsset()));
		return NAME_None;
	}

	return Bindings.GetAnimTrackFromBinding(Binding).Role;
}

FTransform UContextualAnimUtilities::BP_SceneBindings_GetAlignmentTransformFromBinding(const FContextualAnimSceneBindings& Bindings, const FContextualAnimSceneBinding& Binding, const FContextualAnimSetPivot& Pivot)
{
	if (!Bindings.IsValid())
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("UContextualAnimUtilities::BP_SceneBindings_GetAlignmentTransformFromBinding Failed. Reason: Invalid Bindings. SceneAsset: %s"), *GetNameSafe(Bindings.GetSceneAsset()));
		return FTransform::Identity;
	}

	const FContextualAnimTrack& AnimTrack = Bindings.GetAnimTrackFromBinding(Binding);

	float StartTime, EndTime;
	AnimTrack.GetStartAndEndTimeForWarpSection(Pivot.Name, StartTime, EndTime);

	return AnimTrack.GetAlignmentTransformAtTime(EndTime) * Pivot.Transform;
}
