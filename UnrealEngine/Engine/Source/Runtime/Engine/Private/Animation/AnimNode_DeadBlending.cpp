// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNode_DeadBlending.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SaveCachedPose.h"
#include "Animation/BlendProfile.h"
#include "Algo/MaxElement.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Logging/TokenizedMessage.h"
#include "Animation/AnimCurveUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_DeadBlending)

LLM_DEFINE_TAG(Animation_DeadBlending);

#define LOCTEXT_NAMESPACE "AnimNode_DeadBlending"

namespace UE::Anim {

	// Inertialization request event bound to a node
	class FDeadBlendingRequester : public IInertializationRequester
	{
	public:
		FDeadBlendingRequester(const FAnimationBaseContext& InContext, FAnimNode_DeadBlending* InNode)
			: Node(*InNode)
			, NodeId(InContext.GetCurrentNodeId())
			, Proxy(*InContext.AnimInstanceProxy)
		{}

	private:
		// IInertializationRequester interface
		virtual void RequestInertialization(
			float InRequestedDuration,
			const UBlendProfile* InBlendProfile) override
		{
			Node.RequestInertialization(FInertializationRequest(InRequestedDuration, InBlendProfile));
		}

		virtual void RequestInertialization(const FInertializationRequest& Request)
		{
			Node.RequestInertialization(Request);
		}

		virtual void AddDebugRecord(const FAnimInstanceProxy& InSourceProxy, int32 InSourceNodeId)
		{
#if WITH_EDITORONLY_DATA
			Proxy.RecordNodeAttribute(InSourceProxy, NodeId, InSourceNodeId, IInertializationRequester::Attribute);
#endif
			TRACE_ANIM_NODE_ATTRIBUTE(Proxy, InSourceProxy, NodeId, InSourceNodeId, IInertializationRequester::Attribute);
		}

		// Node to target
		FAnimNode_DeadBlending& Node;

		// Node index
		int32 NodeId;

		// Proxy currently executing
		FAnimInstanceProxy& Proxy;
	};

}	// namespace UE::Anim

namespace UE::Anim::DeadBlending::Private
{
	static constexpr int32 MaxPoseSnapShotNum = 2;

	static constexpr float Ln2 = 0.69314718056f;

	static int32 GetNumSkeletonBones(const FBoneContainer& BoneContainer)
	{
		const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
		check(SkeletonAsset);

		const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
		return RefSkeleton.GetNum();
	}

	static inline FVector3f VectorDivMax(const float V, const FVector3f W, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector3f(
			V / FMath::Max(W.X, Epsilon),
			V / FMath::Max(W.Y, Epsilon),
			V / FMath::Max(W.Z, Epsilon));
	}

	static inline FVector3f VectorDivMax(const FVector3f V, const FVector3f W, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector3f(
			V.X / FMath::Max(W.X, Epsilon),
			V.Y / FMath::Max(W.Y, Epsilon),
			V.Z / FMath::Max(W.Z, Epsilon));
	}

	static inline FVector VectorDivMax(const FVector V, const FVector W, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector(
			V.X / FMath::Max(W.X, Epsilon),
			V.Y / FMath::Max(W.Y, Epsilon),
			V.Z / FMath::Max(W.Z, Epsilon));
	}

	static inline FVector3f VectorInvExpApprox(const FVector3f V)
	{
		return FVector3f(
			FMath::InvExpApprox(V.X),
			FMath::InvExpApprox(V.Y),
			FMath::InvExpApprox(V.Z));
	}

	static inline FVector VectorEerp(const FVector V, const FVector W, const float Alpha, const float Epsilon = UE_SMALL_NUMBER)
	{
		if (FVector::DistSquared(V, W) < Epsilon)
		{
			return FMath::Lerp(V, W, Alpha);
		}
		else
		{
			return FVector(
				FMath::Pow(V.X, (1.0f - Alpha)) * FMath::Pow(W.X, Alpha),
				FMath::Pow(V.Y, (1.0f - Alpha)) * FMath::Pow(W.Y, Alpha),
				FMath::Pow(V.Z, (1.0f - Alpha)) * FMath::Pow(W.Z, Alpha));
		}
	}

	static inline FVector VectorExp(const FVector V)
	{
		return FVector(
			FMath::Exp(V.X),
			FMath::Exp(V.Y),
			FMath::Exp(V.Z));
	}

	static inline FVector VectorLogSafe(const FVector V, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector(
			FMath::Loge(FMath::Max(V.X, Epsilon)),
			FMath::Loge(FMath::Max(V.Y, Epsilon)),
			FMath::Loge(FMath::Max(V.Z, Epsilon)));
	}

	static inline FVector ExtrapolateTranslation(
		const FVector Translation,
		const FVector3f Velocity,
		const float Time,
		const FVector3f DecayHalflife,
		const float Epsilon = UE_SMALL_NUMBER)
	{
		if (Velocity.SquaredLength() > Epsilon)
		{
			const FVector3f C = VectorDivMax(Ln2, DecayHalflife, Epsilon);
			return Translation + (FVector)(VectorDivMax(Velocity, C, Epsilon) * (FVector3f::OneVector - VectorInvExpApprox(C * Time)));
		}
		else
		{
			return Translation;
		}
	}

	static inline FQuat ExtrapolateRotation(
		const FQuat Rotation,
		const FVector3f Velocity,
		const float Time,
		const FVector3f DecayHalflife,
		const float Epsilon = UE_SMALL_NUMBER)
	{
		if (Velocity.SquaredLength() > Epsilon)
		{
			const FVector3f C = VectorDivMax(Ln2, DecayHalflife, Epsilon);
			return FQuat::MakeFromRotationVector((FVector)(VectorDivMax(Velocity, C, Epsilon) * (FVector3f::OneVector - VectorInvExpApprox(C * Time)))) * Rotation;
		}
		else
		{
			return Rotation;
		}
	}

	static inline FVector ExtrapolateScale(
		const FVector Scale,
		const FVector3f Velocity,
		const float Time,
		const FVector3f DecayHalflife,
		const float Epsilon = UE_SMALL_NUMBER)
	{
		if (Velocity.SquaredLength() > Epsilon)
		{
			const FVector3f C = VectorDivMax(Ln2, DecayHalflife, Epsilon);
			return VectorExp((FVector)(VectorDivMax(Velocity, C, Epsilon) * (FVector3f::OneVector - VectorInvExpApprox(C * Time)))) * Scale;
		}
		else
		{
			return Scale;
		}
	}

	static inline float ExtrapolateCurve(
		const float Curve,
		const float Velocity,
		const float Time,
		const float DecayHalflife,
		const float Epsilon = UE_SMALL_NUMBER)
	{
		if (FMath::Square(Velocity) > Epsilon)
		{
			const float C = Ln2 / FMath::Max(DecayHalflife, Epsilon);
			return Curve + FMath::Max(Velocity / C, Epsilon) * (1.0f - FMath::InvExpApprox(C * Time));
		}
		else
		{
			return Curve;
		}
	}

	static inline float ClipMagnitudeToGreaterThanEpsilon(const float X, const float Epsilon = UE_KINDA_SMALL_NUMBER)
	{
		return
			X >= 0.0f && X <  Epsilon ?  Epsilon :
			X <  0.0f && X > -Epsilon ? -Epsilon : X;
	}

	static inline float ComputeDecayHalfLifeFromDiffAndVelocity(
		const float SrcDstDiff,
		const float SrcVelocity,
		const float HalfLife,
		const float HalfLifeMin,
		const float HalfLifeMax,
		const float Epsilon = UE_KINDA_SMALL_NUMBER)
	{
		// Essentially what this function does is compute a half-life based on the ratio between the velocity vector and
		// the vector from the source to the destination. This is then clamped to some min and max. If the signs are
		// different (i.e. the velocity and the vector from source to destination are in opposite directions) this will
		// produce a negative number that will get clamped to HalfLifeMin. If the signs match, this will produce a large
		// number when the velocity is small and the vector from source to destination is large, and a small number when
		// the velocity is large and the vector from source to destination is small. This will be clamped either way to 
		// be in the range given by HalfLifeMin and HalfLifeMax. Finally, since the velocity can be close to zero we 
		// have to clamp it to always be greater than some given magnitude (preserving the sign).

		return FMath::Clamp(HalfLife * (SrcDstDiff / ClipMagnitudeToGreaterThanEpsilon(SrcVelocity, Epsilon)), HalfLifeMin, HalfLifeMax);
	}

	static inline FVector3f ComputeDecayHalfLifeFromDiffAndVelocity(
		const FVector SrcDstDiff,
		const FVector3f SrcVelocity,
		const float HalfLife,
		const float HalfLifeMin,
		const float HalfLifeMax,
		const float Epsilon = UE_KINDA_SMALL_NUMBER)
	{
		return FVector3f(
			ComputeDecayHalfLifeFromDiffAndVelocity(SrcDstDiff.X, SrcVelocity.X, HalfLife, HalfLifeMin, HalfLifeMax, Epsilon),
			ComputeDecayHalfLifeFromDiffAndVelocity(SrcDstDiff.Y, SrcVelocity.Y, HalfLife, HalfLifeMin, HalfLifeMax, Epsilon),
			ComputeDecayHalfLifeFromDiffAndVelocity(SrcDstDiff.Z, SrcVelocity.Z, HalfLife, HalfLifeMin, HalfLifeMax, Epsilon));
	}
}

void FAnimNode_DeadBlending::Deactivate()
{
	InertializationState = EInertializationState::Inactive;

	if (!bPreallocateMemory)
	{
		BoneValid.Empty();
		BoneTranslations.Empty();
		BoneRotations.Empty();
		BoneRotationDirections.Empty();
		BoneScales.Empty();

		BoneTranslationVelocities.Empty();
		BoneRotationVelocities.Empty();
		BoneScaleVelocities.Empty();

		BoneTranslationDecayHalfLives.Empty();
		BoneRotationDecayHalfLives.Empty();
		BoneScaleDecayHalfLives.Empty();

		InertializationDurationPerBone.Empty();
	}
}

void FAnimNode_DeadBlending::InitFrom(
	const FCompactPose& InPose,
	const FBlendedCurve& InCurves,
	const FInertializationPose& SrcPosePrev,
	const FInertializationPose& SrcPoseCurr)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAnimNode_DeadBlending::InitFrom);

	const FBoneContainer& BoneContainer = InPose.GetBoneContainer();

	const int32 NumSkeletonBones = UE::Anim::DeadBlending::Private::GetNumSkeletonBones(BoneContainer);

	BoneValid.Init(false, NumSkeletonBones);
	BoneTranslations.Init(FVector::ZeroVector, NumSkeletonBones);
	BoneRotations.Init(FQuat::Identity, NumSkeletonBones);
	BoneRotationDirections.Init(FQuat4f::Identity, NumSkeletonBones);
	BoneScales.Init(FVector::OneVector, NumSkeletonBones);

	BoneTranslationVelocities.Init(FVector3f::ZeroVector, NumSkeletonBones);
	BoneRotationVelocities.Init(FVector3f::ZeroVector, NumSkeletonBones);
	BoneScaleVelocities.Init(FVector3f::ZeroVector, NumSkeletonBones);

	BoneTranslationDecayHalfLives.Init(ExtrapolationHalfLifeMin * FVector3f::OneVector, NumSkeletonBones);
	BoneRotationDecayHalfLives.Init(ExtrapolationHalfLifeMin * FVector3f::OneVector, NumSkeletonBones);
	BoneScaleDecayHalfLives.Init(ExtrapolationHalfLifeMin * FVector3f::OneVector, NumSkeletonBones);

	// Record bone state

	for (FCompactPoseBoneIndex BoneIndex : InPose.ForEachBoneIndex())
	{
		const int32 SkeletonPoseBoneIndex = BoneContainer.GetSkeletonIndex(BoneIndex);

		if (SkeletonPoseBoneIndex == INDEX_NONE ||
			SrcPosePrev.BoneStates[SkeletonPoseBoneIndex] != EInertializationBoneState::Valid ||
			SrcPoseCurr.BoneStates[SkeletonPoseBoneIndex] != EInertializationBoneState::Valid)
		{
			continue;
		}

		// Mark bone as valid

		BoneValid[SkeletonPoseBoneIndex] = true;

		// Get Source Animation Transform

		const FTransform SrcTransformCurr = SrcPoseCurr.BoneTransforms[SkeletonPoseBoneIndex];

		BoneTranslations[SkeletonPoseBoneIndex] = SrcTransformCurr.GetTranslation();
		BoneRotations[SkeletonPoseBoneIndex] = SrcTransformCurr.GetRotation();
		BoneScales[SkeletonPoseBoneIndex] = SrcTransformCurr.GetScale3D();

		if (SrcPoseCurr.DeltaTime > UE_SMALL_NUMBER)
		{
			// Get Source Animation Velocity

			const FTransform SrcTransformPrev = SrcPosePrev.BoneTransforms[SkeletonPoseBoneIndex];

			const FVector TranslationDiff = SrcTransformCurr.GetTranslation() - SrcTransformPrev.GetTranslation();

			FQuat RotationDiff = SrcTransformCurr.GetRotation() * SrcTransformPrev.GetRotation().Inverse();
			RotationDiff.EnforceShortestArcWith(FQuat::Identity);

			const FVector ScaleDiff = UE::Anim::DeadBlending::Private::VectorDivMax(SrcTransformCurr.GetScale3D(), SrcTransformPrev.GetScale3D());

			BoneTranslationVelocities[SkeletonPoseBoneIndex] = (FVector3f)(TranslationDiff / SrcPoseCurr.DeltaTime);
			BoneRotationVelocities[SkeletonPoseBoneIndex] = (FVector3f)(RotationDiff.ToRotationVector() / SrcPoseCurr.DeltaTime);
			BoneScaleVelocities[SkeletonPoseBoneIndex] = (FVector3f)(UE::Anim::DeadBlending::Private::VectorLogSafe(ScaleDiff) / SrcPoseCurr.DeltaTime);

			// Clamp Maximum Velocity

			BoneTranslationVelocities[SkeletonPoseBoneIndex] = BoneTranslationVelocities[SkeletonPoseBoneIndex].GetClampedToMaxSize(MaximumTranslationVelocity);
			BoneRotationVelocities[SkeletonPoseBoneIndex] = BoneRotationVelocities[SkeletonPoseBoneIndex].GetClampedToMaxSize(FMath::DegreesToRadians(MaximumRotationVelocity));
			BoneScaleVelocities[SkeletonPoseBoneIndex] = BoneScaleVelocities[SkeletonPoseBoneIndex].GetClampedToMaxSize(MaximumScaleVelocity);

			// Compute Decay HalfLives

			const FTransform DstTransform = InPose[BoneIndex];

			const FVector TranslationSrcDstDiff = DstTransform.GetTranslation() - SrcTransformCurr.GetTranslation();

			FQuat RotationSrcDstDiff = DstTransform.GetRotation() * SrcTransformCurr.GetRotation().Inverse();
			RotationSrcDstDiff.EnforceShortestArcWith(FQuat::Identity);

			const FVector ScaleSrcDstDiff = UE::Anim::DeadBlending::Private::VectorDivMax(DstTransform.GetScale3D(), SrcTransformCurr.GetScale3D());

			BoneTranslationDecayHalfLives[SkeletonPoseBoneIndex] = UE::Anim::DeadBlending::Private::ComputeDecayHalfLifeFromDiffAndVelocity(
				TranslationSrcDstDiff,
				BoneTranslationVelocities[SkeletonPoseBoneIndex],
				ExtrapolationHalfLife,
				ExtrapolationHalfLifeMin,
				ExtrapolationHalfLifeMax);

			BoneRotationDecayHalfLives[SkeletonPoseBoneIndex] = UE::Anim::DeadBlending::Private::ComputeDecayHalfLifeFromDiffAndVelocity(
				RotationSrcDstDiff.ToRotationVector(),
				BoneRotationVelocities[SkeletonPoseBoneIndex],
				ExtrapolationHalfLife,
				ExtrapolationHalfLifeMin,
				ExtrapolationHalfLifeMax);

			BoneScaleDecayHalfLives[SkeletonPoseBoneIndex] = UE::Anim::DeadBlending::Private::ComputeDecayHalfLifeFromDiffAndVelocity(
				ScaleSrcDstDiff,
				BoneScaleVelocities[SkeletonPoseBoneIndex],
				ExtrapolationHalfLife,
				ExtrapolationHalfLifeMin,
				ExtrapolationHalfLifeMax);
		}
	}

	CurveData.CopyFrom(SrcPoseCurr.Curves.BlendedCurve);

	// Record Source Animation Curve State

	UE::Anim::FNamedValueArrayUtils::Union(CurveData, SrcPoseCurr.Curves.BlendedCurve,
		[this](FDeadBlendingCurveElement& OutResultElement, const UE::Anim::FCurveElement& InElement1, UE::Anim::ENamedValueUnionFlags InFlags)
		{
			OutResultElement.Value = InElement1.Value;
			OutResultElement.Velocity = 0.0f;
			OutResultElement.HalfLife = ExtrapolationHalfLifeMin;
		});

	// Record Source Animation Curve Velocity

	if (SrcPoseCurr.DeltaTime > UE_SMALL_NUMBER)
	{
		UE::Anim::FNamedValueArrayUtils::Union(CurveData, SrcPosePrev.Curves.BlendedCurve,
			[this, DeltaTime = SrcPoseCurr.DeltaTime](FDeadBlendingCurveElement& OutResultElement, const UE::Anim::FCurveElement& InElement1, UE::Anim::ENamedValueUnionFlags InFlags)
			{
				bool bSrcCurrValid = (bool)(InFlags & UE::Anim::ENamedValueUnionFlags::ValidArg0);
				bool bSrcPrevValid = (bool)(InFlags & UE::Anim::ENamedValueUnionFlags::ValidArg1);

				if (bSrcCurrValid && bSrcPrevValid)
				{
					OutResultElement.Velocity = (OutResultElement.Value - InElement1.Value) / DeltaTime;
					OutResultElement.Velocity = FMath::Clamp(OutResultElement.Velocity, -MaximumCurveVelocity, MaximumCurveVelocity);
				}
			});
	}

	// Perform Union with Curves from Destination Animation and compute Half-life

	UE::Anim::FNamedValueArrayUtils::Union(CurveData, InCurves,
		[this](FDeadBlendingCurveElement& OutResultElement, const UE::Anim::FCurveElement& InElement1, UE::Anim::ENamedValueUnionFlags InFlags)
		{
			OutResultElement.HalfLife = UE::Anim::DeadBlending::Private::ComputeDecayHalfLifeFromDiffAndVelocity(
				InElement1.Value - OutResultElement.Value,
				OutResultElement.Velocity,
				ExtrapolationHalfLife,
				ExtrapolationHalfLifeMin,
				ExtrapolationHalfLifeMax);
		});

	// Apply filtering to remove anything we don't want to inertialize

	if (CurveFilter.Num() > 0)
	{
		UE::Anim::FCurveUtils::Filter(CurveData, CurveFilter);
	}
}

void FAnimNode_DeadBlending::ApplyTo(FCompactPose& InOutPose, FBlendedCurve& InOutCurves)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAnimNode_DeadBlending::ApplyTo);

	const FBoneContainer& BoneContainer = InOutPose.GetBoneContainer();

	for (FCompactPoseBoneIndex BoneIndex : InOutPose.ForEachBoneIndex())
	{
		const int32 SkeletonPoseBoneIndex = BoneContainer.GetSkeletonIndex(BoneIndex);

		if (SkeletonPoseBoneIndex == INDEX_NONE || !BoneValid[SkeletonPoseBoneIndex] || BoneFilter.Contains(BoneIndex))
		{
			continue;
		}

		// Compute Extrapolated Bone State

		const FVector ExtrapolatedTranslation = UE::Anim::DeadBlending::Private::ExtrapolateTranslation(
			BoneTranslations[SkeletonPoseBoneIndex],
			BoneTranslationVelocities[SkeletonPoseBoneIndex],
			InertializationTime,
			BoneTranslationDecayHalfLives[SkeletonPoseBoneIndex]);

		const FQuat ExtrapolatedRotation = UE::Anim::DeadBlending::Private::ExtrapolateRotation(
			BoneRotations[SkeletonPoseBoneIndex],
			BoneRotationVelocities[SkeletonPoseBoneIndex],
			InertializationTime,
			BoneRotationDecayHalfLives[SkeletonPoseBoneIndex]);

		const FVector ExtrapolatedScale = UE::Anim::DeadBlending::Private::ExtrapolateScale(
			BoneScales[SkeletonPoseBoneIndex],
			BoneScaleVelocities[SkeletonPoseBoneIndex],
			InertializationTime,
			BoneScaleDecayHalfLives[SkeletonPoseBoneIndex]);

#if WITH_EDITORONLY_DATA
		if (bShowExtrapolations)
		{
			InOutPose[BoneIndex].SetTranslation(ExtrapolatedTranslation);
			InOutPose[BoneIndex].SetRotation(ExtrapolatedRotation);
			InOutPose[BoneIndex].SetScale3D(ExtrapolatedScale);
			continue;
		}
#endif
		// We need to enforce that the blend of the rotation doesn't suddenly "switch sides"
		// given that the extrapolated rotation can become quite far from the destination
		// animation. To do this we keep track of the blend "direction" and ensure that the
		// delta we are applying to the destination animation always remains on the same
		// side of this rotation.

		FQuat RotationDiff = ExtrapolatedRotation * InOutPose[BoneIndex].GetRotation().Inverse();
		RotationDiff.EnforceShortestArcWith((FQuat)BoneRotationDirections[SkeletonPoseBoneIndex]);

		// Update BoneRotationDirections to match our current path
		BoneRotationDirections[SkeletonPoseBoneIndex] = (FQuat4f)RotationDiff;

		// Compute Blend Alpha

		const float Alpha = 1.0f - FAlphaBlend::AlphaToBlendOption(
			InertializationTime / FMath::Max(InertializationDurationPerBone[SkeletonPoseBoneIndex], UE_SMALL_NUMBER),
			InertializationBlendMode, InertializationCustomBlendCurve);

		// Perform Blend

		if (Alpha != 0.0f)
		{
			InOutPose[BoneIndex].SetTranslation(FMath::Lerp(InOutPose[BoneIndex].GetTranslation(), ExtrapolatedTranslation, Alpha));
			InOutPose[BoneIndex].SetRotation(FQuat::MakeFromRotationVector(RotationDiff.ToRotationVector() * Alpha) * InOutPose[BoneIndex].GetRotation());

			// Here we use `Eerp` rather than `Lerp` to interpolate scales by default (see: https://theorangeduck.com/page/scalar-velocity).
			// This default is inconsistent with the rest of Unreal which (mostly) uses `Lerp` on scales. The decision 
			// to use `Eerp` by default here is partially due to the fact we are also dealing properly with scalar 
			// velocities in this node, and partially to try and not to lock this node into having the same less 
			// accurate behavior by default. Users still have the option to interpolate scales with `Lerp` if they want.
			if (bLinearlyInterpolateScales)
			{
				InOutPose[BoneIndex].SetScale3D(FMath::Lerp(InOutPose[BoneIndex].GetScale3D(), ExtrapolatedScale, Alpha));
			}
			else
			{
				InOutPose[BoneIndex].SetScale3D(UE::Anim::DeadBlending::Private::VectorEerp(InOutPose[BoneIndex].GetScale3D(), ExtrapolatedScale, Alpha));
			}
		}
	}

	// Compute Blend Alpha

	const float CurveAlpha = 1.0f - FAlphaBlend::AlphaToBlendOption(
		InertializationTime / FMath::Max(InertializationDuration, UE_SMALL_NUMBER),
		InertializationBlendMode, InertializationCustomBlendCurve);

	// Blend Curves

	UE::Anim::FNamedValueArrayUtils::Union(InOutCurves, CurveData,
		[CurveAlpha, this](UE::Anim::FCurveElement& OutResultElement, const FDeadBlendingCurveElement& InElement1, UE::Anim::ENamedValueUnionFlags InFlags)
		{
			// Compute Extrapolated Curve Value

			const float ExtrapolatedCurve = UE::Anim::DeadBlending::Private::ExtrapolateCurve(
				InElement1.Value,
				InElement1.Velocity,
				InertializationTime,
				InElement1.HalfLife);

#if WITH_EDITORONLY_DATA
			if (bShowExtrapolations)
			{
				OutResultElement.Value = ExtrapolatedCurve;
				OutResultElement.Flags |= InElement1.Flags;
				return;
			}
#endif

			OutResultElement.Value = FMath::Lerp(OutResultElement.Value, ExtrapolatedCurve, CurveAlpha);
			OutResultElement.Flags |= InElement1.Flags;
		});
}

class USkeleton* FAnimNode_DeadBlending::GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle)
{
	bInvalidSkeletonIsError = false;

	if (const IAnimClassInterface* AnimClassInterface = GetAnimClassInterface())
	{
		return AnimClassInterface->GetTargetSkeleton();
	}

	return nullptr;
}

FAnimNode_DeadBlending::FAnimNode_DeadBlending() {}

void FAnimNode_DeadBlending::RequestInertialization(const FInertializationRequest& Request)
{
	if (Request.Duration >= 0.0f)
	{
		RequestQueue.AddUnique(Request);
	}
}

void FAnimNode_DeadBlending::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/DeadBlending"));
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);

	CurveFilter.Empty();
	CurveFilter.SetFilterMode(UE::Anim::ECurveFilterMode::DisallowFiltered);
	CurveFilter.AppendNames(FilteredCurves);

	BoneFilter.Init(FCompactPoseBoneIndex(INDEX_NONE), FilteredBones.Num());

	PoseSnapshots.Empty(UE::Anim::DeadBlending::Private::MaxPoseSnapShotNum);

	RequestQueue.Reserve(8);

	const int32 NumSkeletonBones = bPreallocateMemory ? Context.AnimInstanceProxy->GetSkeleton()->GetReferenceSkeleton().GetNum() : 0;

	BoneValid.Empty(NumSkeletonBones);
	BoneTranslations.Empty(NumSkeletonBones);
	BoneRotations.Empty(NumSkeletonBones);
	BoneRotationDirections.Empty(NumSkeletonBones);
	BoneScales.Empty(NumSkeletonBones);

	BoneTranslationVelocities.Empty(NumSkeletonBones);
	BoneRotationVelocities.Empty(NumSkeletonBones);
	BoneScaleVelocities.Empty(NumSkeletonBones);

	BoneTranslationDecayHalfLives.Empty(NumSkeletonBones);
	BoneRotationDecayHalfLives.Empty(NumSkeletonBones);
	BoneScaleDecayHalfLives.Empty(NumSkeletonBones);

	CurveData.Empty();

	DeltaTime = 0.0f;

	InertializationState = EInertializationState::Inactive;
	InertializationTime = 0.0f;

	InertializationDuration = 0.0f;
	InertializationDurationPerBone.Empty(NumSkeletonBones);
	InertializationMaxDuration = 0.0f;

	InertializationBlendMode = DefaultBlendMode;
	InertializationCustomBlendCurve = DefaultCustomBlendCurve;
}


void FAnimNode_DeadBlending::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);

	FAnimNode_Base::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);

	// Compute Compact Pose Bone Index for each bone in Filter

	const FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
	for (int32 FilterBoneIdx = 0; FilterBoneIdx < FilteredBones.Num(); FilterBoneIdx++)
	{
		FilteredBones[FilterBoneIdx].Initialize(Context.AnimInstanceProxy->GetSkeleton());
		BoneFilter[FilterBoneIdx] = FilteredBones[FilterBoneIdx].GetCompactPoseIndex(RequiredBones);
	}
}


void FAnimNode_DeadBlending::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/DeadBlending"));
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);

	const int32 NodeId = Context.GetCurrentNodeId();
	const FAnimInstanceProxy& Proxy = *Context.AnimInstanceProxy;

	// Allow nodes further towards the leaves to inertialize using this node
	UE::Anim::TScopedGraphMessage<UE::Anim::FDeadBlendingRequester> Inertialization(Context, Context, this);

	// Handle skipped updates for cached poses by forwarding to inertialization nodes in those residual stacks
	UE::Anim::TScopedGraphMessage<UE::Anim::FCachedPoseSkippedUpdateHandler> CachedPoseSkippedUpdate(Context, [this, NodeId, &Proxy](TArrayView<const UE::Anim::FMessageStack> InSkippedUpdates)
	{
		// If we have a pending request forward the request to other Inertialization nodes
		// that were skipped due to pose caching.
		if (RequestQueue.Num() > 0)
		{
			// Cached poses have their Update function called once even though there may be multiple UseCachedPose nodes for the same pose.
			// Because of this, there may be Inertialization ancestors of the UseCachedPose nodes that missed out on requests.
			// So here we forward 'this' node's requests to the ancestors of those skipped UseCachedPose nodes.
			// Note that in some cases, we may be forwarding the requests back to this same node.  Those duplicate requests will ultimately
			// be ignored by the 'AddUnique' in the body of FAnimNode_DeadBlending::RequestInertialization.
			for (const UE::Anim::FMessageStack& Stack : InSkippedUpdates)
			{
				Stack.ForEachMessage<UE::Anim::IInertializationRequester>([this, NodeId, &Proxy](UE::Anim::IInertializationRequester& InMessage)
				{
					for (const FInertializationRequest& Request : RequestQueue)
					{
						InMessage.RequestInertialization(Request);
					}
					InMessage.AddDebugRecord(Proxy, NodeId);

					return UE::Anim::FMessageStack::EEnumerate::Stop;
				});
			}
		}
	});

	Source.Update(Context);

	// Accumulate delta time between calls to Evaluate_AnyThread
	DeltaTime += Context.GetDeltaTime();
}

void FAnimNode_DeadBlending::Evaluate_AnyThread(FPoseContext& Output)
{
	LLM_SCOPE_BYNAME(TEXT("Animation/DeadBlending"));
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	// Evaluate the Input and write it to the Output

	Source.Evaluate(Output);

	// Automatically detect teleports... note that we do the teleport distance check against the root bone's location (world space) rather
	// than the mesh component's location because we still want to inertialize instances where the skeletal mesh component has been moved
	// while simultaneously counter-moving the root bone (as is the case when mounting and dismounting vehicles for example)

	const FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();

	bool bTeleported = false;

	const float TeleportDistanceThreshold = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetTeleportDistanceThreshold();

	if (PoseSnapshots.Num() > 0 && TeleportDistanceThreshold > 0.0f)
	{
		const FVector RootWorldSpaceLocation = ComponentTransform.TransformPosition(Output.Pose[FCompactPoseBoneIndex(0)].GetTranslation());
		const FVector PrevRootWorldSpaceLocation = PoseSnapshots.Last().ComponentTransform.TransformPosition(PoseSnapshots.Last().BoneTransforms[0].GetTranslation());

		if (FVector::DistSquared(RootWorldSpaceLocation, PrevRootWorldSpaceLocation) > FMath::Square(TeleportDistanceThreshold))
		{
			bTeleported = true;
		}
	}

	// If teleported we simply reset the inertialization

	if (bTeleported)
	{
		Deactivate();
	}
	
	// If we don't have any Pose Snapshots recorded it means this is the first time this node has been evaluated in 
	// which case there shouldn't be any discontinuity to remove, so no inertialization needs to be done, and we can 
	// discard any requests.

	if (PoseSnapshots.IsEmpty())
	{
		RequestQueue.Reset();
	}

	// Process Inertialization Requests

	if (!RequestQueue.IsEmpty())
	{
		const int32 NumSkeletonBones = UE::Anim::DeadBlending::Private::GetNumSkeletonBones(Output.AnimInstanceProxy->GetRequiredBones());

		// Find shortest request

		int32 ShortestRequestIdx = INDEX_NONE;
		float ShortestRequestDuration = UE_MAX_FLT;
		for (int32 RequestIdx = 0; RequestIdx < RequestQueue.Num(); RequestIdx++)
		{
			if (RequestQueue[RequestIdx].Duration < ShortestRequestDuration)
			{
				ShortestRequestIdx = RequestIdx;
				ShortestRequestDuration = RequestQueue[RequestIdx].Duration;
			}
		}

		// Record Request

		InertializationTime = 0.0f;
		InertializationDuration = BlendTimeMultiplier * RequestQueue[ShortestRequestIdx].Duration;
		InertializationDurationPerBone.Init(InertializationDuration, NumSkeletonBones);

		if (RequestQueue[ShortestRequestIdx].BlendProfile)
		{
			RequestQueue[ShortestRequestIdx].BlendProfile->FillSkeletonBoneDurationsArray(InertializationDurationPerBone, InertializationDuration);
		}
		else if (DefaultBlendProfile)
		{
			DefaultBlendProfile->FillSkeletonBoneDurationsArray(InertializationDurationPerBone, InertializationDuration);
		}

		// Cache the maximum duration across all bones (so we know when to deactivate the inertialization request)
		InertializationMaxDuration = FMath::Max(InertializationDuration, *Algo::MaxElement(InertializationDurationPerBone));

		if (RequestQueue[ShortestRequestIdx].bUseBlendMode)
		{
			InertializationBlendMode = RequestQueue[ShortestRequestIdx].BlendMode;
			InertializationCustomBlendCurve = RequestQueue[ShortestRequestIdx].CustomBlendCurve;
		}

#if ANIM_TRACE_ENABLED
		InertializationRequestDescription = RequestQueue[ShortestRequestIdx].Description;
		InertializationRequestNodeId = RequestQueue[ShortestRequestIdx].NodeId;
		InertializationRequestAnimInstance = RequestQueue[ShortestRequestIdx].AnimInstance;
#endif

		// Override with defaults

		if (bAlwaysUseDefaultBlendSettings)
		{
			InertializationDuration = BlendTimeMultiplier * DefaultBlendDuration;
			InertializationDurationPerBone.Init(BlendTimeMultiplier * DefaultBlendDuration, NumSkeletonBones);
			InertializationMaxDuration = BlendTimeMultiplier * DefaultBlendDuration;
			InertializationBlendMode = DefaultBlendMode;
			InertializationCustomBlendCurve = DefaultCustomBlendCurve;
		}

		check(InertializationDuration != UE_MAX_FLT);
		check(InertializationMaxDuration != UE_MAX_FLT);

		// Reset Request Queue

		RequestQueue.Reset();

		// Initialize the recorded pose state at the point of transition

		if (PoseSnapshots.Num() > 1)
		{
			// We have two previous poses and so can initialize as normal.

			InitFrom(
				Output.Pose,
				Output.Curve,
				PoseSnapshots[PoseSnapshots.Num() - 2],
				PoseSnapshots[PoseSnapshots.Num() - 1]);
		}
		else if (PoseSnapshots.Num() > 0)
		{
			// We only have a single previous pose. Repeat this pose assuming zero velocity.

			InitFrom(
				Output.Pose,
				Output.Curve,
				PoseSnapshots.Last(),
				PoseSnapshots.Last());
		}
		else
		{
			// This should never happen because we are not able to issue an inertialization 
			// requested until we have at least one pose recorded in the snapshots.
			check(false);
		}

		// Set state to active

		InertializationState = EInertializationState::Active;
	}

	// Update Time Since Transition and deactivate if blend is over

	if (InertializationState == EInertializationState::Active)
	{
		InertializationTime += DeltaTime;

		if (InertializationTime >= InertializationMaxDuration)
		{
			Deactivate();
		}
	}

	// Apply inertialization

	if (InertializationState == EInertializationState::Active)
	{
		ApplyTo(Output.Pose, Output.Curve);
	}

	// Find AttachParentName

	FName AttachParentName = NAME_None;
	if (AActor* Owner = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetOwner())
	{
		if (AActor* AttachParentActor = Owner->GetAttachParentActor())
		{
			AttachParentName = AttachParentActor->GetFName();
		}
	}

	// Record Pose Snapshot

	if (PoseSnapshots.Num() < UE::Anim::DeadBlending::Private::MaxPoseSnapShotNum)
	{
		// Add the pose to the end of the buffer
		PoseSnapshots.AddDefaulted_GetRef().InitFrom(Output.Pose, Output.Curve, ComponentTransform, AttachParentName, DeltaTime);
	}
	else
	{
		// Bubble the old poses forward in the buffer (using swaps to avoid allocations and copies)
		for (int32 SnapshotIndex = 0; SnapshotIndex < UE::Anim::DeadBlending::Private::MaxPoseSnapShotNum - 1; ++SnapshotIndex)
		{
			Swap(PoseSnapshots[SnapshotIndex], PoseSnapshots[SnapshotIndex + 1]);
		}

		// Overwrite the (now irrelevant) pose in the last slot with the new post snapshot
		// (thereby avoiding the reallocation costs we would have incurred had we simply added a new pose at the end)
		PoseSnapshots.Last().InitFrom(Output.Pose, Output.Curve, ComponentTransform, AttachParentName, DeltaTime);
	}

	// Reset Delta Time

	DeltaTime = 0.0f;

	const float InertializationWeight = InertializationState == EInertializationState::Active ?
		1.0f - FAlphaBlend::AlphaToBlendOption(
			InertializationTime / FMath::Max(InertializationDuration, UE_SMALL_NUMBER),
			InertializationBlendMode, InertializationCustomBlendCurve) : 0.0f;

	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("State"), *UEnum::GetValueAsString(InertializationState));
	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("Elapsed Time"), InertializationTime);
	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("Duration"), InertializationDuration);
	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("Max Duration"), InertializationMaxDuration);
	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("Normalized Time"), InertializationDuration > UE_KINDA_SMALL_NUMBER ? (InertializationTime / InertializationDuration) : 0.0f);
	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("Inertialization Weight"), InertializationWeight);
	TRACE_ANIM_NODE_VALUE_WITH_ID(Output, GetNodeIndex(), TEXT("Request Description"), *InertializationRequestDescription.ToString());
	TRACE_ANIM_NODE_VALUE_WITH_ID_ANIM_NODE(Output, GetNodeIndex(), TEXT("Request Node"), InertializationRequestNodeId, InertializationRequestAnimInstance);
}

bool FAnimNode_DeadBlending::NeedsDynamicReset() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE
