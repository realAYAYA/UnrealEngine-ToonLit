// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesAnimInstanceProxy.h"

#include "AnimNode_ControlRig_CorrectivesSource.h"


FPoseCorrectivesAnimInstanceProxy::FPoseCorrectivesAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_CorrectPose* InNode, FAnimNode_ControlRig_CorrectivesSource* InControlRigNode)
	: FAnimPreviewInstanceProxy(InAnimInstance),
	CorrectPoseNode(InNode),
	ControlRigSourceNode(InControlRigNode)
{
	CopySourceNode.SourceMeshComponent.Reset();
	CopySourceNode.bCopyCurves = true;

	CorrectivesSourceNode.bUseSourcePose = false;
}

void FPoseCorrectivesAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	ControlRigSourceNode->SetControlRigUpdatePose(bUseControlRigInput);

	CorrectPoseNode->SourcePose.SetLinkNode(ControlRigSourceNode);
	ControlRigSourceNode->LinkNode(&CorrectivesSourceNode);
	CorrectivesSourceNode.SourcePose.SetLinkNode(&CopySourceNode);
}

bool FPoseCorrectivesAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	CorrectPoseNode->Evaluate_AnyThread(Output);
	return true;
}

FAnimNode_Base* FPoseCorrectivesAnimInstanceProxy::GetCustomRootNode()
{
	return CorrectPoseNode;
}

void FPoseCorrectivesAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&CorrectivesSourceNode);
	OutNodes.Add(ControlRigSourceNode);
	OutNodes.Add(CorrectPoseNode);
}

void FPoseCorrectivesAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	if (CopySourceNode.SourceMeshComponent.IsValid())
	{
		CopySourceNode.PreUpdate(InAnimInstance);
	}
}

void FPoseCorrectivesAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	FAnimInstanceProxy::UpdateAnimationNode(InContext);
}

void FPoseCorrectivesAnimInstanceProxy::SetCorrectivesAssetAndSourceComponent(
	UPoseCorrectivesAsset* InPoseCorrectivesAsset,
	TWeakObjectPtr<USkeletalMeshComponent> InSourceMeshComponent)
{
	CorrectPoseNode->PoseCorrectivesAsset = InPoseCorrectivesAsset;
	CorrectivesSourceNode.PoseCorrectivesAsset = InPoseCorrectivesAsset;
	
	CopySourceNode.SourceMeshComponent = InSourceMeshComponent.Get();
	CopySourceNode.Initialize_AnyThread(FAnimationInitializeContext(this));
}

void FPoseCorrectivesAnimInstanceProxy::SetUseControlRigInput(bool bControlRigInput)
{
	bUseControlRigInput = bControlRigInput;
	ControlRigSourceNode->SetControlRigUpdatePose(bControlRigInput);
	CorrectPoseNode->EditMode = bControlRigInput;
}

bool FPoseCorrectivesAnimInstanceProxy::IsUsingControlRigInput() const
{
	return bUseControlRigInput;
}

void FPoseCorrectivesAnimInstanceProxy::SetUseCorrectiveSource(const FName& CorrectiveName)
{
	CorrectivesSourceNode.bUseCorrectiveSource = true;
	CorrectivesSourceNode.CurrentCorrrective = CorrectiveName;
}

void FPoseCorrectivesAnimInstanceProxy::StopUseCorrectiveSource()
{
	CorrectivesSourceNode.bUseCorrectiveSource = false;
}


FPoseCorrectivesAnimSourceInstanceProxy::FPoseCorrectivesAnimSourceInstanceProxy(UAnimInstance* InAnimInstance)
	: FAnimPreviewInstanceProxy(InAnimInstance)
{
}

void FPoseCorrectivesAnimSourceInstanceProxy::SetCorrectivesAsset(UPoseCorrectivesAsset* InPoseCorrectivesAsset)
{
	CorrectivesSourceNode.PoseCorrectivesAsset = InPoseCorrectivesAsset;
}

void FPoseCorrectivesAnimSourceInstanceProxy::SetUseCorrectiveSource(const FName& CorrectiveName)
{
	CorrectivesSourceNode.bUseCorrectiveSource = true;
	CorrectivesSourceNode.CurrentCorrrective = CorrectiveName;
}

void FPoseCorrectivesAnimSourceInstanceProxy::StopUseCorrectiveSource()
{
	CorrectivesSourceNode.bUseCorrectiveSource = false;
}

bool FPoseCorrectivesAnimSourceInstanceProxy::Evaluate(FPoseContext& Output)
{
	if (!CorrectivesSourceNode.bUseCorrectiveSource)
	{
		// Use animation asset if not use corrective source
		FAnimPreviewInstanceProxy::Evaluate(Output);
	}
	else
	{
		CorrectivesSourceNode.Evaluate_AnyThread(Output);		
	}

	return true;
}


FAnimNode_Base* FPoseCorrectivesAnimSourceInstanceProxy::GetCustomRootNode()
{
	return &CorrectivesSourceNode;
}

void FPoseCorrectivesAnimSourceInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(&CorrectivesSourceNode);
}
