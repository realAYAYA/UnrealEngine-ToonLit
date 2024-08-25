// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialComponent.h"
#include "DMComponentPath.h"
#include "DynamicMaterialModule.h"
#include "HAL/PlatformTime.h"
#include "Model/DynamicMaterialModel.h"

#define LOCTEXT_NAMESPACE "DMMaterialComponent"

double UDMMaterialComponent::MinCleanTime = FPlatformTime::Seconds();
const double UDMMaterialComponent::MinTimeBeforeClean = 0.2; // 5 fps

UDMMaterialComponent::UDMMaterialComponent()
#if WITH_EDITORONLY_DATA
	: ComponentState(EDMComponentLifetimeState::Created)
	, bComponentDirty(true)
#endif
{
}

UObject* UDMMaterialComponent::GetOuterSafe() const
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return nullptr;
	}

	UObject* Outer = GetOuter();

	if (!Outer || !Outer->IsValidLowLevelFast())
	{
		return nullptr;
	}

	return Outer;
}

bool UDMMaterialComponent::IsComponentValid() const
{
	const bool bValid = this != nullptr && IsValid(this)
		&& !HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);

	return bValid;
}

UDMMaterialComponent* UDMMaterialComponent::GetComponentByPath(const FString& InPath) const
{
	FDMComponentPath Path(InPath);
	return GetComponentByPath(Path);
}

UDMMaterialComponent* UDMMaterialComponent::GetComponentByPath(FDMComponentPath& InPath) const
{
	if (InPath.IsLeaf())
	{
		return const_cast<UDMMaterialComponent*>(this);
	}

	// Fetches the first component of the path and removes it from the path
	const FDMComponentPathSegment FirstComponent = InPath.GetFirstSegment();

	if (UDMMaterialComponent* SubComponent = GetSubComponentByPath(InPath, FirstComponent))
	{
		return SubComponent->GetComponentByPath(InPath);
	}

	return nullptr;
}

UDMMaterialComponent* UDMMaterialComponent::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	// We have no subobjects by default
	return nullptr;
}

void UDMMaterialComponent::Update(EDMUpdateType InUpdateType)
{
#if WITH_EDITOR
	OnUpdate.Broadcast(this, InUpdateType);
#endif
}

#if WITH_EDITOR
FString UDMMaterialComponent::GetComponentPath() const
{
	TArray<FString> ComponentPaths;
	GetComponentPathInternal(ComponentPaths);

	FString OutPath = "";

	for (const FString& ComponentPath : ComponentPaths)
	{
		if (OutPath.IsEmpty())
		{
			OutPath = ComponentPath;
		}
		else
		{
			OutPath = ComponentPath + TEXT(".") + OutPath;
		}
	}

	return OutPath;
}

FString UDMMaterialComponent::GetComponentPathComponent() const
{
	return GetName();
}

void UDMMaterialComponent::GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const
{
	OutChildComponentPathComponents.Add(GetComponentPathComponent());

	if (UDMMaterialComponent* ParentComponent = GetParentComponent())
	{
		ParentComponent->GetComponentPathInternal(OutChildComponentPathComponents);
	}
}

UDMMaterialComponent* UDMMaterialComponent::GetParentComponent() const
{
	return Cast<UDMMaterialComponent>(GetOuterSafe());
}

UDMMaterialComponent* UDMMaterialComponent::GetTypedParent(UClass* InParentClass, bool bInAllowSubclasses) const
{
	if (UDMMaterialComponent* Parent = GetParentComponent())
	{
		UClass* ParentClass = Parent->GetClass();

		if (ParentClass == InParentClass)
		{
			return Parent;
		}

		if (bInAllowSubclasses && ParentClass->IsChildOf(InParentClass))
		{
			return Parent;
		}

		return Parent->GetTypedParent(InParentClass, bInAllowSubclasses);
	}

	return nullptr;
}

FText UDMMaterialComponent::GetComponentDescription() const
{
	return GetClass()->GetDisplayNameText();
}

bool UDMMaterialComponent::CanClean()
{
	return (FPlatformTime::Seconds() >= MinCleanTime);
}

void UDMMaterialComponent::PreventClean(double DelayFor)
{
	MinCleanTime = FMath::Max(MinCleanTime, FPlatformTime::Seconds() + DelayFor);
}

bool UDMMaterialComponent::NeedsClean()
{
	if (!IsComponentValid())
	{
		return false;
	}

	return bComponentDirty;
}

void UDMMaterialComponent::DoClean()
{
	bComponentDirty = false;

	if (!IsComponentValid())
	{
		return;
	}

	static const double VeryShortTime = 0.0001;
	
	// Make sure we don't spam updates on a single tick.
	PreventClean(VeryShortTime);
}

void UDMMaterialComponent::SetComponentState(EDMComponentLifetimeState NewState)
{
	if (ComponentState == NewState)
	{
		return;
	}

	if (!IsComponentValid())
	{
		return;
	}

	ComponentState = NewState;
	OnComponentStateChange(NewState);
}

void UDMMaterialComponent::PostLoad()
{
	Super::PostLoad();

	SetFlags(RF_Transactional);

	ComponentState = EDMComponentLifetimeState::Added;
	MarkComponentDirty();
}

void UDMMaterialComponent::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	SetFlags(RF_Transactional);
	ComponentState = EDMComponentLifetimeState::Added;
	MarkComponentDirty();
}

bool UDMMaterialComponent::Modify(bool bInAlwaysMarkDirty /*= true*/)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	MarkComponentDirty();

	return bSaved;
}

void UDMMaterialComponent::OnComponentStateChange(EDMComponentLifetimeState NewState)
{
	if (!IsComponentValid())
	{
		return;
	}

	switch (NewState)
	{
		case EDMComponentLifetimeState::Added:
			OnComponentAdded();
			break;

		case EDMComponentLifetimeState::Removed:
			OnComponentRemoved();
			break;

		default:
			// Nothing
			break;
	}
}

void UDMMaterialComponent::OnComponentAdded()
{
	OnAdded.Broadcast(this, EDMComponentLifetimeState::Added);
}

void UDMMaterialComponent::OnComponentRemoved()
{
	OnAdded.Broadcast(this, EDMComponentLifetimeState::Removed);
}
#endif // WITH_EDITOR

void UDMMaterialComponent::MarkComponentDirty()
{
#if WITH_EDITORONLY_DATA
	bComponentDirty = true;
#endif

#if WITH_EDITOR
	PreventClean();
#endif
}

#undef LOCTEXT_NAMESPACE
