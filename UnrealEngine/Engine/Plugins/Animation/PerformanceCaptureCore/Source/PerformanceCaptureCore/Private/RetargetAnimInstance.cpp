// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetAnimInstance.h"
#include "RetargetInstanceProxy.h"

///////////////////////////////////
/// Anim Instance 
///////////////////////////////////

void URetargetAnimInstance::UpdateCustomRetargetProfile(const FRetargetProfile &InRetargetProfile)
{
	FRetargetAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRetargetAnimInstanceProxy>();
	Proxy.RetargetNode->CustomRetargetProfile = InRetargetProfile;
}

FRetargetProfile URetargetAnimInstance::GetRetargetProfile()
{
	FRetargetAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRetargetAnimInstanceProxy>();
	FRetargetProfile OutProfile = Proxy.RetargetNode->CustomRetargetProfile;
	return OutProfile;
}

void URetargetAnimInstance::NativeInitializeAnimation()
{
	FRetargetAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRetargetAnimInstanceProxy>();
	Proxy.Initialize(this);
}

void URetargetAnimInstance::ConfigureAnimInstance(UIKRetargeter* InIKRetargetAsset, TWeakObjectPtr<USkeletalMeshComponent> InSourceMeshComponent, FRetargetProfile InRetargetProfile)
{
	FRetargetAnimInstanceProxy& Proxy = GetProxyOnGameThread<FRetargetAnimInstanceProxy>();
	Proxy.ConfigureAnimInstanceProxy(InIKRetargetAsset, InSourceMeshComponent, InRetargetProfile);
}

FAnimInstanceProxy* URetargetAnimInstance::CreateAnimInstanceProxy()
{
	return new FRetargetAnimInstanceProxy(this, &RetargetNode);
}
