// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNode_ControlRigBase.h"
#include "ControlRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/NodeMappingContainer.h"
#include "AnimationRuntime.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Algo/Transform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_ControlRigBase)

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimNodeControlRigDebug(TEXT("a.AnimNode.ControlRig.Debug"), 0, TEXT("Set to 1 to turn on debug drawing for AnimNode_ControlRigBase"));
#endif

// CVar to disable control rig execution within an anim node
static TAutoConsoleVariable<int32> CVarControlRigDisableExecutionAnimNode(TEXT("ControlRig.DisableExecutionInAnimNode"), 0, TEXT("if nonzero we disable the execution of Control Rigs inside an anim node."));

FAnimNode_ControlRigBase::FAnimNode_ControlRigBase()
	: FAnimNode_CustomProperty()
	, bResetInputPoseToInitial(true) 
	, bTransferInputPose(true)
	, bTransferInputCurves(true)
	, bTransferPoseInGlobalSpace(true)
	, InputSettings(FControlRigIOSettings())
	, OutputSettings(FControlRigIOSettings())
	, bExecute(true)
	, InternalBlendAlpha (1.f)
	, bControlRigRequiresInitialization(true)
	, LastBonesSerialNumberForCacheBones(0)
{

}

void FAnimNode_ControlRigBase::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::OnInitializeAnimInstance(InProxy, InAnimInstance);

	USkeletalMeshComponent* Component = InAnimInstance->GetOwningComponent();
	UControlRig* ControlRig = GetControlRig();
	if (Component && Component->GetSkeletalMeshAsset() && ControlRig)
	{
#if WITH_EDITORONLY_DATA
		UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(ControlRig->GetClass());
		if (BlueprintClass)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy);
			// node mapping container will be saved on the initialization part
			NodeMappingContainer = Component->GetSkeletalMeshAsset()->GetNodeMappingContainer(Blueprint);
		}
#endif

		// register skeletalmesh component for now
		ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, InAnimInstance->GetOwningComponent());
	}
}

void FAnimNode_ControlRigBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::Initialize_AnyThread(Context);
	Source.Initialize(Context);

	if (UControlRig* ControlRig = GetControlRig())
	{
		//Don't Inititialize the Control Rig here it may have the wrong VM on the CDO
		SetTargetInstance(ControlRig);
		ControlRig->RequestInit();
		bControlRigRequiresInitialization = true;
		LastBonesSerialNumberForCacheBones = 0;
	}
}

void FAnimNode_ControlRigBase::GatherDebugData(FNodeDebugData& DebugData)
{
	Source.GatherDebugData(DebugData.BranchFlow(1.f));
}

void FAnimNode_ControlRigBase::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::Update_AnyThread(Context);
	Source.Update(Context);

	if (bExecute)
	{
		if (UControlRig* ControlRig = GetControlRig())
		{
			// @TODO: fix this to be thread-safe
			// Pre-update doesn't work for custom anim instances
			// FAnimNode_ControlRigExternalSource needs this to be called to reset to ref pose
			ControlRig->SetDeltaTime(Context.GetDeltaTime());
		}
	}
}

bool FAnimNode_ControlRigBase::CanExecute()
{
	if(CVarControlRigDisableExecutionAnimNode->GetInt() != 0)
	{
		return false;
	}

	if (UControlRig* ControlRig = GetControlRig())
	{
		return ControlRig->CanExecute(); 
	}

	return false;
}

void FAnimNode_ControlRigBase::UpdateInput(UControlRig* ControlRig, const FPoseContext& InOutput)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_UpdateInput);


	if(!CanExecute())
	{
		return;
	}

#if WITH_EDITOR
	// if we are recording any change - let's clear the undo stack
	if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
	{
		if(Hierarchy->IsTracingChanges())
		{
			Hierarchy->ResetTransformStack();
		}
	}
#endif

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (InputSettings.bUpdatePose && bTransferInputPose)
	{
		const FBoneContainer& RequiredBones = InOutput.Pose.GetBoneContainer();

		// reset transforms here to prevent additive transforms from accumulating to INF
		// we only update transforms from the mesh pose for bones in the current LOD, 
		// so the reset here ensures excluded bones are also reset
		if(!ControlRigBoneInputMappingByName.IsEmpty() || bResetInputPoseToInitial)
		{
			{
				FRigHierarchyValidityBracket ValidityBracket(ControlRig->GetHierarchy());
			}
			FRigHierarchyValidityBracket ValidityBracket(ControlRig->GetHierarchy());

			{
#if WITH_EDITOR
				// make sure transient controls don't get reset
				UControlRig::FTransientControlPoseScope PoseScope(ControlRig);
#endif 
				ControlRig->GetHierarchy()->ResetPoseToInitial(ERigElementType::Bone);
			}
		}

		if(bTransferPoseInGlobalSpace || NodeMappingContainer.IsValid())
		{
			// get component pose from control rig
			FCSPose<FCompactPose> MeshPoses;
			// first I need to convert to local pose
			MeshPoses.InitPose(InOutput.Pose);

			if(!ControlRigBoneInputMappingByIndex.IsEmpty())
			{
				for (const TPair<uint16, uint16>& Pair : ControlRigBoneInputMappingByIndex)
				{
					const uint16 ControlRigIndex = Pair.Key;
					const uint16 SkeletonIndex = Pair.Value;
					
					FCompactPoseBoneIndex CompactPoseIndex(SkeletonIndex);
					FTransform ComponentTransform = MeshPoses.GetComponentSpaceTransform(CompactPoseIndex);
					ControlRig->GetHierarchy()->SetGlobalTransformByIndex(ControlRigIndex, ComponentTransform, false);
				}
			}
			else
			{
				for (auto Iter = ControlRigBoneInputMappingByName.CreateConstIterator(); Iter; ++Iter)
				{
					const FName& Name = Iter.Key();
					const uint16 Index = Iter.Value();
					const FRigElementKey Key(Name, ERigElementType::Bone);

					FCompactPoseBoneIndex CompactPoseIndex(Index);
					FTransform ComponentTransform = MeshPoses.GetComponentSpaceTransform(CompactPoseIndex);
					if (NodeMappingContainer.IsValid())
					{
						ComponentTransform = NodeMappingContainer->GetSourceToTargetTransform(Name).GetRelativeTransformReverse(ComponentTransform);
					}
					ControlRig->GetHierarchy()->SetGlobalTransform(Key, ComponentTransform, false);
				}
			}
		}
		else
		{
			if(!ControlRigBoneInputMappingByIndex.IsEmpty())
			{
				for (const TPair<uint16, uint16>& Pair : ControlRigBoneInputMappingByIndex)
				{
					const uint16 ControlRigIndex = Pair.Key;
					const uint16 SkeletonIndex = Pair.Value;
					
					FCompactPoseBoneIndex CompactPoseIndex(SkeletonIndex);
					FTransform LocalTransform = InOutput.Pose[CompactPoseIndex];
					ControlRig->GetHierarchy()->SetLocalTransformByIndex(ControlRigIndex, LocalTransform, false);
				}
			}
			else
			{
				for (auto Iter = ControlRigBoneInputMappingByName.CreateConstIterator(); Iter; ++Iter)
				{
					const FName& Name = Iter.Key();
					const uint16 SkeletonIndex = Iter.Value();
					const FRigElementKey Key(Name, ERigElementType::Bone);

					FCompactPoseBoneIndex CompactPoseIndex(SkeletonIndex);
					FTransform LocalTransform = InOutput.Pose[CompactPoseIndex];
					ControlRig->GetHierarchy()->SetLocalTransform(Key, LocalTransform, false);
				}
			}
		}
		
	}

	if (InputSettings.bUpdateCurves && bTransferInputCurves)
	{
		if(!ControlRigCurveMappingByIndex.IsEmpty())
		{
			for (const TPair<uint16, uint16>& Pair : ControlRigCurveMappingByIndex)
			{
				const uint16 ControlRigIndex = Pair.Key;
				const uint16 SkeletonIndex = Pair.Value;

				bool bIsValid;
				const float Value = InOutput.Curve.Get(SkeletonIndex, bIsValid);
				if (bIsValid)
				{
					ControlRig->GetHierarchy()->SetCurveValueByIndex(ControlRigIndex, Value);
				}
				else
				{
					ControlRig->GetHierarchy()->UnsetCurveValueByIndex(ControlRigIndex);
				}
			}
		}
		else
		{
			for (auto Iter = ControlRigCurveMappingByName.CreateConstIterator(); Iter; ++Iter)
			{
				const FName& Name = Iter.Key();
				const uint16 SkeletonIndex = Iter.Value();
				const FRigElementKey Key(Name, ERigElementType::Curve);

				bool bIsValid;
				const float Value = InOutput.Curve.Get(SkeletonIndex, bIsValid);
				if (bIsValid)
				{
					ControlRig->GetHierarchy()->SetCurveValue(Key, Value);
				}
				else
				{
					ControlRig->GetHierarchy()->UnsetCurveValue(Key);
				}
			}
		}
	}

#if WITH_EDITOR
	if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
	{
		if(Hierarchy->IsTracingChanges())
		{
			Hierarchy->StorePoseForTrace(TEXT("FAnimNode_ControlRigBase::UpdateInput"));
		}
	}
#endif
}

void FAnimNode_ControlRigBase::UpdateOutput(UControlRig* ControlRig, FPoseContext& InOutput)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_UpdateOutput);

	if(!CanExecute())
	{
		return;
	}

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (OutputSettings.bUpdatePose)
	{
		// copy output of the rig
		const FBoneContainer& RequiredBones = InOutput.Pose.GetBoneContainer();

		TMap<FName, uint16>& NameBasedMapping = ControlRigBoneOutputMappingByName;
		TArray<TPair<uint16, uint16>>& IndexBasedMapping = ControlRigBoneOutputMappingByIndex;

		// if we don't have a different mapping for outputs, use the input mapping
		if(NameBasedMapping.IsEmpty() && IndexBasedMapping.IsEmpty())
		{
			NameBasedMapping = ControlRigBoneInputMappingByName;
			IndexBasedMapping = ControlRigBoneInputMappingByIndex;
		}

		if(bTransferPoseInGlobalSpace || NodeMappingContainer.IsValid())
		{
			// get component pose from control rig
			FCSPose<FCompactPose> MeshPoses;
			MeshPoses.InitPose(InOutput.Pose);

			if(!IndexBasedMapping.IsEmpty())
			{
				for (const TPair<uint16, uint16>& Pair : IndexBasedMapping)
				{
					const uint16 ControlRigIndex = Pair.Key;
					const uint16 SkeletonIndex = Pair.Value;

					FCompactPoseBoneIndex CompactPoseIndex(SkeletonIndex);
					FTransform ComponentTransform = ControlRig->GetHierarchy()->GetGlobalTransformByIndex(ControlRigIndex);
					MeshPoses.SetComponentSpaceTransform(CompactPoseIndex, ComponentTransform);
				}
			}
			else
			{
				for (auto Iter = NameBasedMapping.CreateConstIterator(); Iter; ++Iter)
				{
					const FName& Name = Iter.Key();
					const uint16 SkeletonIndex = Iter.Value();
					const FRigElementKey Key(Name, ERigElementType::Bone);

					FCompactPoseBoneIndex CompactPoseIndex(SkeletonIndex);
					FTransform ComponentTransform = ControlRig->GetHierarchy()->GetGlobalTransform(Key);
					if (NodeMappingContainer.IsValid())
					{
						ComponentTransform = NodeMappingContainer->GetSourceToTargetTransform(Name) * ComponentTransform;
					}

					MeshPoses.SetComponentSpaceTransform(CompactPoseIndex, ComponentTransform);
				}
			}

			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPosesSafe(MeshPoses, InOutput.Pose);
			InOutput.Pose.NormalizeRotations();
		}
		else
		{
			if(!IndexBasedMapping.IsEmpty())
			{
				for (const TPair<uint16, uint16>& Pair : IndexBasedMapping)
				{
					const uint16 ControlRigIndex = Pair.Key;
					const uint16 SkeletonIndex = Pair.Value;

					FCompactPoseBoneIndex CompactPoseIndex(SkeletonIndex);
					FTransform LocalTransform = ControlRig->GetHierarchy()->GetLocalTransformByIndex(ControlRigIndex);
					InOutput.Pose[CompactPoseIndex] = LocalTransform;
				}
			}
			else
			{
				for (auto Iter = NameBasedMapping.CreateConstIterator(); Iter; ++Iter)
				{
					const FName& Name = Iter.Key();
					const uint16 Index = Iter.Value();
					const FRigElementKey Key(Name, ERigElementType::Bone);

					FCompactPoseBoneIndex CompactPoseIndex(Index);
					FTransform LocalTransform = ControlRig->GetHierarchy()->GetLocalTransform(Key);
					InOutput.Pose[CompactPoseIndex] = LocalTransform;
				}
			}
		}
	}

	if (OutputSettings.bUpdateCurves)
	{
		if(!ControlRigCurveMappingByIndex.IsEmpty())
		{
			for (const TPair<uint16, uint16>& Pair : ControlRigCurveMappingByIndex)
			{
				const uint16 ControlRigIndex = Pair.Key;
				const uint16 SkeletonIndex = Pair.Value;

				if (ControlRig->GetHierarchy()->IsCurveValueSetByIndex(ControlRigIndex))
				{
					InOutput.Curve.Set(SkeletonIndex, ControlRig->GetHierarchy()->GetCurveValueByIndex(ControlRigIndex));
				}
				else
				{
					const int32 WeightIndex = InOutput.Curve.GetArrayIndexByUID(SkeletonIndex);
					if (WeightIndex != INDEX_NONE)
					{
						InOutput.Curve.ValidCurveWeights[WeightIndex] = false;
					}
				}
			}
		}
		else
		{
			for (auto Iter = ControlRigCurveMappingByName.CreateConstIterator(); Iter; ++Iter)
			{
				const FName& Name = Iter.Key();
				const uint16 Index = Iter.Value();
				const FRigElementKey Key(Name, ERigElementType::Curve);

				if (ControlRig->GetHierarchy()->IsCurveValueSet(Key))
				{
					InOutput.Curve.Set(Index, ControlRig->GetHierarchy()->GetCurveValue(Key));
				}
				else
				{
					const int32 WeightIndex = InOutput.Curve.GetArrayIndexByUID(Index);
					if (WeightIndex != INDEX_NONE)
					{
						InOutput.Curve.ValidCurveWeights[WeightIndex] = false;
					}
				}
			}
		}
	}

#if WITH_EDITOR
	if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
	{
		if(Hierarchy->IsTracingChanges())
		{
			Hierarchy->StorePoseForTrace(TEXT("FAnimNode_ControlRigBase::UpdateOutput"));
			Hierarchy->DumpTransformStackToFile();
		}
	}
#endif
}

void FAnimNode_ControlRigBase::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FPoseContext SourcePose(Output);

	if (Source.GetLinkNode())
	{
		Source.Evaluate(SourcePose);
	}
	else
	{
		// apply refpose
		SourcePose.ResetToRefPose();
	}

	if (CanExecute() && FAnimWeight::IsRelevant(InternalBlendAlpha) && GetControlRig())
	{
		if (FAnimWeight::IsFullWeight(InternalBlendAlpha))
		{
			ExecuteControlRig(SourcePose);
			Output = SourcePose;
		}
		else 
		{
			// this blends additively - by weight
			FPoseContext ControlRigPose(SourcePose);
			ControlRigPose = SourcePose;
			ExecuteControlRig(ControlRigPose);

			FPoseContext AdditivePose(ControlRigPose);
			AdditivePose = ControlRigPose;
			FAnimationRuntime::ConvertPoseToAdditive(AdditivePose.Pose, SourcePose.Pose);
			AdditivePose.Curve.ConvertToAdditive(SourcePose.Curve);
			Output = SourcePose;

			UE::Anim::Attributes::ConvertToAdditive(SourcePose.CustomAttributes, AdditivePose.CustomAttributes);

			FAnimationPoseData BaseAnimationPoseData(Output);
			const FAnimationPoseData AdditiveAnimationPoseData(AdditivePose);
			FAnimationRuntime::AccumulateAdditivePose(BaseAnimationPoseData, AdditiveAnimationPoseData, InternalBlendAlpha, AAT_LocalSpaceBase);
		}
	}
	else // if not relevant, skip to run control rig
		// this may cause issue if we have simulation node in the control rig that accumulates time
	{
		Output = SourcePose;
	}
}

void FAnimNode_ControlRigBase::ExecuteControlRig(FPoseContext& InOutput)
{
	if (UControlRig* ControlRig = GetControlRig())
	{
		// temporarily give control rig access to the stack allocated attribute container
		// control rig may have rig units that can add/get attributes to/from this container
		UControlRig::FAnimAttributeContainerPtrScope AttributeScope(ControlRig, InOutput.CustomAttributes);
		
		// first update input to the system
		UpdateInput(ControlRig, InOutput);

		if (bExecute)
		{
#if WITH_EDITOR
			if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
			{
				if(Hierarchy->IsTracingChanges())
				{
					Hierarchy->StorePoseForTrace(TEXT("FAnimNode_ControlRigBase::BeforeEvaluate"));
				}
			}
#endif

			// pick the event to run
			if(EventQueue.IsEmpty())
			{
				if(bClearEventQueueRequired)
				{
					ControlRig->SetEventQueue({FRigUnit_BeginExecution::EventName});
					bClearEventQueueRequired = false;
				}
			}
			else
			{
				TArray<FName> EventNames;
				Algo::Transform(EventQueue, EventNames, [](const FControlRigAnimNodeEventName& InEventName) 
				{
					return InEventName.EventName;
				});
				ControlRig->SetEventQueue(EventNames);
				bClearEventQueueRequired = true;
			}
			
			// evaluate control rig
			ControlRig->Evaluate_AnyThread();

#if ENABLE_ANIM_DEBUG 
			// When Control Rig is at editing time (in CR editor), draw instructions are consumed by ControlRigEditMode, so we need to skip drawing here.
			bool bShowDebug = (CVarAnimNodeControlRigDebug.GetValueOnAnyThread() == 1 && ControlRig->ExecutionType != ERigExecutionType::Editing);

			if (bShowDebug)
			{ 
				QueueControlRigDrawInstructions(ControlRig, InOutput.AnimInstanceProxy);
			}
#endif

#if WITH_EDITOR
			if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
			{
				if(Hierarchy->IsTracingChanges())
				{
					Hierarchy->StorePoseForTrace(TEXT("FAnimNode_ControlRigBase::AfterEvaluate"));
				}
			}
#endif
		}

		// now update output
		UpdateOutput(ControlRig, InOutput);
	}
}

struct FControlRigControlScope
{
	FControlRigControlScope(UControlRig* InControlRig)
		: ControlRig(InControlRig)
	{
		if (ControlRig.IsValid())
		{
			URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
			Hierarchy->ForEach<FRigControlElement>([this, Hierarchy](FRigControlElement* ControlElement) -> bool
			{
				ControlValues.Add(ControlElement->GetKey(), Hierarchy->GetControlValueByIndex(ControlElement->GetIndex()));
				return true; // continue
			});
		}
	}

	~FControlRigControlScope()
	{
		if (ControlRig.IsValid())
		{
			URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
			for (const TPair<FRigElementKey, FRigControlValue>& Pair: ControlValues)
			{
				Hierarchy->SetControlValue(Pair.Key, Pair.Value);
			}
		}
	}

	TMap<FRigElementKey, FRigControlValue> ControlValues;
	TWeakObjectPtr<UControlRig> ControlRig;
};

void FAnimNode_ControlRigBase::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FAnimNode_CustomProperty::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);

	if (UControlRig* ControlRig = GetControlRig())
	{
		// fill up node names
		FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();

		const uint16 BonesSerialNumber = RequiredBones.GetSerialNumber();
		const bool bIsLODChange = !bControlRigRequiresInitialization && (BonesSerialNumber != LastBonesSerialNumberForCacheBones);

		// the construction event may create a set of bones that we can map to. let's run construction now.
		if(ControlRig->IsConstructionModeEnabled() ||
			(ControlRig->IsConstructionRequired() && (bControlRigRequiresInitialization || bIsLODChange)))
		{
			ControlRig->Execute(EControlRigState::Update, FRigUnit_PrepareForExecution::EventName);
			bControlRigRequiresInitialization = false;
		}

		ControlRigBoneInputMappingByIndex.Reset();
		ControlRigBoneOutputMappingByIndex.Reset();
		ControlRigCurveMappingByIndex.Reset();
		ControlRigBoneInputMappingByName.Reset();
		ControlRigBoneOutputMappingByName.Reset();
		ControlRigCurveMappingByName.Reset();

		if(RequiredBones.IsValid())
		{
			const TArray<FBoneIndexType>& RequiredBonesArray = RequiredBones.GetBoneIndicesArray();
			const int32 NumBones = RequiredBonesArray.Num();

			const FReferenceSkeleton& RefSkeleton = RequiredBones.GetReferenceSkeleton();

			// @todo: thread-safe? probably not in editor, but it may not be a big issue in editor
			if (NodeMappingContainer.IsValid())
			{
				// get target to source mapping table - this is reversed mapping table
				TMap<FName, FName> TargetToSourceMappingTable;
				NodeMappingContainer->GetTargetToSourceMappingTable(TargetToSourceMappingTable);

				// now fill up node name
				for (uint16 Index = 0; Index < NumBones; ++Index)
				{
					// get bone name, and find reverse mapping
					FName TargetNodeName = RefSkeleton.GetBoneName(RequiredBonesArray[Index]);
					FName* SourceName = TargetToSourceMappingTable.Find(TargetNodeName);
					if (SourceName)
					{
						ControlRigBoneInputMappingByName.Add(*SourceName, Index);
					}
				}
			}
			else
			{
				TArray<FName> NodeNames;
				TArray<FNodeItem> NodeItems;
				ControlRig->GetMappableNodeData(NodeNames, NodeItems);

				// even if not mapped, we map only node that exists in the controlrig
				for (uint16 Index = 0; Index < NumBones; ++Index)
				{
					const FName& BoneName = RefSkeleton.GetBoneName(RequiredBonesArray[Index]);
					if (NodeNames.Contains(BoneName))
					{
						ControlRigBoneInputMappingByName.Add(BoneName, Index);
					}
				}
			}

			if(!InputBonesToTransfer.IsEmpty())
			{
				ControlRigBoneOutputMappingByName = ControlRigBoneInputMappingByName;
				ControlRigBoneInputMappingByName.Reset();
				
				if (NodeMappingContainer.IsValid())
				{
					// get target to source mapping table - this is reversed mapping table
					TMap<FName, FName> TargetToSourceMappingTable;
					NodeMappingContainer->GetTargetToSourceMappingTable(TargetToSourceMappingTable);

					for(FBoneReference& InputBoneToTransfer : InputBonesToTransfer)
					{
						if(!InputBoneToTransfer.Initialize(RequiredBones))
						{
							continue;
						}
						FName TargetNodeName = RefSkeleton.GetBoneName(InputBoneToTransfer.BoneIndex);
						FName* SourceName = TargetToSourceMappingTable.Find(TargetNodeName);
						if (SourceName)
						{
							ControlRigBoneInputMappingByName.Add(*SourceName, InputBoneToTransfer.BoneIndex);
						}
					}
				}
				else
				{
					TArray<FName> NodeNames;
					TArray<FNodeItem> NodeItems;
					ControlRig->GetMappableNodeData(NodeNames, NodeItems);

					for(FBoneReference& InputBoneToTransfer : InputBonesToTransfer)
					{
						if(!InputBoneToTransfer.Initialize(RequiredBones))
						{
							continue;
						}
						const FName& BoneName = RefSkeleton.GetBoneName(RequiredBonesArray[InputBoneToTransfer.BoneIndex]);
						if (NodeNames.Contains(BoneName))
						{
							ControlRigBoneInputMappingByName.Add(BoneName, InputBoneToTransfer.BoneIndex);
						}
					}
				}
			}
			
			// we just support curves by name only
			{
				const FSmartNameMapping* CurveMapping = RequiredBones.GetSkeletonAsset()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
				URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
				CurveMapping->Iterate([Hierarchy, this](const FSmartNameMappingIterator& Iterator)
				{
					// see if the curve name exists in the control rig
					FName CurveName;
					if (Iterator.GetName(CurveName))
					{
						if (Hierarchy->GetIndex(FRigElementKey(CurveName, ERigElementType::Curve)) != INDEX_NONE)
						{
							ControlRigCurveMappingByName.Add(CurveName, Iterator.GetIndex());
						}
					}
				});
				
					
			}

			// check if we can switch the bones to an index based mapping.
			// we can only do that if there is no node mapping container set.
			if(!NodeMappingContainer.IsValid())
			{
				for(int32 InputOutput = 0; InputOutput < 2; InputOutput++)
				{
					bool bIsMappingByIndex = true;
					TMap<FName, uint16>& NameBasedMapping = InputOutput == 0 ? ControlRigBoneInputMappingByName : ControlRigBoneOutputMappingByName;
					if(NameBasedMapping.IsEmpty())
					{
						continue;
					}
					
					TArray<TPair<uint16, uint16>>& IndexBasedMapping = InputOutput == 0 ? ControlRigBoneInputMappingByIndex : ControlRigBoneOutputMappingByIndex;
					
					for (auto Iter = NameBasedMapping.CreateConstIterator(); Iter; ++Iter)
					{
						const uint16 SkeletonIndex = Iter.Value();
						const int32 ControlRigIndex = ControlRig->GetHierarchy()->GetIndex(FRigElementKey(Iter.Key(), ERigElementType::Bone));
						if(ControlRigIndex != INDEX_NONE)
						{
							IndexBasedMapping.Add(TPair<uint16, uint16>((uint16)ControlRigIndex, SkeletonIndex));
						}
						else
						{
							bIsMappingByIndex = false;
						}
					}

					if(bIsMappingByIndex)
					{
						NameBasedMapping.Reset();
					}
					else
					{
						IndexBasedMapping.Reset();
					}
				}
			}

			bool bIsCurveMappingByIndex = true;
			
			// check if we can switch the curves to a index based mapping as well
			for (auto Iter = ControlRigCurveMappingByName.CreateConstIterator(); Iter; ++Iter)
			{
				const uint16 SkeletonIndex = Iter.Value();
				const int32 ControlRigIndex = ControlRig->GetHierarchy()->GetIndex(FRigElementKey(Iter.Key(), ERigElementType::Curve));
				if(ControlRigIndex != INDEX_NONE)
				{
					ControlRigCurveMappingByIndex.Add(TPair<uint16, uint16>((uint16)ControlRigIndex, SkeletonIndex));
				}
				else
				{
					bIsCurveMappingByIndex = false;
				}
			}

			if(bIsCurveMappingByIndex)
			{
				ControlRigCurveMappingByName.Reset();
			}
			else
			{
				ControlRigCurveMappingByIndex.Reset();
			}
		}

		if(bControlRigRequiresInitialization)
		{
			// re-init only if this is the first run
			// and restore control values
			FControlRigControlScope Scope(ControlRig);
			ControlRig->Execute(EControlRigState::Init, FRigUnit_BeginExecution::EventName);
			bControlRigRequiresInitialization = false;
		}
		
		LastBonesSerialNumberForCacheBones = BonesSerialNumber;
	}
}

UClass* FAnimNode_ControlRigBase::GetTargetClass() const
{
	if (UControlRig* ControlRig = GetControlRig())
	{
		return ControlRig->GetClass();
	}

	return nullptr;
}

void FAnimNode_ControlRigBase::QueueControlRigDrawInstructions(UControlRig* ControlRig, FAnimInstanceProxy* Proxy) const
{
	ensure(ControlRig);
	ensure(Proxy);

	if (ControlRig && Proxy)
	{
		for (const FControlRigDrawInstruction& Instruction : ControlRig->GetDrawInterface())
		{
			if (!Instruction.IsValid())
			{
				continue;
			}

			FTransform InstructionTransform = Instruction.Transform * Proxy->GetComponentTransform();
			switch (Instruction.PrimitiveType)
			{
				case EControlRigDrawSettings::Points:
				{
					for (const FVector& Point : Instruction.Positions)
					{
						Proxy->AnimDrawDebugPoint(InstructionTransform.TransformPosition(Point), Instruction.Thickness, Instruction.Color.ToFColor(true), false, -1.f, SDPG_Foreground);
					}
					break;
				}
				case EControlRigDrawSettings::Lines:
				{
					const TArray<FVector>& Points = Instruction.Positions;

					for (int32 PointIndex = 0; PointIndex < Points.Num() - 1; PointIndex += 2)
					{
						Proxy->AnimDrawDebugLine(InstructionTransform.TransformPosition(Points[PointIndex]), InstructionTransform.TransformPosition(Points[PointIndex + 1]), Instruction.Color.ToFColor(true), false, -1.f, Instruction.Thickness, SDPG_Foreground);
					}
					break;
				}
				case EControlRigDrawSettings::LineStrip:
				{
					const TArray<FVector>& Points = Instruction.Positions;

					for (int32 PointIndex = 0; PointIndex < Points.Num() - 1; PointIndex++)
					{
						Proxy->AnimDrawDebugLine(InstructionTransform.TransformPosition(Points[PointIndex]), InstructionTransform.TransformPosition(Points[PointIndex + 1]), Instruction.Color.ToFColor(true), false, -1.f, Instruction.Thickness, SDPG_Foreground);
					}
					break;
				}

				case EControlRigDrawSettings::DynamicMesh:
				{
					// TODO: Add support for this if anyone is actually using it. Currently it is only defined and referenced in an unused API, DrawCone in Control Rig.
					break;
				}
			}
		}
	}
}


