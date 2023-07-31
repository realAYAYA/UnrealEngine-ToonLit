// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaToolMenuContext.h"
#include "IPersonaToolkit.h"

USkeleton* UPersonaToolMenuContext::GetSkeleton() const
{
	if (HasValidToolkit())
	{
		return WeakToolkit.Pin()->GetSkeleton();
	}

	return nullptr;
}

UDebugSkelMeshComponent* UPersonaToolMenuContext::GetPreviewMeshComponent() const
{
	if (HasValidToolkit())
	{
		return WeakToolkit.Pin()->GetPreviewMeshComponent();
	}

	return nullptr;
}

USkeletalMesh* UPersonaToolMenuContext::GetMesh() const
{
	if (HasValidToolkit())
	{
		return WeakToolkit.Pin()->GetMesh();
	}

	return nullptr;
}

UAnimBlueprint* UPersonaToolMenuContext::GetAnimBlueprint() const
{
	if (HasValidToolkit())
	{
		return WeakToolkit.Pin()->GetAnimBlueprint();
	}

	return nullptr;
}

UAnimationAsset* UPersonaToolMenuContext::GetAnimationAsset() const
{
	if (HasValidToolkit())
	{
		return WeakToolkit.Pin()->GetAnimationAsset();
	}

	return nullptr;
}

void UPersonaToolMenuContext::SetToolkit(TSharedRef<IPersonaToolkit> InToolkit)
{
	WeakToolkit = InToolkit;
}

bool UPersonaToolMenuContext::HasValidToolkit() const
{
	return WeakToolkit.IsValid();
}

