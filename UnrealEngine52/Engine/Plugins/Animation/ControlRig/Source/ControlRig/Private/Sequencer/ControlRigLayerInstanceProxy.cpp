// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigLayerInstanceProxy.h"
#include "AnimNode_ControlRig_ExternalSource.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "AnimSequencerInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigLayerInstanceProxy)

void FControlRigLayerInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	ConstructNodes();

	FAnimInstanceProxy::Initialize(InAnimInstance);

	UpdateCounter.Reset();
}

void FControlRigLayerInstanceProxy::CacheBones()
{
	if (bBoneCachesInvalidated)
	{
		FAnimationCacheBonesContext Context(this);
		check(CurrentRoot);
		CurrentRoot->CacheBones_AnyThread(Context);

		bBoneCachesInvalidated = false;
	}
}

bool FControlRigLayerInstanceProxy::Evaluate(FPoseContext& Output)
{
	check(CurrentRoot);
	CurrentRoot->Evaluate_AnyThread(Output);
	return true;
}

void FControlRigLayerInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	UpdateCounter.Increment();

	check(CurrentRoot);
	CurrentRoot->Update_AnyThread(InContext);
}

void FControlRigLayerInstanceProxy::ConstructNodes()
{
	if (ControlRigNodes.Num() > 0)
	{
		FAnimNode_ControlRig_ExternalSource* ParentNode = ControlRigNodes[0].Get();
		CurrentRoot = static_cast<FAnimNode_Base*>(ParentNode);

		// index 0 - (N-1) is the order of operation
		for (int32 Index = 1; Index < ControlRigNodes.Num(); ++Index)
		{
			FAnimNode_ControlRig_ExternalSource* CurrentNode = ControlRigNodes[Index].Get();
			ParentNode->Source.SetLinkNode(CurrentNode);
			ParentNode = CurrentNode;
		}

		// last parent node has to link to input pose
		ParentNode->Source.SetLinkNode(&InputPose);
	}
	else
	{
		CurrentRoot = &InputPose;
	}

// 	if (UAnimSequencerInstance* SeqInstance = GetSequencerAnimInstance())
// 	{
// 		SeqInstance->ConstructNodes();
// 	}
}

void FControlRigLayerInstanceProxy::ResetPose()
{
	if (UAnimSequencerInstance* SeqInstance = GetSequencerAnimInstance())
	{
		SeqInstance->ResetPose();
	}
}	

void FControlRigLayerInstanceProxy::ResetNodes()
{
	if (UAnimSequencerInstance* SeqInstance = GetSequencerAnimInstance())
	{
		SeqInstance->ResetNodes();
	}
}

FControlRigLayerInstanceProxy::~FControlRigLayerInstanceProxy()
{
}

void FControlRigLayerInstanceProxy::UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InPosition, float Weight, bool bFireNotifies)
{
	if (UAnimSequencerInstance* SeqInstance = GetSequencerAnimInstance())
	{
		SeqInstance->UpdateAnimTrack(InAnimSequence, SequenceId, InPosition, Weight, bFireNotifies);
	}
}

void FControlRigLayerInstanceProxy::UpdateAnimTrack(UAnimSequenceBase* InAnimSequence, int32 SequenceId, float InFromPosition, float InToPosition, float Weight, bool bFireNotifies)
{
	if (UAnimSequencerInstance* SeqInstance = GetSequencerAnimInstance())
	{
		SeqInstance->UpdateAnimTrack(InAnimSequence, SequenceId, InFromPosition, InToPosition, Weight, bFireNotifies);
	}
}

FAnimNode_ControlRig_ExternalSource* FControlRigLayerInstanceProxy::FindControlRigNode(int32 ControlRigID) const
{
	return SequencerToControlRigNodeMap.FindRef(ControlRigID);
}


/** Anim Instance Source info - created externally and used here */
void FControlRigLayerInstanceProxy::SetSourceAnimInstance(UAnimInstance* SourceAnimInstance, FAnimInstanceProxy* SourceAnimInputProxy)
{
	CurrentSourceAnimInstance = SourceAnimInstance;
	InputPose.Unlink();

	if (CurrentSourceAnimInstance)
	{
		InputPose.Link(CurrentSourceAnimInstance, SourceAnimInputProxy);
	}
}

/** ControlRig related support */
void FControlRigLayerInstanceProxy::AddControlRigTrack(int32 ControlRigID, UControlRig* InControlRig)
{
	FAnimNode_ControlRig_ExternalSource* Node = FindControlRigNode(ControlRigID);

	if(!Node)
	{
		FAnimNode_ControlRig_ExternalSource* Parent = (ControlRigNodes.Num() > 0)? ControlRigNodes.Last().Get() : nullptr;
		Node = ControlRigNodes.Add_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>()).Get();
		SequencerToControlRigNodeMap.FindOrAdd(ControlRigID) = Node;
		if (Parent)
		{
			FAnimNode_Base* LinkedNode = Parent->Source.GetLinkNode();
			Parent->Source.SetLinkNode(Node);
			Node->Source.SetLinkNode(LinkedNode);
		}
		else
		{
			// first node
			CurrentRoot = Node;
			Node->Source.SetLinkNode(&InputPose);
		}
	}

	Node->SetControlRig(InControlRig);
	Node->OnInitializeAnimInstance(this, CastChecked<UAnimInstance>(GetAnimInstanceObject()));
	Node->Initialize_AnyThread(FAnimationInitializeContext(this));
}

bool FControlRigLayerInstanceProxy::HasControlRigTrack(int32 ControlRigID)
{
	FAnimNode_ControlRig_ExternalSource* Node = FindControlRigNode(ControlRigID);
	return (Node != nullptr) ? true : false;
}

void FControlRigLayerInstanceProxy::ResetControlRigTracks()
{
	SequencerToControlRigNodeMap.Reset();
	ControlRigNodes.Reset();

}
void FControlRigLayerInstanceProxy::UpdateControlRigTrack(int32 ControlRigID, float Weight, const FControlRigIOSettings& InputSettings, bool bExecute)
{
	if (FAnimNode_ControlRig_ExternalSource* Node = FindControlRigNode(ControlRigID))
	{
		Node->InternalBlendAlpha = FMath::Clamp(Weight, 0.f, 1.f);
		Node->InputSettings = InputSettings;
		Node->bExecute = bExecute;
	}
}

void FControlRigLayerInstanceProxy::RemoveControlRigTrack(int32 ControlRigID)
{
	if (FAnimNode_ControlRig_ExternalSource* Node = FindControlRigNode(ControlRigID))
	{
		FAnimNode_ControlRig_ExternalSource* Parent = nullptr;
		for (int32 Index = 0; Index < ControlRigNodes.Num(); ++Index)
		{
			FAnimNode_ControlRig_ExternalSource* Current = ControlRigNodes[Index].Get();

			if (Current == Node)
			{
				// we need to delete this one
				// find next child one
				FAnimNode_ControlRig_ExternalSource* Child = (ControlRigNodes.IsValidIndex(Index + 1)) ? ControlRigNodes[Index + 1].Get() : nullptr;

				// if no parent, change root
				if (Parent == nullptr)
				{
					// first one to delete
					if (Child)
					{
						CurrentRoot = Child;
					}
					else
					{
						CurrentRoot = &InputPose;
					}
				}
				else
				{
					if (Child)
					{
						Parent->Source.SetLinkNode(Child);
					}
					else
					{
						Parent->Source.SetLinkNode(&InputPose);
					}
				}

				ControlRigNodes.RemoveAt(Index);
				break;
			}

			Parent = Current;
		}

		SequencerToControlRigNodeMap.Remove(ControlRigID);
	}
}

/** Sequencer AnimInstance Interface - don't need these right now */
void FControlRigLayerInstanceProxy::AddAnimation(int32 SequenceId, UAnimSequenceBase* InAnimSequence)
{

}

void FControlRigLayerInstanceProxy::RemoveAnimation(int32 SequenceId)
{

}

UAnimSequencerInstance* FControlRigLayerInstanceProxy::GetSequencerAnimInstance()
{
	return Cast<UAnimSequencerInstance>(CurrentSourceAnimInstance);
}

UControlRig* FControlRigLayerInstanceProxy::GetFirstAvailableControlRig() const
{
	for (const TSharedPtr<FAnimNode_ControlRig_ExternalSource>& ControlRigNode : ControlRigNodes)
	{
		if (UControlRig* Rig = ControlRigNode->GetControlRig())
		{
			return Rig;
		}
	}

	return nullptr;
}

void FControlRigLayerInstanceProxy::AddReferencedObjects(UAnimInstance* InAnimInstance, FReferenceCollector& Collector)
{
	if (CurrentSourceAnimInstance)
	{
		Collector.AddReferencedObject(CurrentSourceAnimInstance);
	}
}

void FControlRigLayerInstanceProxy::InitializeCustomProxy(FAnimInstanceProxy* InputProxy, UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::InitializeInputProxy(InputProxy, InAnimInstance);
}

void FControlRigLayerInstanceProxy::GatherCustomProxyDebugData(FAnimInstanceProxy* InputProxy, FNodeDebugData& DebugData)
{
	FAnimInstanceProxy::GatherInputProxyDebugData(InputProxy, DebugData);
}

void FControlRigLayerInstanceProxy::CacheBonesCustomProxy(FAnimInstanceProxy* InputProxy)
{
	FAnimInstanceProxy::CacheBonesInputProxy(InputProxy);
}

void FControlRigLayerInstanceProxy::UpdateCustomProxy(FAnimInstanceProxy* InputProxy, const FAnimationUpdateContext& Context)
{
	FAnimInstanceProxy::UpdateInputProxy(InputProxy, Context);
}

void FControlRigLayerInstanceProxy::EvaluateCustomProxy(FAnimInstanceProxy* InputProxy, FPoseContext& Output)
{
	FAnimInstanceProxy::EvaluateInputProxy(InputProxy, Output);
}

void FControlRigLayerInstanceProxy::ResetCounter(FAnimInstanceProxy* InAnimInstanceProxy)
{
	FAnimInstanceProxy::ResetCounterInputProxy(InAnimInstanceProxy);
}
//////////////////////////////////////////////////////////////////////////////////////////////////
void FAnimNode_ControlRigInputPose::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	if (InputProxy)
	{
		FAnimationInitializeContext InputContext(InputProxy);
		if (InputPose.GetLinkNode())
		{
			InputPose.Initialize(InputContext);
		}
 		else 
 		{
			FControlRigLayerInstanceProxy::InitializeCustomProxy(InputProxy, InputAnimInstance);
 		}
	}
}

void FAnimNode_ControlRigInputPose::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	if (InputProxy)
	{
		FAnimationCacheBonesContext InputContext(InputProxy);
		if (InputPose.GetLinkNode())
		{
			InputPose.CacheBones(InputContext);
		}
		else
		{
			FControlRigLayerInstanceProxy::CacheBonesCustomProxy(InputProxy);
		}
	}
}

void FAnimNode_ControlRigInputPose::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	if (InputProxy)
	{
		FAnimationUpdateContext InputContext = Context.WithOtherProxy(InputProxy);
		if (InputPose.GetLinkNode())
		{
			InputPose.Update(InputContext);
		}
		else
		{
			FControlRigLayerInstanceProxy::UpdateCustomProxy(InputProxy, InputContext);
		}
	}
}

void FAnimNode_ControlRigInputPose::Evaluate_AnyThread(FPoseContext& Output)
{
	if (InputProxy)
	{
		FBoneContainer& RequiredBones = InputProxy->GetRequiredBones();
		if (RequiredBones.IsValid())
		{
			Output.Pose.SetBoneContainer(&RequiredBones);
			FPoseContext InputContext(InputProxy, Output.ExpectsAdditivePose());

			// if no linked node, just use Evaluate of proxy
			if (InputPose.GetLinkNode())
			{
				InputPose.Evaluate(InputContext);
			}
			else
			{
				FControlRigLayerInstanceProxy::EvaluateCustomProxy(InputProxy, InputContext);
			}

			Output.Pose.MoveBonesFrom(InputContext.Pose);
			Output.Curve.MoveFrom(InputContext.Curve);
			Output.CustomAttributes.MoveFrom(InputContext.CustomAttributes);
			return;
		}
	}

	Output.ResetToRefPose();
}

void FAnimNode_ControlRigInputPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);

	if (InputProxy)
	{
		if (InputPose.GetLinkNode())
		{
			InputPose.GatherDebugData(DebugData);
		}
		else
		{
			FControlRigLayerInstanceProxy::GatherCustomProxyDebugData(InputProxy, DebugData);
		}
	}
}

void FAnimNode_ControlRigInputPose::Link(UAnimInstance* InInputInstance, FAnimInstanceProxy* InInputProxy)
{
	Unlink();

	if (InInputInstance)
	{
		InputAnimInstance = InInputInstance;
		InputProxy = InInputProxy;
		InputPose.SetLinkNode(InputProxy->GetRootNode());
		
		// reset counter, so that input proxy can restart
		FControlRigLayerInstanceProxy::ResetCounter(InputProxy);
	}
}

void FAnimNode_ControlRigInputPose::Unlink()
{
	InputProxy = nullptr;
	InputAnimInstance = nullptr;
	InputPose.SetLinkNode(nullptr);
}


