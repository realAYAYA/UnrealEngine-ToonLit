// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationModifiersAssetUserData.h"
#include "AnimationModifier.h"
#include "UObject/UObjectThreadContext.h"

#include "HAL/PlatformCrt.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "UObject/ObjectPtr.h"
#include "Animation/AnimSequence.h"

void UAnimationModifiersAssetUserData::AddAnimationModifier(UAnimationModifier* Instance)
{
	AnimationModifierInstances.Add(Instance);
}

void UAnimationModifiersAssetUserData::RemoveAnimationModifierInstance(UAnimationModifier* Instance)
{
	checkf(AnimationModifierInstances.Contains(Instance), TEXT("Instance suppose to be removed is not found"));
	AnimationModifierInstances.Remove(Instance);
}

const TArray<UAnimationModifier*>& UAnimationModifiersAssetUserData::GetAnimationModifierInstances() const
{
	return AnimationModifierInstances;
}

void UAnimationModifiersAssetUserData::ChangeAnimationModifierIndex(UAnimationModifier* Instance, int32 Direction)
{
	checkf(AnimationModifierInstances.Contains(Instance), TEXT("Instance suppose to be moved is not found"));
	const int32 CurrentIndex = AnimationModifierInstances.IndexOfByKey(Instance);
	const int32 NewIndex = FMath::Clamp(CurrentIndex + Direction, 0, AnimationModifierInstances.Num() - 1);
	if (CurrentIndex != NewIndex)
	{
		AnimationModifierInstances.Swap(CurrentIndex, NewIndex);
	}
}

void UAnimationModifiersAssetUserData::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	RemoveInvalidModifiers();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#if WITH_EDITOR
void UAnimationModifiersAssetUserData::PostEditChangeOwner()
{
	Super::PostEditChangeOwner();

	// We cant call blueprint implemented functions while routing post load
	if (FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		return;
	}

	UAnimSequence* ModifiedAnimSequence = Cast<UAnimSequence>(GetOuter());
	if (!ModifiedAnimSequence)
	{
		return;
	}

	for (UAnimationModifier* Modifier : AnimationModifierInstances)
	{
		if (!Modifier->bReapplyPostOwnerChange || Modifier->IsCurrentlyApplyingModifier())
		{
			continue;
		}

		Modifier->ApplyToAnimationSequence(ModifiedAnimSequence);
	}
}
#endif // WITH_EDITOR

void UAnimationModifiersAssetUserData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void UAnimationModifiersAssetUserData::PostLoad()
{
	Super::PostLoad();
	RemoveInvalidModifiers();
}

void UAnimationModifiersAssetUserData::RemoveInvalidModifiers()
{
	// This will catch force-deleted blueprints to be removed from our stored array
	AnimationModifierInstances.RemoveAll([](UAnimationModifier* Modifier) { return Modifier == nullptr; });
}
