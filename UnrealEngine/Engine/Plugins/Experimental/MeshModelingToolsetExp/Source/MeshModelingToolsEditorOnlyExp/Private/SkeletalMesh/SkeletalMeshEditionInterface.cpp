// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMesh/SkeletalMeshEditionInterface.h"

IMPLEMENT_HIT_PROXY(HBoneHitProxy, HHitProxy)

ISkeletalMeshNotifier& ISkeletalMeshEditingInterface::GetNotifier()
{
	if (!Notifier)
	{
		Notifier.Reset(new FSkeletalMeshToolNotifier(TWeakInterfacePtr<ISkeletalMeshEditingInterface>(this)));
	}
	
	return *Notifier;
}

bool ISkeletalMeshEditingInterface::NeedsNotification() const
{
	return Notifier && Notifier->Delegate().IsBound(); 
}

void ISkeletalMeshEditingInterface::BindTo(TSharedPtr<ISkeletalMeshEditorBinding> InBinding)
{
	Binding = InBinding;
}

void ISkeletalMeshEditingInterface::Unbind()
{
	Binding.Reset();
}

TWeakObjectPtr<USkeletonModifier> ISkeletalMeshEditingInterface::GetModifier() const
{
	return nullptr;
}

TOptional<FName> ISkeletalMeshEditingInterface::GetBoneName(HHitProxy* InHitProxy) const
{
	if (const HBoneHitProxy* BoneProxy = HitProxyCast<HBoneHitProxy>(InHitProxy))
	{
		return BoneProxy->BoneName;
	}

	return Binding.IsValid() && Binding.Pin()->GetNameFunction() ? Binding.Pin()->GetNameFunction()(InHitProxy) : TOptional<FName>();
}

TArray<FName> ISkeletalMeshEditingInterface::GetSelectedBones() const
{
	static const TArray<FName> Dummy;
	return Binding.IsValid() ? Binding.Pin()->GetSelectedBones() : Dummy;
}

FSkeletalMeshToolNotifier::FSkeletalMeshToolNotifier(TWeakInterfacePtr<ISkeletalMeshEditingInterface> InInterface)
	: ISkeletalMeshNotifier()
	, Interface(InInterface)
{}

void FSkeletalMeshToolNotifier::HandleNotification(const TArray<FName>& BoneNames, const ESkeletalMeshNotifyType InNotifyType)
{
	if (Interface.IsValid())
	{
		Interface->HandleSkeletalMeshModified(BoneNames, InNotifyType);
	}
}
