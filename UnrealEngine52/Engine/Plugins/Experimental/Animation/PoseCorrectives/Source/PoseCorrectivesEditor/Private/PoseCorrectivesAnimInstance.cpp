// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesAnimInstance.h"

#include "PoseCorrectivesAnimInstanceProxy.h"


UPoseCorrectivesAnimInstance::UPoseCorrectivesAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = false;
}

void UPoseCorrectivesAnimInstance::SetCorrectivesAssetAndSourceComponent(
	UPoseCorrectivesAsset* InAsset,
	TWeakObjectPtr<USkeletalMeshComponent> InSourceMeshComponent)
{
	GetProxyOnGameThread<FPoseCorrectivesAnimInstanceProxy>().SetCorrectivesAssetAndSourceComponent(InAsset, InSourceMeshComponent);
}

void UPoseCorrectivesAnimInstance::SetSourceControlRig(UControlRig* InControlRig)
{
	ControlRigSourceNode.SetControlRig(InControlRig);
}

void UPoseCorrectivesAnimInstance::SetUseControlRigInput(bool bUseControlRigInput)
{
	GetProxyOnGameThread<FPoseCorrectivesAnimInstanceProxy>().SetUseControlRigInput(bUseControlRigInput);
}

bool UPoseCorrectivesAnimInstance::IsUsingControlRigInput() const
{
	const FPoseCorrectivesAnimInstanceProxy& Proxy = GetProxyOnGameThread<FPoseCorrectivesAnimInstanceProxy>();
	return Proxy.IsUsingControlRigInput();
}

void UPoseCorrectivesAnimInstance::SetUseCorrectiveSource(const FName& CorrectiveName)
{
	GetProxyOnGameThread<FPoseCorrectivesAnimInstanceProxy>().SetUseCorrectiveSource(CorrectiveName);
}

void UPoseCorrectivesAnimInstance::StopUseCorrectiveSource()
{
	GetProxyOnGameThread<FPoseCorrectivesAnimInstanceProxy>().StopUseCorrectiveSource();
}


FAnimInstanceProxy* UPoseCorrectivesAnimInstance::CreateAnimInstanceProxy()
{
	return new FPoseCorrectivesAnimInstanceProxy(this, &PoseCorrectivesNode, &ControlRigSourceNode);
}

UPoseCorrectivesAnimSourceInstance::UPoseCorrectivesAnimSourceInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = false;
}

void UPoseCorrectivesAnimSourceInstance::SetCorrectivesAsset(UPoseCorrectivesAsset* InAsset)
{
	GetProxyOnGameThread<FPoseCorrectivesAnimSourceInstanceProxy>().SetCorrectivesAsset(InAsset);
}

void UPoseCorrectivesAnimSourceInstance::SetUseCorrectiveSource(const FName& CorrectiveName)
{
	GetProxyOnGameThread<FPoseCorrectivesAnimSourceInstanceProxy>().SetUseCorrectiveSource(CorrectiveName);
}

void UPoseCorrectivesAnimSourceInstance::StopUseCorrectiveSource()
{
	GetProxyOnGameThread<FPoseCorrectivesAnimSourceInstanceProxy>().StopUseCorrectiveSource();
}

FAnimInstanceProxy* UPoseCorrectivesAnimSourceInstance::CreateAnimInstanceProxy()
{
	return new FPoseCorrectivesAnimSourceInstanceProxy(this);
}

