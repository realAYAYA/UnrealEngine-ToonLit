// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_RetargetPoseFromMesh)


void FAnimNode_RetargetPoseFromMesh::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
    FAnimNode_Base::Initialize_AnyThread(Context);

	// Initial update of the node, so we dont have a frame-delay on setup
	GetEvaluateGraphExposedInputs().Execute(Context);

	if (!Processor && IsInGameThread())
	{
		Processor = NewObject<UIKRetargetProcessor>(Context.AnimInstanceProxy->GetSkelMeshComponent());	
	}
}

void FAnimNode_RetargetPoseFromMesh::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)

	const FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
	if (!RequiredBones.IsValid())
	{
		return;
	}
	
	if (!Context.AnimInstanceProxy->GetSkelMeshComponent()->GetSkeletalMeshAsset())
	{
		return;
	}

	// rebuild mapping
	RequiredToTargetBoneMapping.Reset();

	const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();
	const FReferenceSkeleton& TargetSkeleton = Context.AnimInstanceProxy->GetSkelMeshComponent()->GetSkeletalMeshAsset()->GetRefSkeleton();
	const TArray<FBoneIndexType>& RequiredBonesArray = RequiredBones.GetBoneIndicesArray();
	for (int32 Index = 0; Index < RequiredBonesArray.Num(); ++Index)
	{
		const FBoneIndexType ReqBoneIndex = RequiredBonesArray[Index]; 
		if (ReqBoneIndex != INDEX_NONE)
		{
			const FName Name = RefSkeleton.GetBoneName(ReqBoneIndex);
			const int32 TargetBoneIndex = TargetSkeleton.FindBoneIndex(Name);
			if (TargetBoneIndex != INDEX_NONE)
			{
				// store require bone to target bone indices
				RequiredToTargetBoneMapping.Emplace(Index, TargetBoneIndex);
			}
		}
	}
}

void FAnimNode_RetargetPoseFromMesh::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)
	FAnimNode_Base::Update_AnyThread(Context);
    // this introduces a frame of latency in setting the pin-driven source component,
    // but we cannot do the work to extract transforms on a worker thread as it is not thread safe.
    GetEvaluateGraphExposedInputs().Execute(Context);
	DeltaTime += Context.GetDeltaTime();
}

void FAnimNode_RetargetPoseFromMesh::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)

	SCOPE_CYCLE_COUNTER(STAT_IKRetarget);

	if (!(IKRetargeterAsset && Processor && SourceMeshComponent.IsValid()))
	{
		Output.ResetToRefPose();
		return;
	}

	// it's possible in editor to have anim instances initialized before PreUpdate() is called
	// which results in trying to run the retargeter without an source pose to copy from
	const bool bSourcePoseCopied = !SourceMeshComponentSpaceBoneTransforms.IsEmpty();

	// ensure processor was initialized with the currently used assets (source/target meshes and retarget asset)
	// if processor is not ready this tick, it will be next tick as this state will trigger re-initialization
	const TObjectPtr<USkeletalMesh> SourceMesh = SourceMeshComponent->GetSkeletalMeshAsset();
	const TObjectPtr<USkeletalMesh> TargetMesh = Output.AnimInstanceProxy->GetSkelMeshComponent()->GetSkeletalMeshAsset();
	const bool bIsProcessorReady = Processor->WasInitializedWithTheseAssets(SourceMesh, TargetMesh, IKRetargeterAsset);

	// if not ready to run, skip retarget and output the ref pose
	if (!(bIsProcessorReady && bSourcePoseCopied))
	{
		Output.ResetToRefPose();
		return;
	}

#if WITH_EDITOR
	// live preview source asset settings in the retarget, editor only
	// NOTE: this copies goal targets as well, but these are overwritten by IK chain goals
	if (bDriveWithAsset)
	{
		Processor->ApplySettingsFromAsset();
		if (const FRetargetProfile* CurrentProfile = IKRetargeterAsset->GetCurrentProfile())
		{
			Processor->ApplySettingsFromProfile(*CurrentProfile);
		}
	}
#endif

	// apply custom profile settings to the processor
	Processor->ApplySettingsFromProfile(CustomRetargetProfile);

	// run the retargeter
	const TArray<FTransform>& RetargetedPose = Processor->RunRetargeter(SourceMeshComponentSpaceBoneTransforms, SpeedValuesFromCurves, DeltaTime);
	DeltaTime = 0.0f;

	// copy pose back
	FCSPose<FCompactPose> ComponentPose;
	ComponentPose.InitPose(Output.Pose);
	const FCompactPose& CompactPose = ComponentPose.GetPose();
	for (const TPair<int32, int32>& Pair : RequiredToTargetBoneMapping)
	{
		const FCompactPoseBoneIndex CompactBoneIndex(Pair.Key);
		if (CompactPose.IsValidIndex(CompactBoneIndex))
		{
			const int32 TargetBoneIndex = Pair.Value;
			ComponentPose.SetComponentSpaceTransform(CompactBoneIndex, RetargetedPose[TargetBoneIndex]);
		}
	}

	// convert to local space
	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(ComponentPose, Output.Pose);

	// copy curves over
	if (bCopyCurves)
	{
		for (const TPair<FName, float>& SourceCurve : SourceCurveValues)
		{
			if (const SmartName::UID_Type* UID = CurveNameToUIDMap.Find(SourceCurve.Key))
			{
				// set source value to output curve
				Output.Curve.Set(*UID, SourceCurve.Value);
			}
		}
	}
}

void FAnimNode_RetargetPoseFromMesh::PreUpdate(const UAnimInstance* InAnimInstance)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(PreUpdate)
	
	if (!IKRetargeterAsset)
	{
		return;
	}
	
	if (!Processor)
	{
		Processor = NewObject<UIKRetargetProcessor>(InAnimInstance->GetOwningComponent());	
	}

	const TObjectPtr<USkeletalMeshComponent> TargetMeshComponent = InAnimInstance->GetSkelMeshComponent();
	if (EnsureProcessorIsInitialized(TargetMeshComponent))
	{
		CopyBoneTransformsFromSource(TargetMeshComponent);

		if(bCopyCurves)
		{
			SourceCurveValues.Reset();
			if (const UAnimInstance* SourceAnimInstance = SourceMeshComponent->GetAnimInstance())
			{
				// attribute curve contains all list	
				SourceCurveValues.Append(SourceAnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve));
			}

			UpdateSpeedValuesFromCurves();
		}
	}
}

void FAnimNode_RetargetPoseFromMesh::UpdateSpeedValuesFromCurves()
{
	SpeedValuesFromCurves.Reset();
	
	if (!IKRetargeterAsset)
	{
		return;
	}

	const TArray<TObjectPtr<URetargetChainSettings>>& ChainSettings = IKRetargeterAsset->GetAllChainSettings();
	for (const TObjectPtr<URetargetChainSettings>& ChainSetting : ChainSettings)
	{
		FName CurveName = ChainSetting->Settings.SpeedPlanting.SpeedCurveName;
		if (CurveName == NAME_None)
		{
			continue;
		}

		// value will be negative if the curve was not found (retargeter ignores negative speeds)
		float CurveValue = -1.0f;
		if (SourceCurveValues.Contains(CurveName))
		{
			CurveValue = SourceCurveValues[CurveName];
		}
		SpeedValuesFromCurves.Add(CurveName, CurveValue);
	}
}

UIKRetargetProcessor* FAnimNode_RetargetPoseFromMesh::GetRetargetProcessor() const
{
	return Processor;
}

bool FAnimNode_RetargetPoseFromMesh::EnsureProcessorIsInitialized(const TObjectPtr<USkeletalMeshComponent> TargetMeshComponent)
{
	// has user supplied a retargeter asset?
	if (!IKRetargeterAsset)
	{
		return false;
	}
	
	// if user hasn't explicitly connected a source mesh, optionally use the parent mesh component (if there is one) 
	if (!SourceMeshComponent.IsValid() && bUseAttachedParent)
	{
		// Walk up the attachment chain until we find a skeletal mesh component
		USkeletalMeshComponent* ParentComponent = nullptr;
		for (USceneComponent* AttachParentComp = TargetMeshComponent->GetAttachParent(); AttachParentComp != nullptr; AttachParentComp = AttachParentComp->GetAttachParent())
		{
			ParentComponent = Cast<USkeletalMeshComponent>(AttachParentComp);
			if (ParentComponent)
			{
				break;
			}
		}

		if (ParentComponent)
		{
			SourceMeshComponent = ParentComponent;
		}
	}
	
	// has a source mesh been plugged in or found?
	if (!SourceMeshComponent.IsValid())
	{
		return false; // can't do anything if we don't have a source mesh component
	}

	// check that both a source and target mesh exist
	const TObjectPtr<USkeletalMesh> SourceMesh = SourceMeshComponent->GetSkeletalMeshAsset();
	const TObjectPtr<USkeletalMesh> TargetMesh = TargetMeshComponent->GetSkeletalMeshAsset();
	if (!SourceMesh || !TargetMesh)
	{
		return false; // cannot initialize if components are missing skeletal mesh references
	}
	// check that both have skeleton assets (shouldn't get this far without a skeleton)
	const TObjectPtr<USkeleton> SourceSkeleton = SourceMesh->GetSkeleton();
	const TObjectPtr<USkeleton> TargetSkeleton = TargetMesh->GetSkeleton();
	if (!SourceSkeleton || !TargetSkeleton)
	{
		return false;
	}
	
	// try initializing the processor
	if (!Processor->WasInitializedWithTheseAssets(SourceMesh, TargetMesh, IKRetargeterAsset))
	{
		// initialize retarget processor with source and target skeletal meshes
		// (asset is passed in as outer UObject for new UIKRigProcessor) 
		Processor->Initialize(SourceMesh,	TargetMesh,IKRetargeterAsset);

		// create a map of curve names to IDs that are present on the target skeleton
		if (bCopyCurves)
		{
			const FSmartNameMapping* SourceContainer = SourceSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
			const FSmartNameMapping* TargetContainer = TargetSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

			TArray<FName> SourceCurveNames;
			SourceContainer->FillNameArray(SourceCurveNames);
			CurveNameToUIDMap.Reset();
			for (int32 Index = 0; Index < SourceCurveNames.Num(); ++Index)
			{
				SmartName::UID_Type UID = TargetContainer->FindUID(SourceCurveNames[Index]);
				if (UID != SmartName::MaxUID)
				{
					// has a valid UID, add to the list
					SmartName::UID_Type& Value = CurveNameToUIDMap.Add(SourceCurveNames[Index]);
					Value = UID;
				}
			}
		}
	}

	return Processor->IsInitialized();
}

void FAnimNode_RetargetPoseFromMesh::CopyBoneTransformsFromSource(USkeletalMeshComponent* TargetMeshComponent)
{
	// get the mesh component to use as the source
	const TObjectPtr<USkeletalMeshComponent> ComponentToCopyFrom =  GetComponentToCopyPoseFrom();

	// this should not happen as we're guaranteed to be initialized at this stage
	// but just in case component is lost after initialization, we avoid a crash
	if (!ComponentToCopyFrom)
	{
		return; 
	}
	
	// skip copying pose when component is no longer ticking
	if (!ComponentToCopyFrom->IsRegistered())
	{
		return; 
	}
	
	const bool bUROInSync =
		ComponentToCopyFrom->ShouldUseUpdateRateOptimizations() &&
		ComponentToCopyFrom->AnimUpdateRateParams != nullptr &&
		SourceMeshComponent->AnimUpdateRateParams == TargetMeshComponent->AnimUpdateRateParams;
	const bool bUsingExternalInterpolation = ComponentToCopyFrom->IsUsingExternalInterpolation();
	const TArray<FTransform>& CachedComponentSpaceTransforms = ComponentToCopyFrom->GetCachedComponentSpaceTransforms();
	const bool bArraySizesMatch = CachedComponentSpaceTransforms.Num() == ComponentToCopyFrom->GetComponentSpaceTransforms().Num();

	// copy source array from the appropriate location
	SourceMeshComponentSpaceBoneTransforms.Reset();
	if ((bUROInSync || bUsingExternalInterpolation) && bArraySizesMatch)
	{
		SourceMeshComponentSpaceBoneTransforms.Append(CachedComponentSpaceTransforms); // copy from source's cache
	}
	else
	{
		SourceMeshComponentSpaceBoneTransforms.Append(ComponentToCopyFrom->GetComponentSpaceTransforms()); // copy directly
	}
}

TObjectPtr<USkeletalMeshComponent> FAnimNode_RetargetPoseFromMesh::GetComponentToCopyPoseFrom() const
{
	// if our source is running under leader-pose, then get bone data from there
	if (SourceMeshComponent.IsValid())
	{
		if(USkeletalMeshComponent* LeaderPoseComponent = Cast<USkeletalMeshComponent>(SourceMeshComponent->LeaderPoseComponent.Get()))
		{
			return LeaderPoseComponent;
		}
	}
	
	return SourceMeshComponent.Get();
}

