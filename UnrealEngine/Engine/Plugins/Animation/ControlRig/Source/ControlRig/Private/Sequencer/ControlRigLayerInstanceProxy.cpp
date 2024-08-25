// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/ControlRigLayerInstanceProxy.h"
#include "AnimNode_ControlRig_ExternalSource.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "AnimSequencerInstance.h"
#include "ControlRig.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"

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

void FControlRigLayerInstanceProxy::PreEvaluateAnimation(UAnimInstance* InAnimInstance)
{
	Super::PreEvaluateAnimation(InAnimInstance);

	if (CurrentSourceAnimInstance)
	{
		CurrentSourceAnimInstance->PreEvaluateAnimation();
	}
}

void FControlRigLayerInstanceProxy::SortControlRigNodes()
{
	auto SortPredicate = [](TSharedPtr<FAnimNode_ControlRig_ExternalSource>& A, TSharedPtr<FAnimNode_ControlRig_ExternalSource>& B)
	{
		FAnimNode_ControlRig_ExternalSource* APtr = A.Get();
		FAnimNode_ControlRig_ExternalSource* BPtr = B.Get();
		if (APtr && BPtr && APtr->GetControlRig() && B->GetControlRig())
		{
			const bool AIsAdditive = A->GetControlRig()->IsAdditive();
			const bool BIsAdditive = B->GetControlRig()->IsAdditive();
			if (AIsAdditive != BIsAdditive)
			{
				return AIsAdditive; // if additive then first(first goes last) so true if AIsAdditive;
			}
			UMovieSceneControlRigParameterTrack* TrackA = APtr->GetControlRig()->GetTypedOuter<UMovieSceneControlRigParameterTrack>();
			UMovieSceneControlRigParameterTrack* TrackB = BPtr->GetControlRig()->GetTypedOuter<UMovieSceneControlRigParameterTrack>();
			if (TrackA && TrackB)
			{
				return TrackA->GetPriorityOrder() < TrackB->GetPriorityOrder();
			}
		}
		else if (APtr && APtr->GetControlRig())
		{
			return false;
		}
		return true;
	};

	Algo::Sort(ControlRigNodes, SortPredicate);

	ConstructNodes(); //we need to sort since prority may change at any time 
}

bool FControlRigLayerInstanceProxy::Evaluate(FPoseContext& Output)
{
	SortControlRigNodes();  //mz todo once we move over to ECS see if we can avoid this and trigger it as needed.
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
	const int32 DefaultPriorityOrder = 100;
	if(!Node)
	{
		UMovieSceneControlRigParameterTrack* Track = InControlRig->GetTypedOuter<UMovieSceneControlRigParameterTrack>();

		if (ControlRigNodes.Num() > 0)
		{
			int32 PriorityOrder = Track ? Track->GetPriorityOrder() : INDEX_NONE;
			if (PriorityOrder == INDEX_NONE) //track has no order so just add to end, will happen on creation
			{
				if (InControlRig->IsAdditive()) //additive added to end of all
				{
					Node = ControlRigNodes[ControlRigNodes.Num() - 1].Get();
					if (Track)
					{
						if (UMovieSceneControlRigParameterTrack* OtherTrack = Node->GetControlRig()->GetTypedOuter<UMovieSceneControlRigParameterTrack>())
						{
							Track->SetPriorityOrder(OtherTrack->GetPriorityOrder() + 1);
						}
						else
						{
							Track->SetPriorityOrder(DefaultPriorityOrder + ControlRigNodes.Num() -1);
						}
					}
					Node = ControlRigNodes.Add_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>()).Get();
				}
				else //if non-additive find first additive then insert there..
				{
					for (int32 Index = 0; Index < ControlRigNodes.Num(); ++Index)
					{
						Node = ControlRigNodes[Index].Get();
						if (!Node || !Node->GetControlRig() || !Node->GetControlRig()->IsAdditive())
						{
							Node = nullptr;
							continue; // not additive
						}
						//okay first additive so insert, but set priority first
						if (Track)
						{
							if (UMovieSceneControlRigParameterTrack* OtherTrack = Node->GetControlRig()->GetTypedOuter<UMovieSceneControlRigParameterTrack>())
							{
								Track->SetPriorityOrder(OtherTrack->GetPriorityOrder() + 1);
							}
							else
							{
								Track->SetPriorityOrder(DefaultPriorityOrder + Index);
							}
						}
						Node = ControlRigNodes.Insert_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>(), Index).Get();
						break;
					}

					if (Node == nullptr) //add to end
					{
						//set priority
						Node = ControlRigNodes[ControlRigNodes.Num() - 1].Get();
						if (Track)
						{
							if (UMovieSceneControlRigParameterTrack* OtherTrack = Node->GetControlRig()->GetTypedOuter<UMovieSceneControlRigParameterTrack>())
							{
								Track->SetPriorityOrder(OtherTrack->GetPriorityOrder() + 1);
							}
							else
							{
								Track->SetPriorityOrder(ControlRigNodes.Num() - 1);
							}
						}
						Node = ControlRigNodes.Add_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>()).Get();
					}
				}
			}
			else
			{
				int32 NewNodePosition = 0;
				for (; NewNodePosition < ControlRigNodes.Num(); ++NewNodePosition)
				{
					Node = ControlRigNodes[NewNodePosition].Get();
	
					if (!Node || !Node->GetControlRig())
					{
						continue; // shouldn't happen
					}
					//additive but current isn't so keep going
					if (InControlRig->IsAdditive() && !Node->GetControlRig()->IsAdditive())
					{
						continue;
					}
					//not additive but this one is so we add it here
					else if (!InControlRig->IsAdditive() && Node->GetControlRig()->IsAdditive())
					{
						break;
					}
					if (UMovieSceneControlRigParameterTrack* OtherTrack = Node->GetControlRig()->GetTypedOuter<UMovieSceneControlRigParameterTrack>())
					{
						if (PriorityOrder >= OtherTrack->GetPriorityOrder())
						{
							continue;
						}
						else
						{
							break;
						}
					}
				}
				if (NewNodePosition >= ControlRigNodes.Num() - 1)
				{
					Node = ControlRigNodes.Add_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>()).Get();
				}
				else
				{
					Node = ControlRigNodes.Insert_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>(), NewNodePosition).Get();
				}
			}
		}
		else //no nodes add first one
		{
			Node = ControlRigNodes.Add_GetRef(MakeShared<FAnimNode_ControlRig_ExternalSource>()).Get();
			if (Track)
			{
				int32 PriorityOrder = Track->GetPriorityOrder();
				if (PriorityOrder == INDEX_NONE) //track has no order so just add to end, will happen on creation
				{
					Track->SetPriorityOrder(DefaultPriorityOrder);
				}
			}
		}
	
		check(Node);
		
		//this will set up the link nodes
		ConstructNodes();
		SequencerToControlRigNodeMap.FindOrAdd(ControlRigID) = Node;
	}

	Node->SetControlRig(InControlRig);
	Node->OnInitializeAnimInstance(this, CastChecked<UAnimInstance>(GetAnimInstanceObject()));
	//mz removed this due to crash since Skeleton is not set up on a previous linked node 
	// see FORT-630426
	//but leaving in case it's needed for something else in which case need
	//to call AnimInstance::UpdateAnimation(via TickAnimation perhaps
	// Also, this initialize will remove any source animations that additive control rigs might depend on
	//Node->Initialize_AnyThread(FAnimationInitializeContext(this));
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

		// "ControlRigNodes" should have nodes sorted from parent(last to evaluate) to child(first to evaluate)
		for (int32 Index = 0; Index < ControlRigNodes.Num(); ++Index)
		{
			FAnimNode_ControlRig_ExternalSource* Current = ControlRigNodes[Index].Get();

			if (Current == Node)
			{
				// we need to delete this one
				// find next child one
				FAnimNode_ControlRig_ExternalSource* Child = (ControlRigNodes.IsValidIndex(Index + 1)) ? ControlRigNodes[Index + 1].Get() : nullptr;

				if (Parent)
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

		if (ControlRigNodes.IsEmpty())
		{
			CurrentRoot = &InputPose;
		}
		else
		{
			// stay consistent with ConstructNodes()
			CurrentRoot = ControlRigNodes[0].Get();
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

	for (TSharedPtr<FAnimNode_ControlRig_ExternalSource>& Node : ControlRigNodes)
	{
#if WITH_EDITORONLY_DATA
		Collector.AddReferencedObject(Node->SourceInstance);
#endif
		Collector.AddReferencedObject(Node->TargetInstance);
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
		if (FAnimNode_Base* InputNode = InputPose.GetLinkNode())
		{
			InputProxy->UpdateAnimation_WithRoot(InputContext, InputNode, TEXT("AnimGraph"));
		}
		else if(InputProxy->HasRootNode())
		{
			InputProxy->UpdateAnimationNode(InputContext);
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
			FPoseContext InnerOutput(InputProxy, Output.ExpectsAdditivePose());
			
			// if no linked node, just use Evaluate of proxy
			if (FAnimNode_Base* InputNode = InputPose.GetLinkNode())
			{
				InputProxy->EvaluateAnimation_WithRoot(InnerOutput, InputNode);
			}
			else if(InputProxy->HasRootNode())
			{
				InputProxy->EvaluateAnimationNode(InnerOutput);
			}
			else
			{
				FControlRigLayerInstanceProxy::EvaluateCustomProxy(InputProxy, InnerOutput);
			}

			Output.Pose.MoveBonesFrom(InnerOutput.Pose);
			Output.Curve.MoveFrom(InnerOutput.Curve);
			Output.CustomAttributes.MoveFrom(InnerOutput.CustomAttributes);
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


