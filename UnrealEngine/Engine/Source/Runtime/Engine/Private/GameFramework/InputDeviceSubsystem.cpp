// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputDeviceProperties.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"			// For RegisterInputPreProcessor
#include "Misc/App.h"	// For FApp::GetDeltaTime()

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputDeviceSubsystem)

#if WITH_EDITOR
#include "Editor.h"		// For PIE delegates
#else
#include "Engine/Engine.h"
#endif

DEFINE_LOG_CATEGORY(LogInputDeviceProperties);

////////////////////////////////////////////////////////
// FInputDeviceSubsystemProcessor

/** An input processor for detecting changes to input devices based on the current FInputDeviceScope stack */
class FInputDeviceSubsystemProcessor : public IInputProcessor
{
	friend class UInputDeviceSubsystem;
	
	void UpdateLatestDevice(const FKey Key, const FInputDeviceId InDeviceId)
	{
#if WITH_EDITOR
		// If we're stopped at a breakpoint we need for this input preprocessor to just ignore all incoming input
		// because we're now doing stuff outside the game loop in the editor and it needs to not block all that.
		// This can happen if you suspend input while spawning a dialog and then hit another breakpoint and then
		// try and use the editor, you can suddenly be unable to do anything.
		if (GIntraFrameDebuggingGameThread)
		{
			return;
		}
#endif
		
		if (UInputDeviceSubsystem* SubSystem = UInputDeviceSubsystem::Get())
		{
			if (const FInputDeviceScope* Scope = FInputDeviceScope::GetCurrent())
			{
				// TODO: Refactor FInputDeviceScope to use FName's instead of a FString for HardwareDeviceIdentifier
				// Look up a hardware device ID from the config that matches this identifier
				const FName DeviceScopeName(*Scope->HardwareDeviceIdentifier);
				if (const FHardwareDeviceIdentifier* Hardware = UInputPlatformSettings::Get()->GetHardwareDeviceForClassName(DeviceScopeName))
				{
					SubSystem->SetMostRecentlyUsedHardwareDevice(InDeviceId, *Hardware);
				}
				// If there isn't one specified in the config file, we can just use the device name and scope name to make one.
				else
				{
					SubSystem->SetMostRecentlyUsedHardwareDevice(InDeviceId, { Scope->InputDeviceName, DeviceScopeName });
				}
			}
			// If there isn't a recent input device scope, then we can check if the key was from a keyboard and mouse
			else if (!Key.IsGamepadKey())
			{
				SubSystem->SetMostRecentlyUsedHardwareDevice(InDeviceId, FHardwareDeviceIdentifier::DefaultKeyboardAndMouse);
			}
		}
	}
	
public:

	// Required by IInputProcessor
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override { }
	
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InEvent) override
	{
		UpdateLatestDevice(InEvent.GetKey(), InEvent.GetInputDeviceId());
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InEvent) override
	{
		if (!FMath::IsNearlyZero(InEvent.GetAnalogValue(), UE_KINDA_SMALL_NUMBER))
		{
			UpdateLatestDevice(InEvent.GetKey(), InEvent.GetInputDeviceId());
		}
		
		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& InEvent) override
	{
		// We only want input events that have made changes
		if (!InEvent.GetCursorDelta().IsNearlyZero())
		{
			UpdateLatestDevice(InEvent.GetEffectingButton(), InEvent.GetInputDeviceId());	
		}
		
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& InEvent) override
	{
		UpdateLatestDevice(InEvent.GetEffectingButton(), InEvent.GetInputDeviceId());
		return false;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& InEvent) override
	{
		UpdateLatestDevice(InEvent.GetEffectingButton(), InEvent.GetInputDeviceId());
		return false;
	}
	
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InEvent, const FPointerEvent* InGestureEvent) override
	{
		UpdateLatestDevice(InEvent.GetEffectingButton(), InEvent.GetInputDeviceId());
		return false;
	}
};

////////////////////////////////////////////////////////
// FActiveDeviceProperty

FActiveDeviceProperty::FActiveDeviceProperty()
	: Property(nullptr)
	, EvaluatedDuration(0.0)
	, PlatformUser(PLATFORMUSERID_NONE)
	, DeviceId(INPUTDEVICEID_NONE)
	, PropertyHandle(FInputDevicePropertyHandle::InvalidHandle)
	, bLooping(false)
	, bIgnoreTimeDilation(false)
	, bPlayWhilePaused(false)
	, bHasBeenAppliedAtLeastOnce(false)
{ }

uint32 GetTypeHash(const FActiveDeviceProperty& InProp)
{
	return InProp.PropertyHandle.GetTypeHash();
}

bool operator==(const FActiveDeviceProperty& ActiveProp, const FInputDevicePropertyHandle& Handle)
{
	return ActiveProp.PropertyHandle == Handle;
}

bool operator!=(const FActiveDeviceProperty& ActiveProp, const FInputDevicePropertyHandle& Handle)
{
	return ActiveProp.PropertyHandle != Handle;
}

// The property handles are the only things that matter when comparing, they will always be unique.
bool FActiveDeviceProperty::operator==(const FActiveDeviceProperty& Other) const
{
	return PropertyHandle == Other.PropertyHandle;
}

bool FActiveDeviceProperty::operator!=(const FActiveDeviceProperty& Other) const
{
	return PropertyHandle != Other.PropertyHandle;
}

////////////////////////////////////////////////////////
// FActivateDevicePropertyParams

FActivateDevicePropertyParams::FActivateDevicePropertyParams()
	: UserId(FSlateApplicationBase::SlateAppPrimaryPlatformUser)
	// Set this Device Id to NONE by default. The subsystem will detect this being invalid and fall back 
	// to the default device for the given platform user. This will let the default behavior of the system be 
	// consistent, while still allowing for setting specific input devices if desired.
	, DeviceId(INPUTDEVICEID_NONE)
	, bLooping(false)
	, bIgnoreTimeDilation(false)
	, bPlayWhilePaused(false)
{
}

////////////////////////////////////////////////////////
// UInputDeviceSubsystem

UInputDeviceSubsystem* UInputDeviceSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UInputDeviceSubsystem>() : nullptr;
}

void UInputDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// We have to have a valid slate app to run this subsystem
	check(FSlateApplication::IsInitialized());
	
	InputPreprocessor = MakeShared<FInputDeviceSubsystemProcessor>();
	FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, 0);

#if WITH_EDITOR
	FEditorDelegates::PreBeginPIE.AddUObject(this, &UInputDeviceSubsystem::OnPrePIEStarted);
	FEditorDelegates::PausePIE.AddUObject(this, &UInputDeviceSubsystem::OnPIEPaused);
	FEditorDelegates::ResumePIE.AddUObject(this, &UInputDeviceSubsystem::OnPIEResumed);
	FEditorDelegates::EndPIE.AddUObject(this, &UInputDeviceSubsystem::OnPIEStopped);
#endif	// WITH_EDITOR
}

void UInputDeviceSubsystem::Deinitialize()
{
	Super::Deinitialize();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
	}
	InputPreprocessor.Reset();
}

bool UInputDeviceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// No slate app means we can't process any input
	if (!FSlateApplication::IsInitialized() ||
		// Commandlets and servers have no use for this subsystem
		IsRunningCommandlet() ||
		IsRunningDedicatedServer())
	{
		return false;
	}

	// There is a setting to turn off this subsystem entirely.
	const bool bShouldCreate = GetDefault<UInputSettings>()->bEnableInputDeviceSubsystem;
	if (!bShouldCreate)
	{
		UE_LOG(LogInputDeviceProperties, Log, TEXT("UInputSettings::bEnableInputDeviceSubsystem is false, the Input Device Subsystem will NOT be created!"));
	}
	
	return bShouldCreate && Super::ShouldCreateSubsystem(Outer);
}

UWorld* UInputDeviceSubsystem::GetTickableGameObjectWorld() const
{
	// Use the default world by default...
	UWorld* World = GetWorld();

	// ...but if we don't have one (i.e. we are in the editor and not PIE'ing)
	// then we need to get the editor world. This will let us preview
	// device properties without needing to actually PIE every time
	if (!World && GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* ThisWorld = Context.World();
			if (!ThisWorld)
			{
				continue;
			}
			// Prefer new PIE window worlds
			else if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = ThisWorld;
				break;
			}
			// Fallback to the editor world, which is still valid for previewing device properties
			else if (Context.WorldType == EWorldType::Editor)
			{
				World = ThisWorld;
			}
		}
	}

	return World;
}

ETickableTickType UInputDeviceSubsystem::GetTickableTickType() const
{
	return (IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional);
}

bool UInputDeviceSubsystem::IsAllowedToTick() const
{
	// Only tick when there are active device properties or ones we want to remove
	const bool bWantsTick = !ActiveProperties.IsEmpty() || !PropertiesPendingRemoval.IsEmpty();
#if WITH_EDITOR
	// If we are PIE'ing, then check if PIE is paused
	if (GEditor && (GEditor->bIsSimulatingInEditor || GEditor->PlayWorld))
	{
		return bIsPIEPlaying && bWantsTick;
	}	
#endif
	
	return bWantsTick;
}

bool UInputDeviceSubsystem::IsTickableInEditor() const
{
	// We want to tick in editor to allow previewing of device properties
	return true;
}

TStatId UInputDeviceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UInputDeviceSubsystem, STATGROUP_Tickables);
}

void UInputDeviceSubsystem::Tick(float InDeltaTime)
{
	// If a property doesn't want to be affected by time dilation then we can use this instead
	const double NonDialatedDeltaTime = FApp::GetDeltaTime();

	const UWorld* World = GetTickableGameObjectWorld();
	const bool bIsGamePaused = World ? World->IsPaused() : false;

	for (auto It = ActiveProperties.CreateIterator(); It; ++It)
	{
		FActiveDeviceProperty& ActiveProp = *It;
		if (!ActiveProp.Property.IsValid())
		{
			// The property may have been GC'd if the world has changed and it was an instanced property
			// on an actor in that world
			It.RemoveCurrent();
			PropertiesPendingRemoval.Remove(ActiveProp.PropertyHandle);
			continue;
		}
		
		if (PropertiesPendingRemoval.Contains(ActiveProp.PropertyHandle))
		{
			ActiveProp.Property->ResetDeviceProperty(ActiveProp.PlatformUser, ActiveProp.DeviceId);
			It.RemoveCurrent();
			PropertiesPendingRemoval.Remove(ActiveProp.PropertyHandle);
			continue;
		}

		// If the game is paused, only play effects that have explicitly specified that they should be
		// played while paused.
		if (bIsGamePaused && !ActiveProp.bPlayWhilePaused)
		{
			continue;
		}

		const double DeltaTime = ActiveProp.bIgnoreTimeDilation ? NonDialatedDeltaTime : static_cast<double>(InDeltaTime);

		// Increase the evaluated time of this property
		ActiveProp.EvaluatedDuration += DeltaTime;

		// If the property has run past it's duration, reset it and remove it from our active properties
		// Don't remove properties that are looping, because the user desires them to keep playing
		if (!ActiveProp.bLooping && ActiveProp.EvaluatedDuration > ActiveProp.Property->GetDuration() && ActiveProp.bHasBeenAppliedAtLeastOnce)
		{
			ActiveProp.Property->ResetDeviceProperty(ActiveProp.PlatformUser, ActiveProp.DeviceId);
			PropertiesPendingRemoval.Remove(ActiveProp.PropertyHandle);
			It.RemoveCurrent();
			continue;
		}
		// Otherwise, we can evaluate and apply it as normal
		else
		{
			ActiveProp.Property->EvaluateDeviceProperty(ActiveProp.PlatformUser, ActiveProp.DeviceId, DeltaTime, ActiveProp.EvaluatedDuration);
			ActiveProp.Property->ApplyDeviceProperty(ActiveProp.PlatformUser, ActiveProp.DeviceId);

			// Track that this property has now successfully been applied at least once
			ActiveProp.bHasBeenAppliedAtLeastOnce = true;
		}
	}

	// If there are still properties pending removal, then they have already been removed by the loop
	// above. Empty out the pending removal set to start fresh next frame.
	PropertiesPendingRemoval.Empty();
}

FInputDevicePropertyHandle UInputDeviceSubsystem::ActivateDeviceProperty(UInputDeviceProperty* Property, const FActivateDevicePropertyParams& Params)
{
	if (!Property)
	{
		UE_LOG(LogInputDeviceProperties, Error, TEXT("Invalid Property passed into ActivateDeviceProperty! Nothing will happen."));
		return FInputDevicePropertyHandle::InvalidHandle;
	}

	if (!Params.UserId.IsValid())
	{
		UE_LOG(LogInputDeviceProperties, Error, TEXT("Invalid Platform User Id '%d' given to ActivateDeviceProperty! Nothing will happen."), Params.UserId.GetInternalId());
		return FInputDevicePropertyHandle::InvalidHandle;
	}

	FInputDevicePropertyHandle OutHandle = FInputDevicePropertyHandle::AcquireValidHandle();

	if (ensureMsgf(OutHandle.IsValid(), TEXT("Unable to acquire a valid input device property handle! The input device property cannot be activated")))
	{
		FActiveDeviceProperty ActiveProp = {};
		ActiveProp.Property = Property;
		ActiveProp.PlatformUser = Params.UserId;

		// If a valid input device was not given, then set this property on the default device for the user.
		ActiveProp.DeviceId = 
			Params.DeviceId.IsValid() ? Params.DeviceId : IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(Params.UserId);		

		ActiveProp.PropertyHandle = OutHandle;
		ActiveProp.bLooping = Params.bLooping;
		ActiveProp.bIgnoreTimeDilation = Params.bIgnoreTimeDilation;
		ActiveProp.bPlayWhilePaused = Params.bPlayWhilePaused;

		// Keep track of this new active property. Placing it in this set will have it updated on this subsystem's Tick
		ActiveProperties.Add(ActiveProp);

#if WITH_EDITORONLY_DATA
		// If we are currently PIE'ing, then track this property if it's class isn't already known. This will
		// ensure that we reset device properties of unique types
		if (bIsPIEPlaying)
		{
			const bool bAlreadyHasResetData = 
				 PropertyClassesRequiringReset.ContainsByPredicate([&ActiveProp](const FResetPIEData& Data)
				 {
				 	return Data.DeviceId == ActiveProp.DeviceId && Data.UserId == ActiveProp.PlatformUser && Data.ClassToReset == ActiveProp.Property->GetClass();
				 });
			
			if (!bAlreadyHasResetData)
			{
			 	FResetPIEData ResetData = {};
             	ResetData.ClassToReset = ActiveProp.Property->GetClass();
             	ResetData.DeviceId = ActiveProp.DeviceId;
             	ResetData.UserId = ActiveProp.PlatformUser;
			 	PropertyClassesRequiringReset.Emplace(ResetData);	
			}
		}
#endif	// WITH_EDITOR
	}

	return OutHandle;
}

FInputDevicePropertyHandle UInputDeviceSubsystem::ActivateDevicePropertyOfClass(TSubclassOf<UInputDeviceProperty> PropertyClass, const FActivateDevicePropertyParams& Params)
{
	if (!PropertyClass)
	{
		UE_LOG(LogInputDeviceProperties, Error, TEXT("Invalid PropertyClass passed into ActivateDeviceProperty! Nothing will happen."));
		return FInputDevicePropertyHandle::InvalidHandle;
	}

	// Spawn a new instance of the given device property and activate it as normal
	return ActivateDeviceProperty(NewObject<UInputDeviceProperty>(/* Outer = */ this, /* Class */ PropertyClass), Params);
}

UInputDeviceProperty* UInputDeviceSubsystem::GetActiveDeviceProperty(const FInputDevicePropertyHandle Handle) const
{
	// Don't include any properties that are pending removal
	if (!PropertiesPendingRemoval.Contains(Handle))
	{
		// We can find the active property based on the handle's hash because FActiveDeviceProperty::GetTypeHash
		// just returns its FInputDevicePropertyHandle's GetTypeHash
		if (const FActiveDeviceProperty* ExistingProp = ActiveProperties.FindByHash(Handle.GetTypeHash(), Handle))
		{
			return ExistingProp->Property.Get();
		}
	}
	
	return nullptr;
}

bool UInputDeviceSubsystem::IsPropertyActive(const FInputDevicePropertyHandle Handle) const
{
	return ActiveProperties.ContainsByHash(Handle.GetTypeHash(), Handle) && !PropertiesPendingRemoval.Contains(Handle);
}

void UInputDeviceSubsystem::RemoveDevicePropertyByHandle(const FInputDevicePropertyHandle HandleToRemove)
{
	PropertiesPendingRemoval.Add(HandleToRemove);
}

void UInputDeviceSubsystem::RemoveDevicePropertyHandles(const TSet<FInputDevicePropertyHandle>& HandlesToRemove)
{
	if (!HandlesToRemove.IsEmpty())
	{
		PropertiesPendingRemoval.Append(HandlesToRemove);	
	}
	else
	{
		UE_LOG(LogInputDeviceProperties, Warning, TEXT("Provided an empty set of handles to remove. Nothing will happen."));
	}
}

void UInputDeviceSubsystem::RemoveAllDeviceProperties()
{
	for (FActiveDeviceProperty& ActiveProperty : ActiveProperties)
	{
		if (ActiveProperty.Property.IsValid())
		{
			ActiveProperty.Property->ResetDeviceProperty(ActiveProperty.PlatformUser, ActiveProperty.DeviceId);
		}		
	}
	
	ActiveProperties.Empty();
	PropertiesPendingRemoval.Empty();
}

FHardwareDeviceIdentifier UInputDeviceSubsystem::GetMostRecentlyUsedHardwareDevice(const FPlatformUserId InUserId) const
{
	if (const FHardwareDeviceIdentifier* FoundDevice = LatestUserDeviceIdentifiers.Find(InUserId))
	{
		return *FoundDevice;
	}
		
	return FHardwareDeviceIdentifier::Invalid;
}

FHardwareDeviceIdentifier UInputDeviceSubsystem::GetInputDeviceHardwareIdentifier(const FInputDeviceId InputDevice) const
{
	if (const FHardwareDeviceIdentifier* FoundDevice = LatestInputDeviceIdentifiers.Find(InputDevice))
	{
		return *FoundDevice;
	}

	return FHardwareDeviceIdentifier::Invalid;
}

void UInputDeviceSubsystem::SetMostRecentlyUsedHardwareDevice(const FInputDeviceId InDeviceId, const FHardwareDeviceIdentifier& InHardwareId)
{	
	const FPlatformUserId OwningUserId = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(InDeviceId);

	// If this hardware is the same as what the platform user already has, then there is no need to fire this event
	if (const FHardwareDeviceIdentifier* ExistingDevice = LatestUserDeviceIdentifiers.Find(OwningUserId))
	{
		if (InHardwareId == *ExistingDevice)
		{
			return;
		}
	}

	// Keep track of each input device's latest hardware id
	LatestInputDeviceIdentifiers.Add(InDeviceId, InHardwareId);

	// Keep a map to platform users so that we can easily get their most recent hardware
	LatestUserDeviceIdentifiers.Add(OwningUserId, InHardwareId);

	if (OnInputHardwareDeviceChanged.IsBound())
	{
		OnInputHardwareDeviceChanged.Broadcast(OwningUserId, InDeviceId);
	}
}

#if WITH_EDITOR
void UInputDeviceSubsystem::OnPrePIEStarted(bool bSimulating)
{
	// Remove all active properties, just in case someone was previewing something in the editor that are still going
	RemoveAllDeviceProperties();
#if WITH_EDITORONLY_DATA
	PropertyClassesRequiringReset.Reset();
#endif	// WITH_EDITORONLY_DATA
	bIsPIEPlaying = true;
}

void UInputDeviceSubsystem::OnPIEPaused(bool bSimulating)
{
	bIsPIEPlaying = false;
}

void UInputDeviceSubsystem::OnPIEResumed(bool bSimulating)
{
	bIsPIEPlaying = true;
}

void UInputDeviceSubsystem::OnPIEStopped(bool bSimulating)
{
	// Remove all active properties when PIE stops
	RemoveAllDeviceProperties();

#if WITH_EDITORONLY_DATA
	// Reset any Device Properties that have been added so that their input devices are in a clean state when PIE ends
	for (const FResetPIEData& ResetData : PropertyClassesRequiringReset)
	{
		UInputDeviceProperty* ToReset = NewObject<UInputDeviceProperty>(/* Outer = */ GetTransientPackage(), /* Class */ ResetData.ClassToReset);
		ToReset->ResetDeviceProperty(ResetData.UserId, ResetData.DeviceId, /* bForceReset= */ true);
	}
	
	PropertyClassesRequiringReset.Reset();
#endif	// WITH_EDITORONLY_DATA
	
	bIsPIEPlaying = false;
}
#endif	// WITH_EDITOR
