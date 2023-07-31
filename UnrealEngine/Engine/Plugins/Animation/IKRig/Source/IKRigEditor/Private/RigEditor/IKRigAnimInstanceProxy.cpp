// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigAnimInstanceProxy.h"
#include "RigEditor/IKRigAnimInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigAnimInstanceProxy)


FIKRigAnimInstanceProxy::FIKRigAnimInstanceProxy(UAnimInstance* InAnimInstance, FAnimNode_IKRig* InIKRigNode)
	: FAnimPreviewInstanceProxy(InAnimInstance),
	IKRigNode(InIKRigNode)
{
}

void FIKRigAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimPreviewInstanceProxy::Initialize(InAnimInstance);
	IKRigNode->Source.SetLinkNode(&SingleNode);

	// force this instance of the IK Rig evaluation to copy setting from the source IK Rig asset
	IKRigNode->bDriveWithSourceAsset = true; 
}

bool FIKRigAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	IKRigNode->Evaluate_AnyThread(Output);
	return true;
}

FAnimNode_Base* FIKRigAnimInstanceProxy::GetCustomRootNode()
{
	return IKRigNode;
}

void FIKRigAnimInstanceProxy::GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes)
{
	OutNodes.Add(IKRigNode);
}

void FIKRigAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& InContext)
{
	if (CurrentAsset != nullptr)
	{
		FAnimPreviewInstanceProxy::UpdateAnimationNode(InContext);
	}
	else
	{
		IKRigNode->Update_AnyThread(InContext);
	}
}

void FIKRigAnimInstanceProxy::SetIKRigAsset(UIKRigDefinition* InIKRigAsset)
{
	IKRigNode->RigDefinitionAsset = InIKRigAsset;
}


