// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "Animators/PropertyAnimatorCoreBase.h"
#include "Components/PropertyAnimatorCoreComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Presets/PropertyAnimatorCorePresetBase.h"
#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorCoreResolver.h"
#include "TimeSources/PropertyAnimatorCoreTimeSourceBase.h"
#include "TimeSources/PropertyAnimatorCoreWorldTimeSource.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "EngineAnalytics.h"
#include "ScopedTransaction.h"
#endif

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreSubsystem"

UPropertyAnimatorCoreSubsystem::FOnAnimatorsSetEnabled UPropertyAnimatorCoreSubsystem::OnAnimatorsSetEnabledDelegate;

void UPropertyAnimatorCoreSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Register default time source
	RegisterTimeSourceClass(UPropertyAnimatorCoreWorldTimeSource::StaticClass());

	RegisterAnimatorClasses();

	// Register property setter resolver
	RegisterSetterResolver(TEXT("bVisible"), [](const UObject* InOwner)->UFunction*
	{
		return InOwner->FindFunction(TEXT("SetVisibility"));
	});

	RegisterSetterResolver(TEXT("bHidden"), [](const UObject* InOwner)->UFunction*
	{
		return InOwner->FindFunction(TEXT("SetActorHiddenInGame"));
	});
}

void UPropertyAnimatorCoreSubsystem::Deinitialize()
{
	AnimatorsWeak.Empty();
	HandlersWeak.Empty();
	TimeSourcesWeak.Empty();
	ResolversWeak.Empty();
	PresetsWeak.Empty();
	SetterResolvers.Empty();

	Super::Deinitialize();
}

UPropertyAnimatorCoreSubsystem* UPropertyAnimatorCoreSubsystem::Get()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<UPropertyAnimatorCoreSubsystem>();
	}

	return nullptr;
}

bool UPropertyAnimatorCoreSubsystem::RegisterAnimatorClass(const UClass* InPropertyControllerClass)
{
	if (!IsValid(InPropertyControllerClass))
	{
		return false;
	}

	if (!InPropertyControllerClass->IsChildOf(UPropertyAnimatorCoreBase::StaticClass())
		|| InPropertyControllerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsAnimatorClassRegistered(InPropertyControllerClass))
	{
		return false;
	}

	if (UPropertyAnimatorCoreBase* CDO = InPropertyControllerClass->GetDefaultObject<UPropertyAnimatorCoreBase>())
	{
		AnimatorsWeak.Add(CDO);

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterAnimatorClass(const UClass* InPropertyControllerClass)
{
	if (!IsValid(InPropertyControllerClass))
	{
		return false;
	}

	for (TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>>::TIterator It(AnimatorsWeak); It; ++It)
	{
		if ((*It)->GetClass() == InPropertyControllerClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsAnimatorClassRegistered(const UClass* InPropertyControllerClass) const
{
	if (!IsValid(InPropertyControllerClass))
	{
		return false;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& Controller : AnimatorsWeak)
	{
		if (Controller->GetClass() == InPropertyControllerClass)
		{
			return true;
		}
	}

	return false;
}

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreSubsystem::GetAnimatorRegistered(const UClass* InAnimatorClass) const
{
	if (!IsValid(InAnimatorClass))
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& AnimatorWeak : AnimatorsWeak)
	{
		if (UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get())
		{
			if (Animator->GetClass() == InAnimatorClass)
			{
				return Animator;
			}
		}
	}

	return nullptr;
}

bool UPropertyAnimatorCoreSubsystem::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData, bool bInCheckNestedProperties) const
{
	if (!InPropertyData.IsResolved())
	{
		return false;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& Controller : AnimatorsWeak)
	{
		if (bInCheckNestedProperties)
		{
			TSet<FPropertyAnimatorCoreData> OutProperties;
			if (Controller->GetPropertiesSupported(InPropertyData, OutProperties))
			{
				return true;
			}
		}
		else
		{
			if (Controller->IsPropertySupported(InPropertyData))
			{
				return true;
			}
		}
	}

	return false;
}

TSet<UPropertyAnimatorCoreBase*> UPropertyAnimatorCoreSubsystem::GetPropertyLinkedAnimators(const FPropertyAnimatorCoreData& InPropertyData) const
{
	TSet<UPropertyAnimatorCoreBase*> ExistingControllers = GetExistingAnimators(InPropertyData);

	for (TSet<UPropertyAnimatorCoreBase*>::TIterator It(ExistingControllers); It; ++It)
	{
		if (!(*It)->IsPropertyLinked(InPropertyData))
		{
			It.RemoveCurrent();
		}
	}

	return ExistingControllers;
}

TSet<UPropertyAnimatorCoreBase*> UPropertyAnimatorCoreSubsystem::GetExistingAnimators(const FPropertyAnimatorCoreData& InPropertyData) const
{
	TSet<UPropertyAnimatorCoreBase*> ExistingPropertyControllers;

	if (!InPropertyData.IsResolved())
	{
		return ExistingPropertyControllers;
	}

	const AActor* Actor = InPropertyData.GetOwningActor();

	for (UPropertyAnimatorCoreBase* Controller : GetExistingAnimators(Actor))
	{
		TSet<FPropertyAnimatorCoreData> OutProperties;
		if (Controller->GetPropertiesSupported(InPropertyData, OutProperties))
		{
			ExistingPropertyControllers.Add(Controller);
		}
	}

	return ExistingPropertyControllers;
}

TSet<UPropertyAnimatorCoreBase*> UPropertyAnimatorCoreSubsystem::GetExistingAnimators(const AActor* InActor) const
{
	TSet<UPropertyAnimatorCoreBase*> ExistingPropertyControllers;

	if (!IsValid(InActor))
	{
		return ExistingPropertyControllers;
	}

	if (const UPropertyAnimatorCoreComponent* PropertyComponent = InActor->FindComponentByClass<UPropertyAnimatorCoreComponent>())
	{
		PropertyComponent->ForEachAnimator([&ExistingPropertyControllers](UPropertyAnimatorCoreBase* InController)->bool
		{
			ExistingPropertyControllers.Add(InController);
			return true;
		});
	}

	return ExistingPropertyControllers;
}

TSet<UPropertyAnimatorCoreBase*> UPropertyAnimatorCoreSubsystem::GetAvailableAnimators(const FPropertyAnimatorCoreData* InPropertyData) const
{
	TSet<UPropertyAnimatorCoreBase*> AvailablePropertyControllers;

	if (InPropertyData && !InPropertyData->IsResolved())
	{
		return AvailablePropertyControllers;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& Controller : AnimatorsWeak)
	{
		bool bIsPropertySupported = true;
		if (InPropertyData)
		{
			TSet<FPropertyAnimatorCoreData> OutProperties;
			if (!Controller->GetPropertiesSupported(*InPropertyData, OutProperties))
			{
				bIsPropertySupported = false;
			}
		}

		if (bIsPropertySupported)
		{
			AvailablePropertyControllers.Add(Controller.Get());
		}
	}

	return AvailablePropertyControllers;
}

TSet<UPropertyAnimatorCoreBase*> UPropertyAnimatorCoreSubsystem::GetAvailableAnimators() const
{
	TSet<UPropertyAnimatorCoreBase*> AvailablePropertyControllers;

	for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& Controller : AnimatorsWeak)
	{
		AvailablePropertyControllers.Add(Controller.Get());
	}

	return AvailablePropertyControllers;
}

UPropertyAnimatorCoreBase* UPropertyAnimatorCoreSubsystem::CreateAnimator(AActor* InActor, const UClass* InAnimatorClass, UPropertyAnimatorCorePresetBase* InPreset, bool bInTransact) const
{
	if (!IsValid(InActor) || !IsValid(InAnimatorClass))
	{
		return nullptr;
	}

	const TSet<UPropertyAnimatorCoreBase*> NewAnimators = CreateAnimators({InActor}, InAnimatorClass, InPreset, bInTransact);

	if (NewAnimators.IsEmpty())
	{
		return nullptr;
	}

	return NewAnimators.Array()[0];
}

TSet<UPropertyAnimatorCoreBase*> UPropertyAnimatorCoreSubsystem::CreateAnimators(const TSet<AActor*>& InActors, const UClass* InAnimatorClass, UPropertyAnimatorCorePresetBase* InPreset, bool bInTransact) const
{
	TSet<UPropertyAnimatorCoreBase*> NewAnimators;

	if (InActors.IsEmpty() || !IsValid(InAnimatorClass))
	{
		return NewAnimators;
	}

	const UPropertyAnimatorCoreBase* AnimatorCDO = GetAnimatorRegistered(InAnimatorClass);

	if (!AnimatorCDO)
	{
		return NewAnimators;
	}

	NewAnimators.Reserve(InActors.Num());

#if WITH_EDITOR
	const FText TransactionText = LOCTEXT("CreateAnimators", "Adding {0} animator to {1} actor(s)");
	const FText AnimatorName = FText::FromName(AnimatorCDO->GetAnimatorOriginalName());
	const FText ActorCount = FText::FromString(FString::FromInt(InActors.Num()));

	FScopedTransaction Transaction(FText::Format(TransactionText, AnimatorName, ActorCount), bInTransact);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(TEXT("Class"), InAnimatorClass->GetName());
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.PropertyAnimator.CreateAnimator"), Attributes);
	}
#endif

	for (AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		UPropertyAnimatorCoreComponent* Component = UPropertyAnimatorCoreComponent::FindOrAdd(Actor);
		if (!Component)
		{
			continue;
		}

#if WITH_EDITOR
      	Component->Modify();
#endif

		UPropertyAnimatorCoreBase* NewActorAnimator = Component->AddAnimator(InAnimatorClass);

		if (!NewActorAnimator)
		{
			continue;
		}

#if WITH_EDITOR
		NewActorAnimator->Modify();
#endif

		// Optionally apply preset if any
		if (InPreset && InPreset->IsPresetSupported(Actor, NewActorAnimator))
		{
			InPreset->ApplyPreset(NewActorAnimator);
		}

		NewAnimators.Add(NewActorAnimator);

		UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.Broadcast(NewActorAnimator);
	}

	return NewAnimators;
}

bool UPropertyAnimatorCoreSubsystem::RemoveAnimator(UPropertyAnimatorCoreBase* InAnimator, bool bInTransact) const
{
	return RemoveAnimators({InAnimator}, bInTransact);
}

bool UPropertyAnimatorCoreSubsystem::RemoveAnimators(const TSet<UPropertyAnimatorCoreBase*> InAnimators, bool bInTransact) const
{
	if (InAnimators.IsEmpty())
	{
		return false;
	}

	bool bResult = true;

	for (UPropertyAnimatorCoreBase* Animator : InAnimators)
	{
		if (!IsValid(Animator))
		{
			continue;
		}

		AActor* OwningActor = Animator->GetTypedOuter<AActor>();
		if (!OwningActor)
		{
			continue;
		}

		UPropertyAnimatorCoreComponent* Component = UPropertyAnimatorCoreComponent::FindOrAdd(OwningActor);
		if (!Component)
		{
			continue;
		}

#if WITH_EDITOR
		const FText TransactionText = LOCTEXT("RemoveAnimator", "Removing {0} animator(s)");
		const FText AnimatorCount = FText::FromString(FString::FromInt(InAnimators.Num()));

		FScopedTransaction Transaction(FText::Format(TransactionText, AnimatorCount), bInTransact);

		Component->Modify();
		Animator->Modify();
#endif

		const bool bAnimatorRemoved = Component->RemoveAnimator(Animator);

		if (bAnimatorRemoved)
		{
			UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.Broadcast(Animator);
		}

		bResult &= bAnimatorRemoved;
	}

	return bResult;
}

bool UPropertyAnimatorCoreSubsystem::ApplyAnimatorPreset(UPropertyAnimatorCoreBase* InAnimator, UPropertyAnimatorCorePresetBase* InPreset, bool bInTransact)
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate() || !IsValid(InPreset))
	{
		return false;
	}

	if (!InPreset->IsPresetApplied(InAnimator))
	{
#if WITH_EDITOR
		const FText TransactionText = LOCTEXT("ApplyAnimatorPreset", "Applying {0} preset on {1} animator");
		const FText PresetName = FText::FromString(InPreset->GetPresetDisplayName());
		const FText AnimatorName = FText::FromName(InAnimator->GetAnimatorOriginalName());

		FScopedTransaction Transaction(FText::Format(TransactionText, PresetName, AnimatorName), bInTransact);

		InAnimator->Modify();
#endif

		return InPreset->ApplyPreset(InAnimator);
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnapplyAnimatorPreset(UPropertyAnimatorCoreBase* InAnimator, UPropertyAnimatorCorePresetBase* InPreset, bool bInTransact)
{
	if (!IsValid(InAnimator) || InAnimator->IsTemplate() || !IsValid(InPreset))
	{
		return false;
	}

	if (InPreset->IsPresetApplied(InAnimator))
	{
#if WITH_EDITOR
		const FText TransactionText = LOCTEXT("UnapplyAnimatorPreset", "Unapplying {0} preset on {1} animator");
		const FText PresetName = FText::FromString(InPreset->GetPresetDisplayName());
		const FText AnimatorName = FText::FromName(InAnimator->GetAnimatorOriginalName());

		FScopedTransaction Transaction(FText::Format(TransactionText, PresetName, AnimatorName), bInTransact);

		InAnimator->Modify();
#endif

		return InPreset->UnapplyPreset(InAnimator);
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::LinkAnimatorProperty(UPropertyAnimatorCoreBase* InAnimator, FPropertyAnimatorCoreData& InProperty, bool bInTransact)
{
	return LinkAnimatorProperties(InAnimator, {InProperty}, bInTransact);
}

bool UPropertyAnimatorCoreSubsystem::LinkAnimatorProperties(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties, bool bInTransact)
{
	if (!IsValid(InAnimator)
		|| InAnimator->IsTemplate()
		|| InProperties.IsEmpty())
	{
		return false;
	}

	if (!InAnimator->IsPropertiesLinked(InProperties))
	{
#if WITH_EDITOR
		FText TransactionText;
		if (InProperties.Num() == 1)
		{
			const FText PropertyName = FText::FromName(InProperties.Array()[0].GetLeafPropertyName());
			const FText AnimatorName = FText::FromName(InAnimator->GetAnimatorOriginalName());

			TransactionText = FText::Format(
				LOCTEXT("LinkAnimatorProperty", "Linking {0} property to {1} animator")
				, PropertyName
				, AnimatorName
			);
		}
		else
		{
			const FText PropertyCount = FText::FromString(FString::FromInt(InProperties.Num()));
			const FText AnimatorName = FText::FromName(InAnimator->GetAnimatorOriginalName());

			TransactionText = FText::Format(
				LOCTEXT("LinkAnimatorProperties", "Linking {0} properties to {1} animator")
				, PropertyCount
				, AnimatorName
			);
		}

		FScopedTransaction Transaction(TransactionText, bInTransact);

		InAnimator->Modify();
#endif

		bool bResult = false;
		for (const FPropertyAnimatorCoreData& PropertyData : InProperties)
		{
			bResult |= InAnimator->LinkProperty(PropertyData);
		}

		return bResult;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnlinkAnimatorProperty(UPropertyAnimatorCoreBase* InAnimator, FPropertyAnimatorCoreData& InProperty, bool bInTransact)
{
	return UnlinkAnimatorProperties(InAnimator, {InProperty}, bInTransact);
}

bool UPropertyAnimatorCoreSubsystem::UnlinkAnimatorProperties(UPropertyAnimatorCoreBase* InAnimator, const TSet<FPropertyAnimatorCoreData>& InProperties, bool bInTransact)
{
	if (!IsValid(InAnimator)
		|| InAnimator->IsTemplate()
		|| InProperties.IsEmpty())
	{
		return false;
	}

	if (InAnimator->IsPropertiesLinked(InProperties))
	{
#if WITH_EDITOR
		FText TransactionText;
		if (InProperties.Num() == 1)
		{
			const FText PropertyName = FText::FromName(InProperties.Array()[0].GetLeafPropertyName());
			const FText AnimatorName = FText::FromName(InAnimator->GetAnimatorOriginalName());

			TransactionText = FText::Format(
				LOCTEXT("UnlinkAnimatorProperty", "Unlinking {0} property from {1} animator")
				, PropertyName
				, AnimatorName
			);
		}
		else
		{
			const FText PropertyCount = FText::FromString(FString::FromInt(InProperties.Num()));
			const FText AnimatorName = FText::FromName(InAnimator->GetAnimatorOriginalName());

			TransactionText = FText::Format(
				LOCTEXT("UnlinkAnimatorProperties", "Unlinking {0} properties from {1} animator")
				, PropertyCount
				, AnimatorName
			);
		}

		FScopedTransaction Transaction(TransactionText, bInTransact);

		InAnimator->Modify();
#endif

		bool bResult = false;
		for (const FPropertyAnimatorCoreData& PropertyData : InProperties)
		{
			bResult |= InAnimator->UnlinkProperty(PropertyData);
		}

		return bResult;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::RegisterHandlerClass(const UClass* InHandlerClass)
{
	if (!IsValid(InHandlerClass))
	{
		return false;
	}

	if (!InHandlerClass->IsChildOf(UPropertyAnimatorCoreHandlerBase::StaticClass())
		|| InHandlerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsHandlerClassRegistered(InHandlerClass))
	{
		return false;
	}

	if (UPropertyAnimatorCoreHandlerBase* CDO = InHandlerClass->GetDefaultObject<UPropertyAnimatorCoreHandlerBase>())
	{
		HandlersWeak.Add(CDO);

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterHandlerClass(const UClass* InHandlerClass)
{
	if (!IsValid(InHandlerClass))
	{
		return false;
	}

	for (TSet<TWeakObjectPtr<UPropertyAnimatorCoreHandlerBase>>::TIterator It(HandlersWeak); It; ++It)
	{
		if (It->Get()->GetClass() == InHandlerClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsHandlerClassRegistered(const UClass* InHandlerClass) const
{
	for (const TWeakObjectPtr<UPropertyAnimatorCoreHandlerBase>& HandlerWeakPair : HandlersWeak)
	{
		if (HandlerWeakPair->GetClass() == InHandlerClass)
		{
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::RegisterResolverClass(const UClass* InResolverClass)
{
	if (!IsValid(InResolverClass))
	{
		return false;
	}

	if (!InResolverClass->IsChildOf(UPropertyAnimatorCoreResolver::StaticClass())
		|| InResolverClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsResolverClassRegistered(InResolverClass))
	{
		return false;
	}

	if (UPropertyAnimatorCoreResolver* CDO = InResolverClass->GetDefaultObject<UPropertyAnimatorCoreResolver>())
	{
		ResolversWeak.Add(CDO);

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterResolverClass(const UClass* InResolverClass)
{
	if (!IsValid(InResolverClass))
	{
		return false;
	}

	for (TSet<TWeakObjectPtr<UPropertyAnimatorCoreResolver>>::TIterator It(ResolversWeak); It; ++It)
	{
		if (It->Get()->GetClass() == InResolverClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsResolverClassRegistered(const UClass* InResolverClass) const
{
	for (const TWeakObjectPtr<UPropertyAnimatorCoreResolver>& ResolverWeakPair : ResolversWeak)
	{
		if (ResolverWeakPair->GetClass() == InResolverClass)
		{
			return true;
		}
	}

	return false;
}

void UPropertyAnimatorCoreSubsystem::GetResolvableProperties(const FPropertyAnimatorCoreData& InPropertyData, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	for (const TWeakObjectPtr<UPropertyAnimatorCoreResolver>& ResolverWeak : ResolversWeak)
	{
		if (UPropertyAnimatorCoreResolver* Resolver = ResolverWeak.Get())
		{
			Resolver->GetResolvableProperties(InPropertyData, OutProperties);
		}
	}
}

bool UPropertyAnimatorCoreSubsystem::RegisterTimeSourceClass(UClass* InTimeSourceClass)
{
	if (!IsValid(InTimeSourceClass))
	{
		return false;
	}

	if (!InTimeSourceClass->IsChildOf(UPropertyAnimatorCoreTimeSourceBase::StaticClass())
		|| InTimeSourceClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsTimeSourceClassRegistered(InTimeSourceClass))
	{
		return false;
	}

	UPropertyAnimatorCoreTimeSourceBase* TimeSourceCDO = InTimeSourceClass->GetDefaultObject<UPropertyAnimatorCoreTimeSourceBase>();
	if (!TimeSourceCDO)
	{
		return false;
	}

	const FName TimeSourceName = TimeSourceCDO->GetTimeSourceName();
	if (TimeSourceName.IsNone())
	{
		return false;
	}

	TimeSourcesWeak.Add(TimeSourceCDO);
	TimeSourceCDO->OnTimeSourceRegistered();

	return true;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterTimeSourceClass(UClass* InTimeSourceClass)
{
	if (!IsValid(InTimeSourceClass))
	{
		return false;
	}

	for (TSet<TWeakObjectPtr<UPropertyAnimatorCoreTimeSourceBase>>::TIterator It(TimeSourcesWeak); It; ++It)
	{
		if (It->Get()->GetClass() == InTimeSourceClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsTimeSourceClassRegistered(UClass* InTimeSourceClass) const
{
	if (!IsValid(InTimeSourceClass))
	{
		return false;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreTimeSourceBase>& TimeSourceWeak : TimeSourcesWeak)
	{
		if (const UPropertyAnimatorCoreTimeSourceBase* TimeSource = TimeSourceWeak.Get())
		{
			if (TimeSource->GetClass() == InTimeSourceClass)
			{
				return true;
			}
		}
	}

	return false;
}

TArray<FName> UPropertyAnimatorCoreSubsystem::GetTimeSourceNames() const
{
	TArray<FName> TimeSourceNames;
	TimeSourceNames.Reserve(TimeSourcesWeak.Num());

	for (const TWeakObjectPtr<UPropertyAnimatorCoreTimeSourceBase>& TimeSourceWeak : TimeSourcesWeak)
	{
		if (const UPropertyAnimatorCoreTimeSourceBase* TimeSource = TimeSourceWeak.Get())
		{
			TimeSourceNames.Add(TimeSource->GetTimeSourceName());
		}
	}

	return TimeSourceNames;
}

UPropertyAnimatorCoreTimeSourceBase* UPropertyAnimatorCoreSubsystem::GetTimeSource(FName InTimeSourceName) const
{
	if (InTimeSourceName.IsNone())
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreTimeSourceBase>& TimeSourceWeak : TimeSourcesWeak)
	{
		if (UPropertyAnimatorCoreTimeSourceBase* TimeSource = TimeSourceWeak.Get())
		{
			if (TimeSource->GetTimeSourceName() == InTimeSourceName)
			{
				return TimeSource;
			}
		}
	}

	return nullptr;
}

UPropertyAnimatorCoreTimeSourceBase* UPropertyAnimatorCoreSubsystem::CreateNewTimeSource(FName InTimeSourceName, UPropertyAnimatorCoreBase* InAnimator)
{
	if (!IsValid(InAnimator) || InTimeSourceName.IsNone())
	{
		return nullptr;
	}

	const UPropertyAnimatorCoreTimeSourceBase* TimeSource = GetTimeSource(InTimeSourceName);

	if (!TimeSource)
	{
		return nullptr;
	}

	// Here unique name needs to be provided
	const UClass* TimeSourceClass = TimeSource->GetClass();
	const FName UniqueObjectName = MakeUniqueObjectName(InAnimator, TimeSourceClass, InTimeSourceName);
	return NewObject<UPropertyAnimatorCoreTimeSourceBase>(InAnimator, TimeSourceClass, UniqueObjectName);
}

bool UPropertyAnimatorCoreSubsystem::RegisterPresetClass(const UClass* InPresetClass)
{
	if (!IsValid(InPresetClass))
	{
		return false;
	}

	if (!InPresetClass->IsChildOf(UPropertyAnimatorCorePresetBase::StaticClass())
		|| InPresetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsPresetClassRegistered(InPresetClass))
	{
		return false;
	}

	if (UPropertyAnimatorCorePresetBase* CDO = InPresetClass->GetDefaultObject<UPropertyAnimatorCorePresetBase>())
	{
		PresetsWeak.Add(CDO);

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterPresetClass(const UClass* InPresetClass)
{
	if (!IsValid(InPresetClass))
	{
		return false;
	}

	for (TSet<TWeakObjectPtr<UPropertyAnimatorCorePresetBase>>::TIterator It(PresetsWeak); It; ++It)
	{
		if ((*It)->GetClass() == InPresetClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsPresetClassRegistered(const UClass* InPresetClass) const
{
	if (!IsValid(InPresetClass))
	{
		return false;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCorePresetBase>& Preset : PresetsWeak)
	{
		if (Preset->GetClass() == InPresetClass)
		{
			return true;
		}
	}

	return false;
}

TSet<UPropertyAnimatorCorePresetBase*> UPropertyAnimatorCoreSubsystem::GetSupportedPresets(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator) const
{
	TSet<UPropertyAnimatorCorePresetBase*> SupportedPresets;

	for (const TWeakObjectPtr<UPropertyAnimatorCorePresetBase>& PresetWeak : PresetsWeak)
	{
		UPropertyAnimatorCorePresetBase* Preset = PresetWeak.Get();

		if (!Preset)
		{
			continue;
		}

		if (!Preset->IsPresetSupported(InActor, InAnimator))
		{
			continue;
		}

		SupportedPresets.Add(Preset);
	}

	return SupportedPresets;
}

TSet<UPropertyAnimatorCorePresetBase*> UPropertyAnimatorCoreSubsystem::GetAvailablePresets() const
{
	TSet<UPropertyAnimatorCorePresetBase*> AvailablePresets;
	AvailablePresets.Reserve(PresetsWeak.Num());

	Algo::TransformIf(
		PresetsWeak
		, AvailablePresets
		, [](const TWeakObjectPtr<UPropertyAnimatorCorePresetBase>& InPresetWeak)
		{
			return InPresetWeak.IsValid();
		}
		, [](const TWeakObjectPtr<UPropertyAnimatorCorePresetBase>& InPresetWeak)
		{
			return InPresetWeak.Get();
		}
	);

	return AvailablePresets;
}

bool UPropertyAnimatorCoreSubsystem::RegisterConverterClass(const UClass* InConverterClass)
{
	if (!IsValid(InConverterClass))
	{
		return false;
	}

	if (!InConverterClass->IsChildOf(UPropertyAnimatorCoreConverterBase::StaticClass())
		|| InConverterClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	if (IsConverterClassRegistered(InConverterClass))
	{
		return false;
	}

	if (UPropertyAnimatorCoreConverterBase* CDO = InConverterClass->GetDefaultObject<UPropertyAnimatorCoreConverterBase>())
	{
		ConvertersWeak.Add(CDO);

		return true;
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterConverterClass(const UClass* InConverterClass)
{
	if (!IsValid(InConverterClass))
	{
		return false;
	}

	for (TSet<TWeakObjectPtr<UPropertyAnimatorCoreConverterBase>>::TIterator It(ConvertersWeak); It; ++It)
	{
		if ((*It)->GetClass() == InConverterClass)
		{
			It.RemoveCurrent();
			return true;
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsConverterClassRegistered(const UClass* InConverterClass)
{
	if (!IsValid(InConverterClass))
	{
		return false;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreConverterBase>& ConverterWeak : ConvertersWeak)
	{
		if (const UPropertyAnimatorCoreConverterBase* Converter = ConverterWeak.Get())
		{
			if (Converter->GetClass() == InConverterClass)
			{
				return true;
			}
		}
	}

	return false;
}

bool UPropertyAnimatorCoreSubsystem::IsConversionSupported(const FPropertyBagPropertyDesc& InFromProperty, const FPropertyBagPropertyDesc& InToProperty)
{
	for (const TWeakObjectPtr<UPropertyAnimatorCoreConverterBase>& ConverterWeak : ConvertersWeak)
	{
		if (const UPropertyAnimatorCoreConverterBase* Converter = ConverterWeak.Get())
		{
			if (Converter->IsConversionSupported(InFromProperty, InToProperty))
			{
				return true;
			}
		}
	}

	return false;
}

TSet<UPropertyAnimatorCoreConverterBase*> UPropertyAnimatorCoreSubsystem::GetSupportedConverters(const FPropertyBagPropertyDesc& InFromProperty, const FPropertyBagPropertyDesc& InToProperty) const
{
	TSet<UPropertyAnimatorCoreConverterBase*> SupportedConverters;

	for (const TWeakObjectPtr<UPropertyAnimatorCoreConverterBase>& ConverterWeak : ConvertersWeak)
	{
		if (UPropertyAnimatorCoreConverterBase* Converter = ConverterWeak.Get())
		{
			if (Converter->IsConversionSupported(InFromProperty, InToProperty))
			{
				SupportedConverters.Add(Converter);
			}
		}
	}

	return SupportedConverters;
}

void UPropertyAnimatorCoreSubsystem::SetActorAnimatorsEnabled(const TSet<AActor*>& InActors, bool bInEnabled, bool bInTransact)
{
	if (InActors.IsEmpty())
	{
		return;
	}

#if WITH_EDITOR
	const FText TransactionText = bInEnabled
		? LOCTEXT("SetActorAnimatorsEnabled", "Actors Animators Enabled")
		: LOCTEXT("SetActorAnimatorsDisabled", "Actors Animators Disabled");

	FScopedTransaction Transaction(TransactionText, bInTransact);
#endif

	for (const AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		UPropertyAnimatorCoreComponent* AnimatorComponent = Actor->FindComponentByClass<UPropertyAnimatorCoreComponent>();

		if (!IsValid(AnimatorComponent))
		{
			continue;
		}

#if WITH_EDITOR
		AnimatorComponent->Modify();
#endif

		AnimatorComponent->SetAnimatorsEnabled(bInEnabled);
	}
}

void UPropertyAnimatorCoreSubsystem::SetLevelAnimatorsEnabled(const UWorld* InWorld, bool bInEnabled, bool bInTransact)
{
	if (!IsValid(InWorld))
	{
		return;
	}

#if WITH_EDITOR
	const FText TransactionText = bInEnabled
		? LOCTEXT("SetLevelAnimatorsEnabled", "Level Animators Enabled")
		: LOCTEXT("SetLevelAnimatorsDisabled", "Level Animators Disabled");

	FScopedTransaction Transaction(TransactionText, bInTransact);
#endif

	OnAnimatorsSetEnabledDelegate.Broadcast(InWorld, bInEnabled, bInTransact);
}

void UPropertyAnimatorCoreSubsystem::SetAnimatorsEnabled(const TSet<UPropertyAnimatorCoreBase*>& InAnimators, bool bInEnabled, bool bInTransact)
{
	if (InAnimators.IsEmpty())
	{
		return;
	}

#if WITH_EDITOR
	const FText TransactionText = bInEnabled
		? LOCTEXT("SetAnimatorsEnabled", "{0} Animators Enabled")
		: LOCTEXT("SetAnimatorsDisabled", "{0} Animators Disabled");

	FScopedTransaction Transaction(FText::Format(TransactionText, FText::FromString(FString::FromInt(InAnimators.Num()))), bInTransact);
#endif

	for (UPropertyAnimatorCoreBase* Animator : InAnimators)
	{
		if (!IsValid(Animator))
		{
			continue;
		}

#if WITH_EDITOR
		Animator->Modify();
#endif

		Animator->SetAnimatorEnabled(bInEnabled);
	}
}

bool UPropertyAnimatorCoreSubsystem::RegisterSetterResolver(FName InPropertyName, TFunction<UFunction*(const UObject*)>&& InFunction)
{
	if (InPropertyName.IsNone())
	{
		return false;
	}

	SetterResolvers.Emplace(InPropertyName, InFunction);

	return true;
}

bool UPropertyAnimatorCoreSubsystem::UnregisterSetterResolver(FName InPropertyName)
{
	return SetterResolvers.Remove(InPropertyName) > 0;
}

bool UPropertyAnimatorCoreSubsystem::IsSetterResolverRegistered(FName InPropertyName) const
{
	return SetterResolvers.Contains(InPropertyName);
}

UFunction* UPropertyAnimatorCoreSubsystem::ResolveSetter(FName InPropertyName, const UObject* InOwner)
{
	if (!IsValid(InOwner))
	{
		return nullptr;
	}

	if (const TFunction<UFunction*(const UObject*)>* SetterResolver = SetterResolvers.Find(InPropertyName))
	{
		return (*SetterResolver)(InOwner);
	}

	return nullptr;
}

UPropertyAnimatorCoreHandlerBase* UPropertyAnimatorCoreSubsystem::GetHandler(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (!InPropertyData.IsResolved())
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<UPropertyAnimatorCoreHandlerBase>& HandlerWeak : HandlersWeak)
	{
		UPropertyAnimatorCoreHandlerBase* Handler = HandlerWeak.Get();
		if (Handler && Handler->IsPropertySupported(InPropertyData))
		{
			return Handler;
		}
	}

	return nullptr;
}

void UPropertyAnimatorCoreSubsystem::RegisterAnimatorClasses()
{
	for (UClass* const Class : TObjectRange<UClass>())
	{
		RegisterAnimatorClass(Class);
		RegisterHandlerClass(Class);
		RegisterResolverClass(Class);
		RegisterTimeSourceClass(Class);
		RegisterPresetClass(Class);
		RegisterConverterClass(Class);
	}
}

#undef LOCTEXT_NAMESPACE
