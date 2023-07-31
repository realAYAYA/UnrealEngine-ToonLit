// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamModifier.h"
#include "VCamComponent.h"
#include "VCamTypes.h"
#include "EnhancedInputComponent.h"
#include "Engine/InputDelegateBinding.h"


void UVCamBlueprintModifier::Initialize(UVCamModifierContext* Context, UInputComponent* InputComponent)
{
	// Forward the Initialize call to the Blueprint Event
	{
		FEditorScriptExecutionGuard ScriptGuard;

		OnInitialize(Context);
	}

	UVCamModifier::Initialize(Context, InputComponent);
}

void UVCamBlueprintModifier::Deinitialize()
{
	if (GetWorld()
		&& !HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)
		// Executing BP code during this phase is not allowed - you hit this when saving the world.
		&& !FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		FEditorScriptExecutionGuard ScriptGuard;

		OnDeinitialize();
	}
	
	Super::Deinitialize();
}

void UVCamBlueprintModifier::Apply(UVCamModifierContext* Context, UCineCameraComponent* CameraComponent, const float DeltaTime)
{
	// Forward the Apply call to the Blueprint Event
	{
		FEditorScriptExecutionGuard ScriptGuard;

		OnApply(Context, CameraComponent, DeltaTime);
	}
}

void UVCamModifier::Initialize(UVCamModifierContext* Context, UInputComponent* InputComponent /* = nullptr */)
{
	// Binds any dynamic input delegates to the provided input component
	if (IsValid(InputComponent))
	{
		UInputDelegateBinding::BindInputDelegates(GetClass(), InputComponent, this);
	}

	bRequiresInitialization = false;
}

void UVCamModifier::Deinitialize()
{
	UVCamComponent* VCamComponent = GetOwningVCamComponent();
	if (IsValid(VCamComponent))
	{
		VCamComponent->UnregisterObjectForInput(this);
	}

	bRequiresInitialization = true;
}

void UVCamModifier::PostLoad()
{
	Super::PostLoad();

	bRequiresInitialization = true;
}

UVCamComponent* UVCamModifier::GetOwningVCamComponent() const
{
	return GetTypedOuter<UVCamComponent>();
}

void UVCamModifier::GetCurrentLiveLinkDataFromOwningComponent(FLiveLinkCameraBlueprintData& LiveLinkData)
{
	if (UVCamComponent* OwningComponent = GetOwningVCamComponent())
	{
		OwningComponent->GetLiveLinkDataForCurrentFrame(LiveLinkData);
	}
}

void UVCamModifier::SetEnabled(bool bNewEnabled)
{
	if (FModifierStackEntry* StackEntry = GetCorrespondingStackEntry())
	{
		StackEntry->bEnabled = bNewEnabled;
	}

	if (!bNewEnabled)
	{
		Deinitialize();
	}
}

bool UVCamModifier::IsEnabled() const
{
	if (FModifierStackEntry* StackEntry = GetCorrespondingStackEntry())
	{
		return StackEntry->bEnabled;
	}

	return false;
}

bool UVCamModifier::SetStackEntryName(FName NewName)
{
	if (const UVCamComponent* ParentComponent = GetOwningVCamComponent())
	{
		const bool bNameAlreadyExists = ParentComponent->ModifierStack.ContainsByPredicate([NewName](const FModifierStackEntry& StackEntryToTest)
		{
			return StackEntryToTest.Name.IsEqual(NewName);
		});

		if (!bNameAlreadyExists)
		{
			if (FModifierStackEntry* StackEntry = GetCorrespondingStackEntry())
			{
				StackEntry->Name = NewName;
				return true;
			}
		}
	}
	return false;
}

FName UVCamModifier::GetStackEntryName() const
{
	if (FModifierStackEntry* StackEntry = GetCorrespondingStackEntry())
	{
		return StackEntry->Name;
	}
	return NAME_None;
}

FModifierStackEntry* UVCamModifier::GetCorrespondingStackEntry() const
{
	FModifierStackEntry* StackEntry = nullptr;

	if (UVCamComponent* ParentComponent = GetOwningVCamComponent())
	{
		StackEntry = ParentComponent->ModifierStack.FindByPredicate([this](const FModifierStackEntry& StackEntryToTest) 
		{
			return StackEntryToTest.GeneratedModifier == this;
		});
	}

	return StackEntry;
}

UWorld* UVCamModifier::GetWorld() const
{
	if (UVCamComponent* ParentComponent = GetOwningVCamComponent())
	{
		return ParentComponent->GetWorld();
	}
	return nullptr;
}