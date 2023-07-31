// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubTransformProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SubTransformProxy)

USubTransformProxy::USubTransformProxy()
{
	RelativeTransform = FTransform::Identity;
}

FTransform USubTransformProxy::AddSubTransformProxy(USubTransformProxy* InProxy, bool bSubscribeToChanges)
{
	check(InProxy);

	FTransform OriginalTransform = InProxy->GetTransform();

	// Add each object to the proxy
	for (FRelativeObject& Object : InProxy->Objects)
	{
		FRelativeObject& NewObj = Objects.Emplace_GetRef();
		NewObj.Component = Object.Component;
		NewObj.bModifyComponentOnTransform = Object.bModifyComponentOnTransform;
		NewObj.StartTransform = Object.Component->GetComponentToWorld();
		NewObj.RelativeTransform = FTransform::Identity;
	}

	UpdateSharedTransform();

	// Get the relative transform of the InProxy we just added
	OriginalTransform.SetToRelativeTransform(SharedTransform);

	if (bSubscribeToChanges)
	{
		OnTransformChanged.AddUObject(InProxy, &USubTransformProxy::OnParentTransformChanged);
		OnRelativeTransformChanged.AddUObject(InProxy, &USubTransformProxy::OnParentRelativeTransformChanged);
	}

	OnRelativeTransformChanged.Broadcast(this, SharedTransform);
	OnTransformChanged.Broadcast(this, SharedTransform);

	return OriginalTransform;
}

void USubTransformProxy::OnParentTransformChanged(UTransformProxy* Parent, FTransform ParentTransform)
{
	FTransform OriginalTransform = SharedTransform;

	FTransform::Multiply(&SharedTransform, &RelativeTransform, &ParentTransform);

	 // Parent Transform changing doesn't have to mean that your transform changed
	if (!SharedTransform.Equals(OriginalTransform))
	{
		UpdateObjectTransforms();

		OnTransformChanged.Broadcast(this, SharedTransform);
	}

}

void USubTransformProxy::OnParentRelativeTransformChanged(UTransformProxy* Parent, FTransform ParentTransform)
{
	RelativeTransform = SharedTransform.GetRelativeTransform(ParentTransform);
}


