// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/ActorModifierCoreDefs.h"
#include "Modifiers/ActorModifierCoreBase.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"

#if WITH_EDITOR
#include "Styling/SlateIconFinder.h"
#endif

FActorModifierCoreMetadata::FActorModifierCoreMetadata()
	: bIsStack(false)
#if WITH_EDITOR
	, bHidden(false)
#endif
{}

FActorModifierCoreMetadata::FActorModifierCoreMetadata(const UActorModifierCoreBase* InModifier)
{
	check(InModifier && InModifier->IsTemplate());

	Class = InModifier->GetClass();
	bIsStack = InModifier->IsA<UActorModifierCoreStack>();
	Category = DefaultCategory;

#if WITH_EDITOR
	bHidden = false;
#endif

	SetProfilerClass<FActorModifierCoreProfiler>();
	SetCompatibilityRule([](const AActor* InActor)->bool
	{
		return IsValid(InActor);
	});

#if WITH_EDITOR
	Icon = FSlateIconFinder::FindIconForClass(Class);
	if (!Icon.IsSet())
	{
		Icon = FSlateIconFinder::FindIconForClass(UActorModifierCoreBase::StaticClass());
	}
#endif
}

bool FActorModifierCoreMetadata::IsDisallowedAfter(const FName& InModifierName) const
{
	return DisallowedAfter.Contains(InModifierName);
}

bool FActorModifierCoreMetadata::IsDisallowedBefore(const FName& InModifierName) const
{
	return DisallowedBefore.Contains(InModifierName);
}

bool FActorModifierCoreMetadata::IsAllowedAfter(const FName& InModifierName) const
{
	return !IsDisallowedAfter(InModifierName);
}

bool FActorModifierCoreMetadata::IsAllowedBefore(const FName& InModifierName) const
{
	return !IsDisallowedBefore(InModifierName);
}

bool FActorModifierCoreMetadata::IsCompatibleWith(const AActor* InActor) const
{
	return CompatibilityRuleFunction(InActor);
}

bool FActorModifierCoreMetadata::DependsOn(const FName& InModifierName) const
{
	if (InModifierName.IsNone() || Name == InModifierName)
	{
		return false;
	}

	if (Dependencies.Contains(InModifierName))
	{
		return true;
	}

	if (const UActorModifierCoreSubsystem* Subsystem = UActorModifierCoreSubsystem::Get())
	{
		TArray<FName> OutDependencies;
		return Subsystem->BuildModifierDependencies(Name, OutDependencies) && OutDependencies.Contains(InModifierName);
	}
	return false;
}

bool FActorModifierCoreMetadata::IsRequiredBy(const FName& InModifierName) const
{
	if (InModifierName.IsNone() || Name == InModifierName)
	{
		return false;
	}

	if (const UActorModifierCoreSubsystem* Subsystem = UActorModifierCoreSubsystem::Get())
	{
		TArray<FName> OutDependencies;
		return Subsystem->BuildModifierDependencies(InModifierName, OutDependencies) && OutDependencies.Contains(Name);
	}
	return false;
}

bool FActorModifierCoreMetadata::ShouldAvoidBefore(FName InCategory) const
{
	return AvoidedBeforeCategories.Contains(InCategory);
}

bool FActorModifierCoreMetadata::ShouldAvoidAfter(FName InCategory) const
{
	return AvoidedAfterCategories.Contains(InCategory);
}

bool FActorModifierCoreMetadata::ResetDefault()
{
	if (const UActorModifierCoreBase* CDO = GetClass()->GetDefaultObject<UActorModifierCoreBase>())
	{
		const FActorModifierCoreMetadata& CDOMetadata = CDO->GetModifierMetadata();

#if WITH_EDITOR
		Color = CDOMetadata.Color;
		Icon = CDOMetadata.Icon;
		bHidden = CDOMetadata.bHidden;
#endif

		Dependencies = CDOMetadata.Dependencies;
		DisallowedAfter = CDOMetadata.DisallowedAfter;
		DisallowedBefore = CDOMetadata.DisallowedBefore;
		bTickAllowed = CDOMetadata.bTickAllowed;
		bMultipleAllowed = CDOMetadata.bMultipleAllowed;
		CompatibilityRuleFunction = CDOMetadata.CompatibilityRuleFunction;
		ProfilerFunction = CDOMetadata.ProfilerFunction;

		return true;
	}

	return false;
}

#if WITH_EDITOR
FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetDisplayName(const FText& InName)
{
	DisplayName = InName;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetDescription(const FText& InDescription)
{
	Description = InDescription;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetIcon(const FSlateIcon& InIcon)
{
	Icon = InIcon;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetHidden(bool bInHidden)
{
	bHidden = bInHidden;
	return *this;
}
#endif

void FActorModifierCoreMetadata::SetupProfilerInstanceInternal(TSharedPtr<FActorModifierCoreProfiler> InProfiler, UActorModifierCoreBase* InModifier, const FName& InProfilerType) const
{
	if (!InProfiler.IsValid() || InProfilerType.IsNone())
	{
		return;
	}

	InProfiler->ConstructInternal(InModifier, InProfilerType);
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetName(FName InName)
{
	Name = InName;

#if WITH_EDITOR
	DisplayName = FText::FromString(FName::NameToDisplayString(Name.ToString(), false));
#endif

	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetCategory(FName InCategory)
{
	Category = InCategory;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::AllowTick(bool bInAllowed)
{
	bTickAllowed = bInAllowed;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::AllowMultiple(bool bInAllowed)
{
	bMultipleAllowed = bInAllowed;
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::AddDependency(const FName& InModifierName)
{
	Dependencies.AddUnique(InModifierName);
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::DisallowBefore(const FName& InModifierName)
{
	DisallowedBefore.Add(InModifierName);
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::DisallowAfter(const FName& InModifierName)
{
	DisallowedAfter.Add(InModifierName);
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::AvoidBeforeCategory(const FName& InCategory)
{
	AvoidedBeforeCategories.Add(InCategory);
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::AvoidAfterCategory(const FName& InCategory)
{
	AvoidedAfterCategories.Add(InCategory);
	return *this;
}

FActorModifierCoreMetadata& FActorModifierCoreMetadata::SetCompatibilityRule(const TFunction<bool(const AActor*)>& InModifierRule)
{
	CompatibilityRuleFunction = InModifierRule;
	return *this;
}

UActorModifierCoreBase* FActorModifierCoreMetadata::CreateModifierInstance(UActorModifierCoreStack* InStack) const
{
	if (InStack && InStack->GetModifiedActor())
	{
		UActorModifierCoreBase* NewModifierInstance = NewObject<UActorModifierCoreBase>(InStack->GetModifiedActor(), Class, NAME_None, RF_Transactional);
		NewModifierInstance->PostModifierCreation(InStack);
		return NewModifierInstance;
	}

	return nullptr;
}

TSharedPtr<FActorModifierCoreProfiler> FActorModifierCoreMetadata::CreateProfilerInstance(UActorModifierCoreBase* InModifier) const
{
	if (InModifier && InModifier->GetModifiedActor())
	{
		return ProfilerFunction(InModifier);
	}

	return nullptr;
}

const FActorModifierCoreStackSearchOp& FActorModifierCoreStackSearchOp::GetDefault()
{
	static const FActorModifierCoreStackSearchOp Default = {};
	return Default;
}

FActorModifierCoreScopedLock::FActorModifierCoreScopedLock(UActorModifierCoreBase* InModifier)
{
	if (InModifier)
	{
		InModifier->LockModifierExecution();
		ModifiersWeak.Add(InModifier);
	}
}

FActorModifierCoreScopedLock::FActorModifierCoreScopedLock(const TSet<UActorModifierCoreBase*>& InModifiers)
{
	// Locking state to prevent from updating
	for (UActorModifierCoreBase* Modifier : InModifiers)
	{
		if (Modifier)
		{
			Modifier->LockModifierExecution();
            ModifiersWeak.Add(Modifier);
		}
	}
}

FActorModifierCoreScopedLock::~FActorModifierCoreScopedLock()
{
	// Unlocking state of modifier
	for (const TWeakObjectPtr<UActorModifierCoreBase>& ModifierWeak : ModifiersWeak)
	{
		if (UActorModifierCoreBase* Modifier = ModifierWeak.Get())
		{
			Modifier->UnlockModifierExecution();
		}
	}

	ModifiersWeak.Empty();
}
