// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/PropertyAnimatorCoreComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreComponent::AddAnimator(const UClass* InAnimatorClass)
{
	if (!InAnimatorClass)
	{
		return nullptr;
	}

	UPropertyAnimatorCoreBase* NewAnimator = NewObject<UPropertyAnimatorCoreBase>(this, InAnimatorClass, NAME_None, RF_Transactional);

	if (NewAnimator)
	{
		AnimatorsInternal = Animators;
		Animators.Add(NewAnimator);

		OnAnimatorsChanged();
	}

	return NewAnimator;
}

bool UPropertyAnimatorCoreComponent::RemoveAnimator(UPropertyAnimatorCoreBase* InAnimator)
{
	if (!Animators.Contains(InAnimator))
	{
		return false;
	}

	AnimatorsInternal = Animators;
	Animators.Remove(InAnimator);

	OnAnimatorsChanged();

	return true;
}

void UPropertyAnimatorCoreComponent::OnAnimatorsSetEnabled(const UWorld* InWorld, bool bInEnabled, bool bInTransact)
{
	if (GetWorld() == InWorld)
	{
#if WITH_EDITOR
		if (bInTransact)
		{
			Modify();
		}
#endif

		SetAnimatorsEnabled(bInEnabled);
	}
}

void UPropertyAnimatorCoreComponent::OnAnimatorsChanged()
{
	TSet<TObjectPtr<UPropertyAnimatorCoreBase>> RemovedAnimators = AnimatorsInternal.Difference(Animators);
	TSet<TObjectPtr<UPropertyAnimatorCoreBase>> AddedAnimators = Animators.Difference(AnimatorsInternal);
	AnimatorsInternal.Empty();

	for (const TObjectPtr<UPropertyAnimatorCoreBase>& RemovedAnimator : RemovedAnimators)
	{
		if (RemovedAnimator)
		{
			RemovedAnimator->SetAnimatorEnabled(false);
			RemovedAnimator->OnAnimatorRemoved();
		}
	}

	const bool bShouldAnimatorsTick = ShouldAnimatorsTick();
	for (const TObjectPtr<UPropertyAnimatorCoreBase>& AddedAnimator : AddedAnimators)
	{
		if (AddedAnimator)
		{
			AddedAnimator->SetAnimatorDisplayName(GetAnimatorName(AddedAnimator));
			AddedAnimator->OnAnimatorAdded();
			AddedAnimator->SetAnimatorEnabled(true);

			if (!bShouldAnimatorsTick)
			{
				AddedAnimator->SetAnimatorEnabled(false);
			}
		}
	}

	OnAnimatorsEnabledChanged();
}

void UPropertyAnimatorCoreComponent::OnAnimatorsEnabledChanged()
{
	const bool bEnableAnimators = ShouldAnimatorsTick();

	if (IsComponentTickEnabled() == bEnableAnimators)
	{
		return;
	}

	for (const TObjectPtr<UPropertyAnimatorCoreBase>& Animator : Animators)
	{
		if (!IsValid(Animator))
		{
			continue;
		}

		if (bEnableAnimators)
		{
			Animator->OnAnimatorEnabled();
		}
		else
		{
			Animator->OnAnimatorDisabled();
		}
	}

	SetComponentTickEnabled(bEnableAnimators);
}

bool UPropertyAnimatorCoreComponent::ShouldAnimatorsTick() const
{
	return bAnimatorsEnabled
		&& !Animators.IsEmpty()
		&& !FMath::IsNearlyZero(AnimatorsMagnitude);
}

FName UPropertyAnimatorCoreComponent::GetAnimatorName(const UPropertyAnimatorCoreBase* InAnimator)
{
	if (!InAnimator)
	{
		return NAME_None;
	}

	FString NewAnimatorName = InAnimator->GetName();

	const int32 Idx = NewAnimatorName.Find(InAnimator->GetAnimatorOriginalName().ToString());
	if (Idx != INDEX_NONE)
	{
		NewAnimatorName = NewAnimatorName.RightChop(Idx);
	}

	return FName(NewAnimatorName);
}

void UPropertyAnimatorCoreComponent::TickComponent(float InDeltaTime, ELevelTick InTickType, FActorComponentTickFunction* InThisTickFunction)
{
	Super::TickComponent(InDeltaTime, InTickType, InThisTickFunction);

	if (!ShouldAnimatorsTick())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const bool bIsSupportedWorld = IsValid(World) && (World->IsGameWorld() || World->IsEditorWorld());

	if (!bIsSupportedWorld)
	{
		return;
	}

	for (const TObjectPtr<UPropertyAnimatorCoreBase>& Animator : Animators)
	{
		if (!IsValid(Animator))
		{
			continue;
		}

		Animator->EvaluateAnimator();
	}
}

UPropertyAnimatorCoreComponent* UPropertyAnimatorCoreComponent::FindOrAdd(AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return nullptr;
	}

	if (UPropertyAnimatorCoreComponent* ExistingComponent = InActor->FindComponentByClass<UPropertyAnimatorCoreComponent>())
	{
		return ExistingComponent;
	}

#if WITH_EDITOR
	InActor->Modify();
#endif

	const UClass* const ComponentClass = UPropertyAnimatorCoreComponent::StaticClass();

	// Construct the new component and attach as needed
	UPropertyAnimatorCoreComponent* const PropertyAnimatorComponent = NewObject<UPropertyAnimatorCoreComponent>(InActor
		, ComponentClass
		, MakeUniqueObjectName(InActor, ComponentClass, TEXT("PropertyAnimatorComponent"))
		, RF_Transactional);

	// Add to SerializedComponents array so it gets saved
	InActor->AddInstanceComponent(PropertyAnimatorComponent);
	PropertyAnimatorComponent->OnComponentCreated();
	PropertyAnimatorComponent->RegisterComponent();

#if WITH_EDITOR
	// Rerun construction scripts
	InActor->RerunConstructionScripts();
#endif

	return PropertyAnimatorComponent;
}

UPropertyAnimatorCoreComponent::UPropertyAnimatorCoreComponent()
{
	if (!IsTemplate())
	{
		bTickInEditor = true;
		PrimaryComponentTick.bCanEverTick = true;
		PrimaryComponentTick.bHighPriority = true;

		// Used to toggle animators state in world
		UPropertyAnimatorCoreSubsystem::OnAnimatorsSetEnabledDelegate.AddUObject(this, &UPropertyAnimatorCoreComponent::OnAnimatorsSetEnabled);
	}
}

void UPropertyAnimatorCoreComponent::SetAnimatorsEnabled(bool bInEnabled)
{
	if (bAnimatorsEnabled == bInEnabled)
	{
		return;
	}

	bAnimatorsEnabled = bInEnabled;
	OnAnimatorsEnabledChanged();
}

void UPropertyAnimatorCoreComponent::SetAnimatorsMagnitude(float InMagnitude)
{
	InMagnitude = FMath::Clamp(InMagnitude, 0.f, 1.f);

	if (FMath::IsNearlyEqual(AnimatorsMagnitude, InMagnitude))
	{
		return;
	}

	AnimatorsMagnitude = InMagnitude;
	OnAnimatorsEnabledChanged();
}

void UPropertyAnimatorCoreComponent::DestroyComponent(bool bPromoteChildren)
{
	AnimatorsInternal = Animators;
	Animators.Empty();

	OnAnimatorsChanged();

	Super::DestroyComponent(bPromoteChildren);
}

#if WITH_EDITOR
void UPropertyAnimatorCoreComponent::PostEditUndo()
{
	Super::PostEditUndo();

	OnAnimatorsEnabledChanged();
}

void UPropertyAnimatorCoreComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	const FName MemberName = PropertyAboutToChange ? PropertyAboutToChange->GetFName() : NAME_None;

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreComponent, Animators))
	{
		AnimatorsInternal = Animators;
	}
}

void UPropertyAnimatorCoreComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreComponent, bAnimatorsEnabled)
		|| MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreComponent, AnimatorsMagnitude))
	{
		OnAnimatorsEnabledChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreComponent, Animators))
	{
		OnAnimatorsChanged();
	}
}
#endif

void UPropertyAnimatorCoreComponent::SetAnimators(const TSet<TObjectPtr<UPropertyAnimatorCoreBase>>& InAnimators)
{
	if (Animators.Includes(InAnimators) && Animators.Num() == InAnimators.Num())
	{
		return;
	}

	AnimatorsInternal = Animators;
	Animators = InAnimators;
	OnAnimatorsChanged();
}

void UPropertyAnimatorCoreComponent::ForEachAnimator(const TFunctionRef<bool(UPropertyAnimatorCoreBase*)> InFunction) const
{
	for (const TObjectPtr<UPropertyAnimatorCoreBase>& Animator : Animators)
	{
		if (Animator)
		{
			if (!InFunction(Animator))
			{
				break;
			}
		}
	}
}
