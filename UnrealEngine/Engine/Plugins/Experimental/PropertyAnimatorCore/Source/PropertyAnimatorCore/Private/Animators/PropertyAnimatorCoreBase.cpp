// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorCoreBase.h"

#include "Components/PropertyAnimatorCoreComponent.h"
#include "GameFramework/Actor.h"
#include "Properties/PropertyAnimatorCoreGroupBase.h"
#include "Properties/PropertyAnimatorCoreResolver.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "TimeSources/PropertyAnimatorCoreTimeSourceBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogPropertyAnimatorCoreBase, Log, All);

UPropertyAnimatorCoreBase::FOnAnimatorUpdated UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate;
UPropertyAnimatorCoreBase::FOnAnimatorUpdated UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate;
UPropertyAnimatorCoreBase::FOnAnimatorUpdated UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate;
UPropertyAnimatorCoreBase::FOnAnimatorPropertyUpdated UPropertyAnimatorCoreBase::OnAnimatorPropertyLinkedDelegate;
UPropertyAnimatorCoreBase::FOnAnimatorPropertyUpdated UPropertyAnimatorCoreBase::OnAnimatorPropertyUnlinkedDelegate;

UPropertyAnimatorCoreBase::UPropertyAnimatorCoreBase()
{
	if (const UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		// Apply default time source
		const TArray<FName> TimeSources = AnimatorSubsystem->GetTimeSourceNames();
		SetTimeSourceName(!TimeSources.IsEmpty() ? TimeSources[0] : NAME_None);
	}

#if WITH_EDITOR
	if (!IsTemplate())
	{
		FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UPropertyAnimatorCoreBase::OnObjectReplaced);
	}
#endif
}

UPropertyAnimatorCoreComponent* UPropertyAnimatorCoreBase::GetAnimatorComponent() const
{
	return GetTypedOuter<UPropertyAnimatorCoreComponent>();
}

float UPropertyAnimatorCoreBase::GetAnimatorComponentMagnitude() const
{
	if (const UPropertyAnimatorCoreComponent* AnimatorComponent = GetAnimatorComponent())
	{
		return AnimatorComponent->GetAnimatorsMagnitude();
	}

	return 0.f;
}

void UPropertyAnimatorCoreBase::UpdateAnimatorDisplayName()
{
	TArray<FString> PropertiesNames;
	for (const FPropertyAnimatorCoreData& LinkedProperty : GetLinkedProperties())
	{
		PropertiesNames.Add(LinkedProperty.GetPropertyDisplayName().ToString());
	}

	auto FindCommonPrefix = [](const TConstArrayView<FString>& InNames)->FString
	{
		if (InNames.IsEmpty())
		{
			return FString();
		}

		FString CommonPrefix = InNames[0];

		for (int32 Index = 1; Index < InNames.Num(); ++Index)
		{
			const FString& CurrentString = InNames[Index];

			int32 CommonChars = 0;
			while (CommonChars < CommonPrefix.Len()
				&& CommonChars < CurrentString.Len()
				&& CommonPrefix[CommonChars] == CurrentString[CommonChars])
			{
				++CommonChars;
			}

			CommonPrefix = CommonPrefix.Left(CommonChars);
		}

		return CommonPrefix;
	};

	FString CommonPrefix = FindCommonPrefix(PropertiesNames);
	CommonPrefix = CommonPrefix.TrimChar(*TEXT("."));

	if (CommonPrefix.IsEmpty())
	{
		SetAnimatorDisplayName(GetFName());
	}
	else
	{
		SetAnimatorDisplayName(FName(GetAnimatorOriginalName().ToString() + TEXT("_") + CommonPrefix));
	}
}

UPropertyAnimatorCoreContext* UPropertyAnimatorCoreBase::GetLinkedPropertyContext(const FPropertyAnimatorCoreData& InProperty) const
{
	const TObjectPtr<UPropertyAnimatorCoreContext>* PropertyOptions = LinkedProperties.FindByPredicate([&InProperty](const UPropertyAnimatorCoreContext* InOptions)
	{
		return InOptions && InOptions->GetAnimatedProperty() == InProperty;
	});

	return PropertyOptions ? *PropertyOptions : nullptr;
}

void UPropertyAnimatorCoreBase::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
#endif
}

void UPropertyAnimatorCoreBase::PostLoad()
{
	CleanTimeSources();

	OnTimeSourceNameChanged();

	Super::PostLoad();

	CleanLinkedProperties();

	OnAnimatorEnabledChanged();
}

void UPropertyAnimatorCoreBase::PreDuplicate(FObjectDuplicationParameters& InDupParams)
{
	Super::PreDuplicate(InDupParams);

	constexpr bool bForceReset = true;
	RestoreProperties(bForceReset);
}

#if WITH_EDITOR
void UPropertyAnimatorCoreBase::PreEditUndo()
{
	Super::PreEditUndo();

	constexpr bool bForceReset = true;
	RestoreProperties(bForceReset);
}

void UPropertyAnimatorCoreBase::PostEditUndo()
{
	Super::PostEditUndo();

	constexpr bool bForceReset = true;
	RestoreProperties(bForceReset);
}

void UPropertyAnimatorCoreBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreBase, bAnimatorEnabled))
	{
		OnAnimatorEnabledChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreBase, TimeSourceName))
	{
		OnTimeSourceNameChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreBase, PropertyGroups))
	{
		OnPropertyGroupsChanged();
	}
}
#endif

AActor* UPropertyAnimatorCoreBase::GetAnimatorActor() const
{
	return GetTypedOuter<AActor>();
}

void UPropertyAnimatorCoreBase::SetAnimatorEnabled(bool bInIsEnabled)
{
	if (bAnimatorEnabled == bInIsEnabled)
	{
		return;
	}

	bAnimatorEnabled = bInIsEnabled;
	OnAnimatorEnabledChanged();
}

void UPropertyAnimatorCoreBase::SetTimeSourceName(FName InTimeSourceName)
{
	if (TimeSourceName == InTimeSourceName)
	{
		return;
	}

	const TArray<FString> TimeSources = GetTimeSourceNames();
	if (!TimeSources.Contains(InTimeSourceName))
	{
		return;
	}

	TimeSourceName = InTimeSourceName;
	OnTimeSourceNameChanged();
}

FName UPropertyAnimatorCoreBase::GetAnimatorOriginalName() const
{
	const UPropertyAnimatorCoreBase* CDO = GetClass()->GetDefaultObject<UPropertyAnimatorCoreBase>();
	return CDO ? CDO->AnimatorDisplayName : NAME_None;
}

bool UPropertyAnimatorCoreBase::GetPropertiesSupported(const FPropertyAnimatorCoreData& InPropertyData, TSet<FPropertyAnimatorCoreData>& OutProperties, bool bInRecursiveSearch) const
{
	const FProperty* LeafProperty = InPropertyData.GetLeafProperty();
	UObject* Owner = InPropertyData.GetOwner();

	// Is property editable
	if (!LeafProperty->HasAnyPropertyFlags(EPropertyFlags::CPF_Edit))
	{
		return false;
	}

	// We can directly control the member property
	if (IsPropertySupported(InPropertyData))
	{
		OutProperties.Add(InPropertyData);

		if (bInRecursiveSearch)
		{
			return true;
		}
	}

	// Look for inner properties that can be controlled too
	TFunction<bool(TArray<FProperty*>&, UObject*, TSet<FPropertyAnimatorCoreData>&)> FindSupportedPropertiesRecursively = [this, &FindSupportedPropertiesRecursively, bInRecursiveSearch, InPropertyData](TArray<FProperty*>& InChainProperties, UObject* InOwner, TSet<FPropertyAnimatorCoreData>& OutSupportedProperties)
	{
		FProperty* InLeafProperty = InChainProperties.Last();
		if (const FStructProperty* StructProp = CastField<FStructProperty>(InLeafProperty))
		{
			for (FProperty* Property : TFieldRange<FProperty>(StructProp->Struct))
			{
				if (!Property->HasAnyPropertyFlags(EPropertyFlags::CPF_Edit))
				{
					continue;
				}

				// Copy over resolver if any on that property
				FPropertyAnimatorCoreData PropertyControlData(InOwner, InChainProperties, Property, InPropertyData.GetPropertyResolverClass());

				// We can directly control this property
				if (IsPropertySupported(PropertyControlData))
				{
					OutSupportedProperties.Add(PropertyControlData);

					if (bInRecursiveSearch)
					{
						continue;
					}
				}

				// Check nested properties inside this property
				TArray<FProperty*> NestedChainProperties(InChainProperties);
				NestedChainProperties.Add(Property);
				FindSupportedPropertiesRecursively(NestedChainProperties, InOwner, OutSupportedProperties);
			}
		}

		return !OutSupportedProperties.IsEmpty();
	};

	TArray<FProperty*> ChainProperties = InPropertyData.GetChainProperties();

	return FindSupportedPropertiesRecursively(ChainProperties, Owner, OutProperties);
}

bool UPropertyAnimatorCoreBase::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	// Without any handler we can't control the property type
	if (!InPropertyData.GetPropertyHandler())
	{
		return false;
	}

	return IsPropertyDirectlySupported(InPropertyData)
		|| IsPropertyIndirectlySupported(InPropertyData);
}

void UPropertyAnimatorCoreBase::OnAnimatorEnabled()
{
	UE_LOG(LogPropertyAnimatorCoreBase
		, Log
		, TEXT("%s : PropertyAnimator %s (%s) enabled")
		, GetAnimatorActor() ? *GetAnimatorActor()->GetActorNameOrLabel() : TEXT("Invalid Actor")
		, *GetAnimatorDisplayName()
		, *GetAnimatorOriginalName().ToString());
}

void UPropertyAnimatorCoreBase::OnAnimatorDisabled()
{
	UE_LOG(LogPropertyAnimatorCoreBase
		, Log
		, TEXT("%s : PropertyAnimator %s (%s) disabled")
		, GetAnimatorActor() ? *GetAnimatorActor()->GetActorNameOrLabel() : TEXT("Invalid Actor")
		, *GetAnimatorDisplayName()
		, *GetAnimatorOriginalName().ToString());

	constexpr bool bForceReset = true;
	RestoreProperties(bForceReset);
}

TSubclassOf<UPropertyAnimatorCoreContext> UPropertyAnimatorCoreBase::GetPropertyContextClass(const FPropertyAnimatorCoreData& InProperty)
{
	return UPropertyAnimatorCoreContext::StaticClass();
}

void UPropertyAnimatorCoreBase::OnAnimatorEnabledChanged()
{
	if (bAnimatorEnabled)
	{
		OnAnimatorEnabled();
	}
	else
	{
		OnAnimatorDisabled();
	}
}

void UPropertyAnimatorCoreBase::CleanLinkedProperties()
{
	for (TArray<TObjectPtr<UPropertyAnimatorCoreContext>>::TIterator It(LinkedProperties); It; ++It)
	{
		const UPropertyAnimatorCoreContext* PropertyContext = It->Get();
		if (!PropertyContext || !PropertyContext->GetAnimatedProperty().IsResolved())
		{
			It.RemoveCurrent();
		}
	}
}

void UPropertyAnimatorCoreBase::CleanTimeSources()
{
	for (TMap<FName, TObjectPtr<UPropertyAnimatorCoreTimeSourceBase>>::TIterator It(TimeSourcesInstances); It; ++It)
	{
		// Remove invalid time sources
		if (!It->Value.Get())
		{
			It.RemoveCurrent();
		}
	}
}

void UPropertyAnimatorCoreBase::OnTimeSourceNameChanged()
{
	if (ActiveTimeSource)
	{
		ActiveTimeSource->DeactivateTimeSource();
	}

	ActiveTimeSource = FindOrAddTimeSource(TimeSourceName);

	if (ActiveTimeSource)
	{
		ActiveTimeSource->ActivateTimeSource();
	}
}

void UPropertyAnimatorCoreBase::EvaluateAnimator()
{
	UPropertyAnimatorCoreTimeSourceBase* TimeSource = GetActiveTimeSource();

	if (!GetAnimatorEnabled()
		|| !TimeSource)
	{
		return;
	}

	const TOptional<double> TimeElapsed = TimeSource->GetConditionalTimeElapsed();

	if (!TimeElapsed.IsSet())
	{
		return;
	}

	RestoreProperties();

	SaveProperties();

	EvaluatedPropertyValues.Reset();

	FPropertyAnimatorCoreEvaluationParameters Parameters;
	Parameters.AnimatorsMagnitude = GetAnimatorComponentMagnitude();
	Parameters.TimeElapsed = TimeElapsed.GetValue();

	bEvaluatingProperties = true;
	EvaluateProperties(Parameters);
	bEvaluatingProperties = false;
}

void UPropertyAnimatorCoreBase::OnObjectReplaced(const TMap<UObject*, UObject*>& InReplacementMap)
{
	constexpr bool bResolve = false;
	ForEachLinkedProperty<UPropertyAnimatorCoreContext>([&InReplacementMap](UPropertyAnimatorCoreContext* InContext, const FPropertyAnimatorCoreData& InProperty)->bool
	{
		const TWeakObjectPtr<UObject> OwnerWeak = InProperty.GetOwnerWeak();

		constexpr bool bEvenIfPendingKill = true;
		const UObject* Owner = OwnerWeak.Get(bEvenIfPendingKill);

		if (UObject* const* NewOwner = InReplacementMap.Find(Owner))
		{
			InContext->SetAnimatedPropertyOwner(*NewOwner);
		}

		return true;
	}, bResolve);
}

void UPropertyAnimatorCoreBase::OnPropertyGroupsChanged()
{
	TSet<FName> CurrentGroupNames;

	Algo::TransformIf(
		PropertyGroups
		, CurrentGroupNames
		, [](const UPropertyAnimatorCoreGroupBase* InGroup)->bool
		{
			return !!InGroup;
		}
		, [](const UPropertyAnimatorCoreGroupBase* InGroup)->FName
		{
			return InGroup->GetFName();
		}
	);

	// Remove assigned group in property context if removed from animator
	constexpr bool bResolve = false;
	ForEachLinkedProperty<UPropertyAnimatorCoreContext>([&CurrentGroupNames](UPropertyAnimatorCoreContext* InContext, const FPropertyAnimatorCoreData& InProperty)->bool
	{
		if (!CurrentGroupNames.Contains(InContext->GroupName))
		{
			InContext->SetGroup(nullptr);
		}

		return true;
	}, bResolve);
}

void UPropertyAnimatorCoreBase::RestoreProperties(bool bInForce)
{
	constexpr bool bResolve = false;
	ForEachLinkedProperty<UPropertyAnimatorCoreContext>([bInForce](UPropertyAnimatorCoreContext* InOptions, const FPropertyAnimatorCoreData& InPropertyData)
	{
		bool bRestore = false;

		if (bInForce)
		{
			bRestore = true;
		}
		if (InOptions->Mode == EPropertyAnimatorCoreMode::Additive)
		{
			bRestore = true;
		}
		else if (InOptions->Mode == EPropertyAnimatorCoreMode::Absolute
			&& (InOptions->IsResolvable() || InOptions->IsConverted()))
		{
			bRestore = true;
		}

		if (bRestore)
		{
			InOptions->Restore();
		}

		return true;
	}, bResolve);
}

void UPropertyAnimatorCoreBase::SaveProperties()
{
	constexpr bool bResolve = false;
	ForEachLinkedProperty<UPropertyAnimatorCoreContext>([](UPropertyAnimatorCoreContext* InOptions, const FPropertyAnimatorCoreData& InPropertyData)
	{
		InOptions->Save();
		return true;
	}, bResolve);
}

TArray<FString> UPropertyAnimatorCoreBase::GetTimeSourceNames() const
{
	TArray<FString> TimeSourceNames;

	if (const UPropertyAnimatorCoreSubsystem* ControllerSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		const TArray<FName> TimeSources = ControllerSubsystem->GetTimeSourceNames();

		Algo::Transform(TimeSources, TimeSourceNames, [](const FName& InTimeSource)
		{
			return InTimeSource.ToString();
		});
	}

	return TimeSourceNames;
}

UPropertyAnimatorCoreTimeSourceBase* UPropertyAnimatorCoreBase::FindOrAddTimeSource(FName InTimeSourceName)
{
	if (IsTemplate())
	{
		return nullptr;
	}

	UPropertyAnimatorCoreSubsystem* Subsystem = UPropertyAnimatorCoreSubsystem::Get();

	if (!Subsystem || InTimeSourceName.IsNone())
	{
		return nullptr;
	}

	// Check cached time source instances
	UPropertyAnimatorCoreTimeSourceBase* NewTimeSource = nullptr;
	if (TObjectPtr<UPropertyAnimatorCoreTimeSourceBase> const* TimeSourceInstance = TimeSourcesInstances.Find(InTimeSourceName))
	{
		if (const TObjectPtr<UPropertyAnimatorCoreTimeSourceBase> CachedTimeSource = *TimeSourceInstance)
		{
			NewTimeSource = CachedTimeSource.Get();
		}
	}

	// Create new time source instance and cache it
	if (!NewTimeSource)
	{
		NewTimeSource = Subsystem->CreateNewTimeSource(InTimeSourceName, this);
		if (NewTimeSource)
		{
			TimeSourcesInstances.Add(InTimeSourceName, NewTimeSource);
		}
	}

	return NewTimeSource;
}

void UPropertyAnimatorCoreBase::SetAnimatorDisplayName(FName InName)
{
	if (AnimatorDisplayName == InName)
	{
		return;
	}

	AnimatorDisplayName = InName;
	OnAnimatorDisplayNameChanged();

	OnAnimatorRenamedDelegate.Broadcast(this);
}

TSet<FPropertyAnimatorCoreData> UPropertyAnimatorCoreBase::GetLinkedProperties() const
{
	TSet<FPropertyAnimatorCoreData> LinkedPropertiesSet;

	for (const TObjectPtr<UPropertyAnimatorCoreContext>& Options : LinkedProperties)
	{
		if (Options)
		{
			LinkedPropertiesSet.Emplace(Options->GetAnimatedProperty());
		}
	}

	return LinkedPropertiesSet;
}

int32 UPropertyAnimatorCoreBase::GetLinkedPropertiesCount() const
{
	return LinkedProperties.Num();
}

bool UPropertyAnimatorCoreBase::LinkProperty(const FPropertyAnimatorCoreData& InLinkProperty)
{
	if (!InLinkProperty.IsResolved())
	{
		return false;
	}

	const UObject* Owner = InLinkProperty.GetOwner();
	const AActor* OwningActor = GetTypedOuter<AActor>();

	if (Owner != OwningActor && !Owner->IsIn(OwningActor))
	{
		return false;
	}

	if (!IsPropertySupported(InLinkProperty))
	{
		return false;
	}

	if (IsPropertyLinked(InLinkProperty))
	{
		return false;
	}

	const TSubclassOf<UPropertyAnimatorCoreContext> ContextSubclass = GetPropertyContextClass(InLinkProperty);
	const UClass* ContextClass = ContextSubclass.Get();

	if (!IsValid(ContextClass))
	{
		return false;
	}

	UPropertyAnimatorCoreContext* PropertyContext = NewObject<UPropertyAnimatorCoreContext>(this, ContextClass, NAME_None, RF_Transactional);
	PropertyContext->ConstructInternal(InLinkProperty);

	LinkedProperties.Add(PropertyContext);
	OnPropertyLinked(PropertyContext);

	UPropertyAnimatorCoreBase::OnAnimatorPropertyLinkedDelegate.Broadcast(this, InLinkProperty);

	return true;
}

bool UPropertyAnimatorCoreBase::UnlinkProperty(const FPropertyAnimatorCoreData& InUnlinkProperty)
{
	if (!InUnlinkProperty.IsResolved())
	{
		return false;
	}

	if (!IsPropertyLinked(InUnlinkProperty))
	{
		return false;
	}

	const UObject* Owner = InUnlinkProperty.GetOwner();
	const AActor* OwningActor = GetTypedOuter<AActor>();

	if (Owner != OwningActor && !Owner->IsIn(OwningActor))
	{
		return false;
	}

	UPropertyAnimatorCoreContext* PropertyContext = GetLinkedPropertyContext(InUnlinkProperty);

	PropertyContext->Restore();
	LinkedProperties.Remove(PropertyContext);
	OnPropertyUnlinked(PropertyContext);

	UPropertyAnimatorCoreBase::OnAnimatorPropertyUnlinkedDelegate.Broadcast(this, InUnlinkProperty);

	return true;
}

bool UPropertyAnimatorCoreBase::IsPropertyLinked(const FPropertyAnimatorCoreData& InPropertyData) const
{
	return LinkedProperties.ContainsByPredicate([&InPropertyData](const UPropertyAnimatorCoreContext* InOptions)
	{
		return InOptions && InOptions->GetAnimatedProperty() == InPropertyData;
	});
}

bool UPropertyAnimatorCoreBase::IsPropertiesLinked(const TSet<FPropertyAnimatorCoreData>& InProperties) const
{
	const TSet<FPropertyAnimatorCoreData> LinkedPropertiesSet = GetLinkedProperties();
	return LinkedPropertiesSet.Includes(InProperties);
}

TSet<FPropertyAnimatorCoreData> UPropertyAnimatorCoreBase::GetInnerPropertiesLinked(const FPropertyAnimatorCoreData& InPropertyData) const
{
	TSet<FPropertyAnimatorCoreData> OutProperties;

	if (!InPropertyData.IsResolved())
	{
		return OutProperties;
	}

	FProperty* LeafProperty = InPropertyData.GetLeafProperty();

	for (const FPropertyAnimatorCoreData& ControllerProperty : GetLinkedProperties())
	{
		const int32 LeafPropertyIdx = ControllerProperty.GetChainProperties().Find(LeafProperty);

		// If member property is inside array and not the last one, then this controlled property is inside the InPropertyData
		if (LeafPropertyIdx != INDEX_NONE)
		{
			OutProperties.Add(ControllerProperty);
		}
	}

	return OutProperties;
}
