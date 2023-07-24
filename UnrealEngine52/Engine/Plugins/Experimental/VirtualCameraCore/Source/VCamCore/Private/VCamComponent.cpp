// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamComponent.h"

#include "Modifier/VCamModifier.h"
#include "Modifier/VCamModifierContext.h"
#include "Output/VCamOutputProviderBase.h"
#include "Util/LevelViewportUtils.h"
#include "VCamCoreCustomVersion.h"

#include "Algo/ForEach.h"
#include "CineCameraComponent.h"
#include "Engine/GameEngine.h"
#include "Engine/InputDelegateBinding.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystemInterface.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/InputSettings.h"
#include "ILiveLinkClient.h"
#include "InputMappingContext.h"
#include "Input/InputVCamSubsystem.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkTransformRole.h"
#include "VCamSubsystem.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"

#include "IConcertClient.h"
#include "IConcertSession.h"
#include "IConcertSessionHandler.h"
#include "IConcertSyncClient.h"
#include "IMultiUserClientModule.h"
#include "VCamMultiUser.h"

#include "VPSettings.h"
#include "VPRolesSubsystem.h"
#endif

DEFINE_LOG_CATEGORY(LogVCamComponent);

namespace UE::VCamCore::Private
{
	static bool CanInitVCamInstance(UVCamComponent* Component)
	{
		/*
		 * Other GWorld unrelated UWorld assets might get as part of complex editor operations.
		 * The actors in such worlds are usually not consciously being worked on by the user.
		 * If there is a VCam instance in such a world, it would register to Live Link and lock up the viewport every UVCamComponent::Update().
		 * A user would not expect this to happen and cannot resolve it either except by restarting the editor.
		 *
		 * Examples how this could happen "legitimately":
		 *  - If you delete the VCam after an auto save, the delete code will check for any referencers.
		 *	It will find the auto save package and load a VCam component.
		 *	This temporary instance should not register any delegates.
		 *	- Load a Level Snapshot containing VCam (without there being a VCam in the level)
		 *	- (User) code calls LoadPackage
		 */
		UWorld* OwnerWorld = Component->GetWorld();
		const bool bIsInEditedWorld =
			OwnerWorld // CDO's do not have an owner world
			&& (!GWorld // Can be nullptr during initial load
				|| GWorld == OwnerWorld
				|| GWorld->ContainsLevel(OwnerWorld->PersistentLevel)
				);
		
		/*
		 * For temporary objects, the InputComponent should not be created and neither should we subscribe to global callbacks
		 *	1. The Blueprint editor has two objects:
		 *		1.1 The "real" one which saves the property data - this one is RF_ArchetypeObject
		 *		1.2 The preview one (which I assume is displayed in the viewport) - this one is RF_Transient.
		 *	2. When you drag-create an actor, level editor creates a RF_Transient template actor. After you release the mouse, a real one is created (not RF_Transient).
		 */
		return !Component->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)
			&& !GIsCookerLoadingPackage
			&& bIsInEditedWorld;
	}
}

UVCamComponent::UVCamComponent()
{
	UClass* InputComponentClass = UInputSettings::GetDefaultInputComponentClass();
	constexpr bool bIsRequired = true;
	constexpr bool bIsTransient = true;
	InputComponent = Cast<UInputComponent>(CreateDefaultSubobject(TEXT("VCamInput0"), InputComponentClass, InputComponentClass, bIsRequired, bIsTransient));
	
	EnsureDelegatesRegistered();
}

void UVCamComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Deinitialize();

	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient.OnLiveLinkTicked().RemoveAll(this);
	}

#if WITH_EDITOR
	// Remove all event listeners
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		LevelEditorModule->OnMapChanged().RemoveAll(this);
	}

	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);

	MultiUserShutdown();
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
#endif
}

void UVCamComponent::HandleObjectReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	for (const TPair<UObject*, UObject*>& ReplacementPair : ReplacementMap)
	{
		UObject* FromObject = ReplacementPair.Key;
		UObject* ToObject = ReplacementPair.Value;

		if (ToObject == this)
		{
			if (UVCamComponent* OldComponent = Cast<UVCamComponent>(FromObject))
			{
				OldComponent->NotifyComponentWasReplaced(this);
			}

			OnComponentReplaced.Broadcast(this);
		}
	}
}


void UVCamComponent::NotifyComponentWasReplaced(UVCamComponent* ReplacementComponent)
{
	check(ReplacementComponent);

	// Make sure to copy over our delegate bindings to the component replacing us
	ReplacementComponent->OnComponentReplaced = OnComponentReplaced;
	OnComponentReplaced.Clear();
	
	const bool bWasEnabled = bEnabled;
	// Ensure all modifiers and output providers get deinitialized
	if (bEnabled)
	{
		SetEnabled(false);
	}
	
	// Refresh the enabled state on the new component to prevent any stale state from the replacement but only do so once.
	// Careful: NotifyComponentWasReplaced can be called multiple times with the same arguments.
	// If this is not the first time NotifyComponentWasReplaced is called, then bWasEnabled will already be false (see above).
	const bool bNeedsToStartupOutputProviders = bWasEnabled && ReplacementComponent->IsEnabled();
	if (bNeedsToStartupOutputProviders)
	{
		ReplacementComponent->ViewportLocker.Reset();
		ReplacementComponent->SetEnabled(false);
		ReplacementComponent->SetEnabled(true);
	}

	// There's a current issue where FKeys will be nulled when the component reconstructs so we'll explicitly
	// pass the Input Profile to the new component to avoid this
	ReplacementComponent->InputProfile = InputProfile;
	ReplacementComponent->ApplyInputProfile();
	
	DestroyComponent();
}

void UVCamComponent::RegisterInputComponent()
{
	// Ensure we start from a clean slate
	UnregisterInputComponent();
	
	if (UInputVCamSubsystem* InputSubsystem = GetInputVCamSubsystem())
	{
		InputSubsystem->PushInputComponent(InputComponent);
	}
}

void UVCamComponent::UnregisterInputComponent()
{
	if (UInputVCamSubsystem* InputSubsystem = GetInputVCamSubsystem()
		// InputComponent is nulled by GC when exiting the engine
		; InputComponent && InputSubsystem)
	{
		// Note: Despite the functions being called "Pop" it's actually just removing our specific input component from
		// the stack rather than blindly popping the top component
		InputSubsystem->PopInputComponent(InputComponent);
	}
	
	AppliedInputContexts.Reset();
}

bool UVCamComponent::CanUpdate() const
{
	const bool bShouldUpdate = bEnabled && !bIsEditorObjectButPIEIsRunning;
	
	// This prevents us updating in asset editors or invalid worlds
	constexpr int ValidWorldTypes = EWorldType::Game | EWorldType::PIE | EWorldType::Editor;
	UWorld* World = GetWorld();
	const bool bIsSupportedWorld = World && (World->WorldType & ValidWorldTypes) != EWorldType::None;

	// Modifiers requires a cine camera component
	const bool bHasValidComponent = GetTargetCamera() != nullptr;
	
	return bShouldUpdate && bIsSupportedWorld && bHasValidComponent;

}

void UVCamComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();

	// Attachment change event was a detach. We only want to respond to attaches 
	if (GetAttachParent() == nullptr)
	{
		return;
	}

	UCineCameraComponent* TargetCamera = GetTargetCamera();

	// This flag must be false on the attached CameraComponent or the UMG will not render correctly if the aspect ratios are mismatched
	if (TargetCamera)
	{
		TargetCamera->bConstrainAspectRatio = false;
	}

	for (UVCamOutputProviderBase* Provider : OutputProviders)
	{
		if (Provider)
		{
			Provider->SetTargetCamera(TargetCamera);
		}
	}

#if WITH_EDITOR
	CheckForErrors();
#endif
}

void UVCamComponent::Serialize(FArchive& Ar)
{
	using namespace UE::VCamCore;
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FVCamCoreCustomVersion::GUID);
	
	if (Ar.IsLoading() && Ar.CustomVer(FVCamCoreCustomVersion::GUID) < FVCamCoreCustomVersion::MoveTargetViewportFromComponentToOutput)
	{
		Algo::ForEach(ViewportLocker.Locks, [this](TPair<EVCamTargetViewportID, FVCamViewportLockState>& Pair){ Pair.Value.bLockViewportToCamera = bLockViewportToCamera_DEPRECATED; });
	}
}

void UVCamComponent::PostLoad()
{
	Super::PostLoad();

	EnsureDelegatesRegistered();
	// Ensure the input profile is applied when this component is loaded
	ApplyInputProfile();
}

void UVCamComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UVCamComponent* This = CastChecked<UVCamComponent>(InThis);
	This->SubsystemCollection.AddReferencedObjects(This, Collector);
	Super::AddReferencedObjects(InThis, Collector);
}

#if WITH_EDITOR

void UVCamComponent::CheckForErrors()
{
	Super::CheckForErrors();

	if (!HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed))
	{
		if (!GetTargetCamera())
		{
			UE_LOG(LogVCamComponent, Error, TEXT("Attached Parent should be a CineCamera derived component."));
		}
	}
}

void UVCamComponent::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);
}

void UVCamComponent::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	FProperty* MemberProperty = PropertyAboutToChange.GetActiveMemberNode()->GetValue();

	// Copy the property that is going to be changed so we can use it in PostEditChange if needed (for ArrayClear, ArrayRemove, etc.)
	if (MemberProperty)
	{
		static FName NAME_OutputProviders = GET_MEMBER_NAME_CHECKED(UVCamComponent, OutputProviders);
		static FName NAME_ModifierStack = GET_MEMBER_NAME_CHECKED(UVCamComponent, ModifierStack);
		static FName NAME_Enabled = GET_MEMBER_NAME_CHECKED(UVCamComponent, bEnabled);

		const FName MemberPropertyName = MemberProperty->GetFName();

		if (MemberPropertyName == NAME_OutputProviders)
		{
			SavedOutputProviders.Empty();
			SavedOutputProviders = OutputProviders;
		}
		else if (MemberPropertyName == NAME_ModifierStack)
		{
			SavedModifierStack = ModifierStack;
		}
		else if (MemberPropertyName == NAME_Enabled)
		{
			// Changing the enabled state needs to be done here instead of PostEditChange
			// So we need to grab the value from the FProperty directly before using it
			void* PropertyData = MemberProperty->ContainerPtrToValuePtr<void>(this);
			bool bWasEnabled = false;
			MemberProperty->CopySingleValue(&bWasEnabled, PropertyData);
			
			SetEnabled(!bWasEnabled);
		}
	}
	
	UObject::PreEditChange(PropertyAboutToChange);
}

void UVCamComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;
	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();
		
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UVCamComponent, bEnabled))
		{
			// Only act here if we are a struct (like FModifierStackEntry)
			if (!Property->GetOwner<UClass>())
			{
				SetEnabled(bEnabled);
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UVCamComponent, ModifierStack))
		{
			ValidateModifierStack();
			SavedModifierStack.Empty();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UVCamComponent, InputProfile))
		{
			ApplyInputProfile();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UVCamComponent, InputDeviceSettings))
		{
			SetInputDeviceSettings(InputDeviceSettings);
		}

		for (const UVCamOutputProviderBase* OutputProvider : OutputProviders)
		{
			if (IsValid(OutputProvider))
			{
				OutputProvider->NotifyWidgetOfComponentChange();
			}
		}
	}

	// Called e.g. after PostEditUndo. Must make sure that the delegates are registered.
	EnsureDelegatesRegistered();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UVCamComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.PropertyChain.GetActiveNode()->GetValue();
	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_OutputProviders = GET_MEMBER_NAME_CHECKED(UVCamComponent, OutputProviders);
		static FName NAME_TargetViewport = UVCamOutputProviderBase::GetTargetViewportPropertyName();

		if (Property->GetFName() == NAME_OutputProviders)
		{
			FProperty* ActualProperty = PropertyChangedEvent.PropertyChain.GetActiveNode()->GetNextNode()
				? PropertyChangedEvent.PropertyChain.GetActiveNode()->GetNextNode()->GetValue()
				: nullptr;
			if (ActualProperty == nullptr)
			{
				OnOutputProvidersEdited(PropertyChangedEvent);
			}
			else if (ActualProperty->GetFName() == NAME_TargetViewport)
			{
				OnTargetViewportEdited();
			}

			// We created this in PreEditChange, so we need to always get rid of it
			SavedOutputProviders.Empty();
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UVCamComponent::OnOutputProvidersEdited(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const int32 ChangedIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.GetPropertyName().ToString());
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		if (OutputProviders.IsValidIndex(ChangedIndex))
		{
			UVCamOutputProviderBase* ChangedProvider = OutputProviders[ChangedIndex];

			// If we changed the output type, be sure to delete the old one before setting up the new one
			if (SavedOutputProviders.IsValidIndex(ChangedIndex) && (SavedOutputProviders[ChangedIndex] != ChangedProvider))
			{
				DestroyOutputProvider(SavedOutputProviders[ChangedIndex]);
			}

			// We only Initialize a provider if they're able to be updated
			// If they later become able to be updated then they will be
			// Initialized inside the Update() loop
			if (ChangedProvider && ShouldUpdateOutputProviders())
			{
				ChangedProvider->SetTargetCamera(GetTargetCamera());
				ChangedProvider->Initialize();
			}
		}
	}
	else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
	{
		if (SavedOutputProviders.IsValidIndex(ChangedIndex))
		{
			DestroyOutputProvider(SavedOutputProviders[ChangedIndex]);
		}
	}
	else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
	{
		for (UVCamOutputProviderBase* ClearedProvider : SavedOutputProviders)
		{
			DestroyOutputProvider(ClearedProvider);
		}
	}
}

void UVCamComponent::OnTargetViewportEdited()
{
	if (bEnabled)
	{
		SetEnabled(false);
		SetEnabled(true);
		UpdateActorViewportLocks();
	}
}

#endif // WITH_EDITOR

void UVCamComponent::AddInputMappingContext(const UVCamModifier* Modifier)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		const int32 InputPriority = Modifier->InputContextPriority;
		const UInputMappingContext* IMC = Modifier->InputMappingContext;
		if (IsValid(IMC))
		{
			if (!EnhancedInputSubsystemInterface->HasMappingContext(IMC))
			{
				EnhancedInputSubsystemInterface->AddMappingContext(IMC, InputPriority);
			}
			// Ensure we store the IMC even if it's already in the system as it could have been registered from outside the VCam 
			AppliedInputContexts.AddUnique(IMC);
		}
	}
}

void UVCamComponent::RemoveInputMappingContext(const UVCamModifier* Modifier)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		const UInputMappingContext* IMC = Modifier->InputMappingContext;
		if (IsValid(IMC))
		{
			EnhancedInputSubsystemInterface->RemoveMappingContext(IMC);
			AppliedInputContexts.Remove(IMC);
		}
	}
}

void UVCamComponent::AddInputMappingContext(UInputMappingContext* Context, int32 Priority)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		if (IsValid(Context))
		{
			EnhancedInputSubsystemInterface->AddMappingContext(Context, Priority);
			AppliedInputContexts.AddUnique(Context);
		}
	}
}

void UVCamComponent::RemoveInputMappingContext(UInputMappingContext* Context)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		if (IsValid(Context))
		{
			EnhancedInputSubsystemInterface->RemoveMappingContext(Context);
			AppliedInputContexts.Remove(Context);
		}
	}
}

bool UVCamComponent::SetInputProfileFromName(const FName ProfileName)
{
	if (const UVCamInputSettings* VCamInputSettings = GetDefault<UVCamInputSettings>())
	{
		if (const FVCamInputProfile* NewInputProfile = VCamInputSettings->InputProfiles.Find(ProfileName))
		{
			InputProfile = *NewInputProfile;
			ApplyInputProfile();
		}
	}
	return false;
}

bool UVCamComponent::AddInputProfileWithCurrentlyActiveMappings(const FName ProfileName, bool bUpdateIfProfileAlreadyExists)
{
	UVCamInputSettings* VCamInputSettings = GetMutableDefault<UVCamInputSettings>();

	// If we don't have a valid settings object then early out
	if (!VCamInputSettings)
	{
		return false;
	}
	
	FVCamInputProfile* TargetInputProfile = VCamInputSettings->InputProfiles.Find(ProfileName);

	// An Input Profile with this name already exists
	if (TargetInputProfile)
	{
		if (!bUpdateIfProfileAlreadyExists)
		{
			return false;
		}
	}
	else
	{
		TargetInputProfile = &VCamInputSettings->InputProfiles.Add(ProfileName);
	}

	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		TArray<FEnhancedActionKeyMapping> PlayerMappableActionKeyMappings;
		for (const UInputMappingContext* MappingContext : AppliedInputContexts)
		{
			if (!IsValid(MappingContext))
			{
				continue;
			}
			for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
			{
				if (Mapping.IsPlayerMappable())
				{
					const FName MappingName = Mapping.GetMappingName();

					// Prefer to use the current mapped key but fallback to the default if no key is mapped
					FKey CurrentKey = EnhancedInputSubsystemInterface->GetPlayerMappedKeyInSlot(MappingName);
					if (!CurrentKey.IsValid())
					{
						CurrentKey = Mapping.Key;
					}

					if (!TargetInputProfile->MappableKeyOverrides.Contains(MappingName))
					{
						TargetInputProfile->MappableKeyOverrides.Add(MappingName, CurrentKey);
					}
				}
			}
		}
	}
	
	VCamInputSettings->SaveConfig();
	return true;
}

bool UVCamComponent::SaveCurrentInputProfileToSettings(const FName ProfileName) const
{
	UVCamInputSettings* VCamInputSettings = GetMutableDefault<UVCamInputSettings>();

	// If we don't have a valid settings object then early out
	if (!VCamInputSettings)
	{
		return false;
	}

	FVCamInputProfile& SettingsInputProfile = VCamInputSettings->InputProfiles.FindOrAdd(ProfileName);
	SettingsInputProfile = InputProfile;
	VCamInputSettings->SaveConfig();

	return true;
}

TArray<FEnhancedActionKeyMapping> UVCamComponent::GetAllPlayerMappableActionKeyMappings() const
{
	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		return EnhancedInputSubsystemInterface->GetAllPlayerMappableActionKeyMappings();
	}
	return TArray<FEnhancedActionKeyMapping>();
}

FKey UVCamComponent::GetPlayerMappedKey(const FName MappingName) const
{
	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		return EnhancedInputSubsystemInterface->GetPlayerMappedKeyInSlot(MappingName);
	}
	return EKeys::Invalid;
}

void UVCamComponent::Update()
{
	if (!CanUpdate())
	{
		return;
	}

	// If requested then disable the component if we're spawned by sequencer
	if (bDisableComponentWhenSpawnedBySequencer)
	{
		static const FName SequencerActorTag(TEXT("SequencerActor"));
		AActor* OwningActor = GetOwner();
		if (OwningActor && OwningActor->ActorHasTag(SequencerActorTag))
		{
			UE_LOG(LogVCamComponent, Warning, TEXT("%s was spawned by Sequencer. Disabling the component because \"Disable Component When Spawned By Sequencer\" was true."), *GetFullName(OwningActor->GetOuter()));
			SetEnabled(false);
			return;
		}
	}

	EnsureInitialized();
#if WITH_EDITOR
	// Somebody may have externally changed subsystem from out under us (user code). Let's make sure the details panel updates.
	SyncInputSettings();
#endif
	
	const float DeltaTime = GetDeltaTime();
	if (ShouldEvaluateModifierStack())
	{
		// Ensure the actor lock reflects the state of the lock property
		// This is needed as UActorComponent::ConsolidatedPostEditChange will cause the component to be reconstructed on PostEditChange if the component is inherited
		UpdateActorViewportLocks();
		
		FLiveLinkCameraBlueprintData InitialLiveLinkData;
		GetLiveLinkDataForCurrentFrame(InitialLiveLinkData);
		CopyLiveLinkDataToCamera(InitialLiveLinkData, GetTargetCamera());

		TickModifierStack(DeltaTime);
		SendCameraDataViaMultiUser();
	}

	if (ShouldUpdateOutputProviders())
	{
		TickOutputProviders(DeltaTime);
	}
	
	TickSubsystems(DeltaTime);
}

void UVCamComponent::SetEnabled(bool bNewEnabled)
{
	// Disable all outputs and modifiers if we're no longer enabled
	// NOTE this must be done BEFORE setting the actual bEnabled variable because OutputProviderBase now checks the component enabled state
	if (!bNewEnabled)
	{
		Deinitialize();
	}

	bEnabled = bNewEnabled;

	// Enable any outputs that are set to active
	// NOTE this must be done AFTER setting the actual bEnabled variable because OutputProviderBase now checks the component enabled state
	if (bNewEnabled)
	{
		Initialize();
	}
}

UCineCameraComponent* UVCamComponent::GetTargetCamera() const
{
	return Cast<UCineCameraComponent>(GetAttachParent());
}

bool UVCamComponent::AddModifier(const FName Name, const TSubclassOf<UVCamModifier> ModifierClass, UVCamModifier*& CreatedModifier)
{
	CreatedModifier = nullptr;

	if (GetModifierByName(Name))
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Unable to add Modifier to Stack as another Modifier with the name \"%s\" exists"), *Name.ToString());
		return false;
	}

	ModifierStack.Emplace(Name, ModifierClass, this);
	FModifierStackEntry& NewModifierEntry = ModifierStack.Last();
	CreatedModifier = NewModifierEntry.GeneratedModifier;

	return CreatedModifier != nullptr;
}

bool UVCamComponent::InsertModifier(const FName Name, int32 Index, const TSubclassOf<UVCamModifier> ModifierClass, UVCamModifier*& CreatedModifier)
{
	CreatedModifier = nullptr;

	if (GetModifierByName(Name))
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Unable to add Modifier to Stack as another Modifier with the name \"%s\" exists"), *Name.ToString());
		return false;
	}

	if (Index < 0 || Index > ModifierStack.Num())
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Insert Modifier failed with invalid index %d for stack of size %d."), Index, ModifierStack.Num());
		return false;
	}
	
	ModifierStack.EmplaceAt(Index, Name, ModifierClass, this);
	FModifierStackEntry& NewModifierEntry = ModifierStack[Index];
	CreatedModifier = NewModifierEntry.GeneratedModifier;

	return CreatedModifier != nullptr;
}

bool UVCamComponent::SetModifierIndex(int32 OriginalIndex, int32 NewIndex)
{
	if (!ModifierStack.IsValidIndex(OriginalIndex))
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Set Modifier Index failed as the Original Index, %d, was out of range for stack of size %d"), OriginalIndex, ModifierStack.Num());
		return false;
	}

	if (!ModifierStack.IsValidIndex(NewIndex))
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Set Modifier Index failed as the New Index, %d, was out of range for stack of size %d"), NewIndex, ModifierStack.Num());
		return false;
	}

	FModifierStackEntry StackEntry = ModifierStack[OriginalIndex];
	ModifierStack.RemoveAtSwap(OriginalIndex);
	ModifierStack.Insert(StackEntry, NewIndex);

	return true;
}

void UVCamComponent::RemoveAllModifiers()
{
	ModifierStack.Empty();
}

bool UVCamComponent::RemoveModifier(const UVCamModifier* Modifier)
{
	const int32 RemovedCount = ModifierStack.RemoveAll([Modifier](const FModifierStackEntry& StackEntry)
		{
			return StackEntry.GeneratedModifier && StackEntry.GeneratedModifier == Modifier;
		});

	return RemovedCount > 0;
}

bool UVCamComponent::RemoveModifierByIndex(const int ModifierIndex)
{
	if (ModifierStack.IsValidIndex(ModifierIndex))
	{
		ModifierStack.RemoveAt(ModifierIndex);
		return true;
	}
	return false;
}

bool UVCamComponent::RemoveModifierByName(const FName Name)
{
	const int32 RemovedCount = ModifierStack.RemoveAll([Name](const FModifierStackEntry& StackEntry)
		{
			return StackEntry.Name.IsEqual(Name);
		});

	return RemovedCount > 0;
}

int32 UVCamComponent::GetNumberOfModifiers() const
{
	return ModifierStack.Num();
}

void UVCamComponent::GetAllModifiers(TArray<UVCamModifier*>& Modifiers) const
{
	Modifiers.Empty();

	for (const FModifierStackEntry& StackEntry : ModifierStack)
	{
		Modifiers.Add(StackEntry.GeneratedModifier);
	}
}

TArray<FName> UVCamComponent::GetAllModifierNames() const
{
	TArray<FName> ModifierNames;
	Algo::Transform(ModifierStack, ModifierNames, [](const FModifierStackEntry& StackEntry){ return StackEntry.Name; });
	return ModifierNames;
}

UVCamModifier* UVCamComponent::GetModifierByIndex(const int32 Index) const
{
	if (ModifierStack.IsValidIndex(Index))
	{
		return ModifierStack[Index].GeneratedModifier;
	}

	return nullptr;
}

UVCamModifier* UVCamComponent::GetModifierByName(const FName Name) const
{
	const FModifierStackEntry* StackEntry = ModifierStack.FindByPredicate([Name](const FModifierStackEntry& StackEntry)
	{
		return StackEntry.Name.IsEqual(Name);
	});

	if (StackEntry)
	{
		return StackEntry->GeneratedModifier;
	}
	return nullptr;
}

void UVCamComponent::GetModifiersByClass(TSubclassOf<UVCamModifier> ModifierClass,
	TArray<UVCamModifier*>& FoundModifiers) const
{
	FoundModifiers.Empty();

	for (const FModifierStackEntry& StackEntry : ModifierStack)
	{
		if (StackEntry.GeneratedModifier && StackEntry.GeneratedModifier->IsA(ModifierClass))
		{
			FoundModifiers.Add(StackEntry.GeneratedModifier);
		}
	}
}

void UVCamComponent::GetModifiersByInterface(TSubclassOf<UInterface> InterfaceClass, TArray<UVCamModifier*>& FoundModifiers) const
{
	FoundModifiers.Empty();

	for (const FModifierStackEntry& StackEntry : ModifierStack)
	{
		if (StackEntry.GeneratedModifier && StackEntry.GeneratedModifier->GetClass()->ImplementsInterface(InterfaceClass))
		{
			FoundModifiers.Add(StackEntry.GeneratedModifier);
		}
	}
}

void UVCamComponent::SetModifierContextClass(TSubclassOf<UVCamModifierContext> ContextClass, UVCamModifierContext*& CreatedContext)
{
	if (ContextClass)
	{
		if (ContextClass != ModifierContext->StaticClass())
		{
			// Only reinstance if it's a new class
			ModifierContext = NewObject<UVCamModifierContext>(this, ContextClass.Get());
		}
	}
	else
	{
		// If the context class is invalid then clear the modifier context
		ModifierContext = nullptr;
	}

	CreatedContext = ModifierContext;
}

UVCamModifierContext* UVCamComponent::GetModifierContext() const
{
	return ModifierContext;
}

bool UVCamComponent::AddOutputProvider(TSubclassOf<UVCamOutputProviderBase> ProviderClass, UVCamOutputProviderBase*& CreatedProvider)
{
	CreatedProvider = nullptr;

	if (ProviderClass)
	{
		int NewItemIndex = OutputProviders.Emplace(NewObject<UVCamOutputProviderBase>(this, ProviderClass.Get()));
		CreatedProvider = OutputProviders[NewItemIndex];
	}

	return CreatedProvider != nullptr;
}

bool UVCamComponent::InsertOutputProvider(int32 Index, TSubclassOf<UVCamOutputProviderBase> ProviderClass, UVCamOutputProviderBase*& CreatedProvider)
{
	CreatedProvider = nullptr;

	if (Index < 0 || Index > OutputProviders.Num())
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Insert Output Provider failed with invalid index %d for stack of size %d."), Index, OutputProviders.Num());
		return false;
	}

	if (ProviderClass)
	{
		OutputProviders.EmplaceAt(Index, NewObject<UVCamOutputProviderBase>(this, ProviderClass.Get()));
		CreatedProvider = OutputProviders[Index];
	}

	return CreatedProvider != nullptr;
}

bool UVCamComponent::SetOutputProviderIndex(int32 OriginalIndex, int32 NewIndex)
{
	if (!OutputProviders.IsValidIndex(OriginalIndex))
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Set Output Provider Index failed as the Original Index, %d, was out of range for stack of size %d"), OriginalIndex, OutputProviders.Num());
		return false;
	}

	if (!OutputProviders.IsValidIndex(NewIndex))
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("Set Output Provider Index failed as the New Index, %d, was out of range for stack of size %d"), NewIndex, OutputProviders.Num());
		return false;
	}

	UVCamOutputProviderBase* Provider = OutputProviders[OriginalIndex];
	OutputProviders.RemoveAtSwap(OriginalIndex);
	OutputProviders.Insert(Provider, NewIndex);

	return true;
}

void UVCamComponent::RemoveAllOutputProviders()
{
	OutputProviders.Empty();
}

bool UVCamComponent::RemoveOutputProvider(const UVCamOutputProviderBase* Provider)
{
	int32 NumRemoved = OutputProviders.RemoveAll([Provider](const UVCamOutputProviderBase* ProviderInArray) { return ProviderInArray == Provider; });
	return NumRemoved > 0;
}

bool UVCamComponent::RemoveOutputProviderByIndex(const int32 ProviderIndex)
{
	if (OutputProviders.IsValidIndex(ProviderIndex))
	{
		OutputProviders.RemoveAt(ProviderIndex);
		return true;
	}
	return false;
}

int32 UVCamComponent::GetNumberOfOutputProviders() const
{
	return OutputProviders.Num();
}

void UVCamComponent::GetAllOutputProviders(TArray<UVCamOutputProviderBase*>& Providers) const
{
	Providers = OutputProviders;
}

UVCamOutputProviderBase* UVCamComponent::GetOutputProviderByIndex(const int32 ProviderIndex) const
{
	if (OutputProviders.IsValidIndex(ProviderIndex))
	{
		return OutputProviders[ProviderIndex];
	}
	return nullptr;
}

void UVCamComponent::GetOutputProvidersByClass(TSubclassOf<UVCamOutputProviderBase> ProviderClass, TArray<UVCamOutputProviderBase*>& FoundProviders) const
{
	FoundProviders.Empty();

	if (ProviderClass)
	{
		FoundProviders = OutputProviders.FilterByPredicate([ProviderClass](const UVCamOutputProviderBase* ProviderInArray) { return ProviderInArray->IsA(ProviderClass); });
	}
}

void UVCamComponent::SetInputProfile(const FVCamInputProfile& NewInputProfile)
{
	InputProfile = NewInputProfile;
	ApplyInputProfile();
}

const FVCamInputDeviceConfig& UVCamComponent::GetInputDeviceSettings() const
{
	// This input system exists while this component is enabled.
	// User code may call SetInputSettings directly on the subsystem - we want to capture that case. 
	if (UInputVCamSubsystem* InputSubsystem = GetInputVCamSubsystem())
	{
		return InputSubsystem->GetInputSettings();
	}
	
	return InputDeviceSettings;
}

void UVCamComponent::SetInputDeviceSettings(const FVCamInputDeviceConfig& NewInputProfile)
{
	InputDeviceSettings = NewInputProfile;
	if (UInputVCamSubsystem* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		EnhancedInputSubsystemInterface->SetInputSettings(InputDeviceSettings);
	}
}

TArray<UVCamSubsystem*> UVCamComponent::GetSubsystemArray(const TSubclassOf<UVCamSubsystem>& Class) const
{
	return SubsystemCollection.GetSubsystemArray<UVCamSubsystem>(Class);
}

UInputVCamSubsystem* UVCamComponent::GetInputVCamSubsystem() const
{
	return SubsystemCollection.GetSubsystem<UInputVCamSubsystem>(UInputVCamSubsystem::StaticClass());
}

void UVCamComponent::GetLiveLinkDataForCurrentFrame(FLiveLinkCameraBlueprintData& LiveLinkData)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		FLiveLinkSubjectFrameData EvaluatedFrame;

		// Manually get all enabled and virtual LiveLink subjects so we can test roles without generating warnings
		const bool bIncludeDisabledSubjects = false;
		const bool bIncludeVirtualSubjects = true;
		TArray<FLiveLinkSubjectKey> AllEnabledSubjectKeys = LiveLinkClient.GetSubjects(bIncludeDisabledSubjects, bIncludeVirtualSubjects);
		const FLiveLinkSubjectKey* FoundSubjectKey = AllEnabledSubjectKeys.FindByPredicate([=](FLiveLinkSubjectKey& InSubjectKey) { return InSubjectKey.SubjectName == LiveLinkSubject; } );

		if (FoundSubjectKey)
		{
			if (LiveLinkClient.DoesSubjectSupportsRole_AnyThread(*FoundSubjectKey, ULiveLinkCameraRole::StaticClass()))
			{
				if (LiveLinkClient.EvaluateFrame_AnyThread(LiveLinkSubject, ULiveLinkCameraRole::StaticClass(), EvaluatedFrame))
				{
					FLiveLinkBlueprintDataStruct WrappedBlueprintData(FLiveLinkCameraBlueprintData::StaticStruct(), &LiveLinkData);
					GetDefault<ULiveLinkCameraRole>()->InitializeBlueprintData(EvaluatedFrame, WrappedBlueprintData);
				}
			}
			else if (LiveLinkClient.DoesSubjectSupportsRole_AnyThread(*FoundSubjectKey, ULiveLinkTransformRole::StaticClass()))
			{
				if (LiveLinkClient.EvaluateFrame_AnyThread(LiveLinkSubject, ULiveLinkTransformRole::StaticClass(), EvaluatedFrame))
				{
					LiveLinkData.FrameData.Transform = EvaluatedFrame.FrameData.Cast<FLiveLinkTransformFrameData>()->Transform;
				}
			}
		}
	}
}

void UVCamComponent::RegisterObjectForInput(UObject* Object)
{
	if (IsValid(InputComponent) && IsValid(Object))
	{
		InputComponent->ClearBindingsForObject(Object);
		UInputDelegateBinding::BindInputDelegates(Object->GetClass(), InputComponent, Object);
	}
}

void UVCamComponent::UnregisterObjectForInput(UObject* Object) const
{
	if (IsValid(InputComponent) && Object)
	{
		InputComponent->ClearBindingsForObject(Object);
	}
}

TArray<FEnhancedActionKeyMapping> UVCamComponent::GetPlayerMappableKeys() const
{
	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		return EnhancedInputSubsystemInterface->GetAllPlayerMappableActionKeyMappings();
	}
	return {};
}

void UVCamComponent::InjectInputForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		EnhancedInputSubsystemInterface->InjectInputForAction( Action, RawValue, Modifiers, Triggers);
	}
}

void UVCamComponent::InjectInputVectorForAction(const UInputAction* Action, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem())
	{
		EnhancedInputSubsystemInterface->InjectInputVectorForAction(Action, Value, Modifiers, Triggers);
	}
}

void UVCamComponent::ApplyInputProfile()
{
	IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetInputVCamSubsystem();
	if (EnhancedInputSubsystemInterface)
	{
		EnhancedInputSubsystemInterface->RemoveAllPlayerMappedKeys();
		for (const TPair<FName, FKey>& MappableKeyOverride : InputProfile.MappableKeyOverrides)
		{
			const FName& MappingName = MappableKeyOverride.Key;
			const FKey& NewKey = MappableKeyOverride.Value;
			
			// Ensure we have a valid name to map
			if (MappingName != NAME_None)
			{
				EnhancedInputSubsystemInterface->AddPlayerMappedKeyInSlot(MappingName, NewKey);
			}
		}
	}
}

void UVCamComponent::EnsureDelegatesRegistered()
{
	if (UE::VCamCore::Private::CanInitVCamInstance(this))
	{
		// Hook into the Live Link Client for our Tick
		IModularFeatures& ModularFeatures = IModularFeatures::Get();

		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			if (!LiveLinkClient.OnLiveLinkTicked().IsBoundToObject(this))
			{
				LiveLinkClient.OnLiveLinkTicked().AddUObject(this, &UVCamComponent::Update);
			}
		}
		else
		{
			UE_LOG(LogVCamComponent, Error, TEXT("LiveLink is not available. Some VCamCore features may not work as expected"));
		}

#if WITH_EDITOR
		// Add the necessary event listeners so we can start/end properly
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"))
			; LevelEditorModule && LevelEditorModule->OnMapChanged().IsBoundToObject(this))
		{
			LevelEditorModule->OnMapChanged().AddUObject(this, &UVCamComponent::OnMapChanged);
		}

		// Both of them should either be valid or invalid
		if (!FEditorDelegates::BeginPIE.IsBoundToObject(this))
		{
			FEditorDelegates::BeginPIE.AddUObject(this, &UVCamComponent::OnBeginPIE);
		}
		if (!FEditorDelegates::EndPIE.IsBoundToObject(this))
		{
			FEditorDelegates::EndPIE.AddUObject(this, &UVCamComponent::OnEndPIE);
		}

		MultiUserStartup();
		if (!FCoreUObjectDelegates::OnObjectsReplaced.IsBoundToObject(this))
		{
			FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UVCamComponent::HandleObjectReplaced);
		}
#endif

		// Apply the Default Input profile if possible
		if (const UVCamInputSettings* VCamInputSettings = GetDefault<UVCamInputSettings>())
		{
			SetInputProfileFromName(VCamInputSettings->DefaultInputProfile);
		}
	}
}

void UVCamComponent::EnsureInitialized()
{
	if (!SubsystemCollection.IsInitialized())
	{
		Initialize();
	}
}

void UVCamComponent::Initialize()
{
	SubsystemCollection.Initialize(this);
	// 1. Input is required
	RegisterInputComponent();

	// 2. Output provider overlay widgets will access the modifiers, so let's init them first
	const bool bInitModifiers = ShouldEvaluateModifierStack() && CanUpdate(); 
	if (bInitModifiers)
	{
		for (FModifierStackEntry& ModifierStackEntry : ModifierStack)
		{
			if (UVCamModifier* Modifier = ModifierStackEntry.GeneratedModifier
				; IsValid(Modifier) && ModifierStackEntry.bEnabled && !Modifier->IsInitialized())
			{
				Modifier->Initialize(ModifierContext, InputComponent);
				AddInputMappingContext(Modifier);
			}
		}
	}

	// 3. Safe to override input actions with custom player mappings now that all modifiers have been registered.
	ApplyInputProfile();

	// 4. Output providers
	const bool bInitOutputProviders = bInitModifiers && ShouldUpdateOutputProviders();
	if (bInitOutputProviders)
	{
		for (UVCamOutputProviderBase* Provider : OutputProviders)
		{
			if (IsValid(Provider))
			{
				Provider->Initialize();
			}
		}
	}
}

void UVCamComponent::Deinitialize()
{
	for (UVCamOutputProviderBase* Provider : OutputProviders)
	{
		if (IsValid(Provider))
		{
			Provider->Deinitialize();
		}
	}

	for (FModifierStackEntry& ModifierEntry : ModifierStack)
	{
		if (IsValid(ModifierEntry.GeneratedModifier))
		{
			ModifierEntry.GeneratedModifier->Deinitialize();
		}
	}

	// Viewports may be manipulated by output providers or modifiers
	UnlockAllViewports();
	
	UnregisterInputComponent();
	SubsystemCollection.Deinitialize();
}

void UVCamComponent::SyncInputSettings()
{
	if (UInputVCamSubsystem* InputSubsystem = GetInputVCamSubsystem())
	{
		SetInputDeviceSettings(InputDeviceSettings);
	}
}

void UVCamComponent::TickModifierStack(const float DeltaTime)
{
	UCineCameraComponent* CameraComponent = GetTargetCamera();
	check(CameraComponent);
	
	for (FModifierStackEntry& ModifierStackEntry : ModifierStack)
	{
		if (UVCamModifier* Modifier = ModifierStackEntry.GeneratedModifier; IsValid(Modifier))
		{
			if (ModifierStackEntry.bEnabled)
			{
				if (!Modifier->IsInitialized())
				{
					Modifier->Initialize(ModifierContext, InputComponent);
					AddInputMappingContext(Modifier);
				}

				Modifier->Apply(ModifierContext, CameraComponent, DeltaTime);
			}
			else if (Modifier->IsInitialized())
			{
				// If the modifier is initialized but not enabled then we deinitialize it
				Modifier->Deinitialize();
			}
		}
	}
}

void UVCamComponent::TickOutputProviders(float DeltaTime)
{
	for (UVCamOutputProviderBase* Provider : OutputProviders)
	{
		if (Provider)
		{
			// Initialize the Provider if required
			if (!Provider->IsInitialized())
			{
				Provider->Initialize();
			}

			Provider->Tick(DeltaTime);
		}
	}
}

void UVCamComponent::TickSubsystems(float DeltaTime)
{
	const TSubclassOf<UVCamSubsystem> SubsystemClass = UVCamSubsystem::StaticClass();
	for (UVCamSubsystem* Subsystem : SubsystemCollection.GetSubsystemArray(SubsystemClass))
	{
		Subsystem->OnUpdate(DeltaTime);
	}
}

void UVCamComponent::CopyLiveLinkDataToCamera(const FLiveLinkCameraBlueprintData& LiveLinkData, UCineCameraComponent* CameraComponent)
{
	const FLiveLinkCameraStaticData& StaticData = LiveLinkData.StaticData;
	const FLiveLinkCameraFrameData& FrameData = LiveLinkData.FrameData;

	if (CameraComponent)
	{
		if (StaticData.bIsFieldOfViewSupported) { CameraComponent->SetFieldOfView(FrameData.FieldOfView); }
		if (StaticData.bIsAspectRatioSupported) { CameraComponent->SetAspectRatio(FrameData.AspectRatio); }
		if (StaticData.bIsProjectionModeSupported) { CameraComponent->SetProjectionMode(FrameData.ProjectionMode == ELiveLinkCameraProjectionMode::Perspective ? ECameraProjectionMode::Perspective : ECameraProjectionMode::Orthographic); }

		if (StaticData.bIsFocalLengthSupported) { CameraComponent->CurrentFocalLength = FrameData.FocalLength; }
		if (StaticData.bIsApertureSupported) { CameraComponent->CurrentAperture = FrameData.Aperture; }
		if (StaticData.FilmBackWidth > 0.0f) { CameraComponent->Filmback.SensorWidth = StaticData.FilmBackWidth; }
		if (StaticData.FilmBackHeight > 0.0f) { CameraComponent->Filmback.SensorHeight = StaticData.FilmBackHeight; }
		if (StaticData.bIsFocusDistanceSupported) { CameraComponent->FocusSettings.ManualFocusDistance = FrameData.FocusDistance; }

		// Naive Transform copy. Should really use something like FLiveLinkTransformControllerData
		CameraComponent->SetRelativeTransform(FrameData.Transform);
	}
}

float UVCamComponent::GetDeltaTime()
{
	float DeltaTime = 0.f;
	const double CurrentEvaluationTime = FPlatformTime::Seconds();

	if (LastEvaluationTime >= 0.0)
	{
		DeltaTime = CurrentEvaluationTime - LastEvaluationTime;
	}

	LastEvaluationTime = CurrentEvaluationTime;
	return DeltaTime;
}

void UVCamComponent::UpdateActorViewportLocks()
{
	if (const UCineCameraComponent* Camera = GetTargetCamera()
		; Camera && ensure(Camera->GetOwner()) && IsEnabled())
	{
		UE::VCamCore::LevelViewportUtils::Private::UpdateViewportLocksFromOutputs(OutputProviders, ViewportLocker, *Camera->GetOwner());
	}
}

void UVCamComponent::UnlockAllViewports()
{
	if (const UCineCameraComponent* Camera = GetTargetCamera()
		; Camera && ensure(Camera->GetOwner()))
	{
		UE::VCamCore::LevelViewportUtils::Private::UnlockAllViewports(ViewportLocker, *Camera->GetOwner());
	}
}

void UVCamComponent::DestroyOutputProvider(UVCamOutputProviderBase* Provider)
{
	if (Provider)
	{
		// Begin Destroy will deinitialize if needed
		Provider->ConditionalBeginDestroy();
		Provider = nullptr;
	}
}

void UVCamComponent::ValidateModifierStack(const FString BaseName /*= "NewModifier"*/)
{
	int32 ModifiedStackIndex;
	bool bIsNewEntry;

	FindModifiedStackEntry(ModifiedStackIndex, bIsNewEntry);

	// Early out in the case of no modified entry
	if (ModifiedStackIndex == INDEX_NONE)
	{
		return;
	}

	// Addition
	if (bIsNewEntry)
	{
		// Keep trying to append an ever increasing int to the base name until we find a unique name
		int32 DuplicatedCount = 1;
		FString UniqueName = BaseName;

		while (DoesNameExistInSavedStack(FName(*UniqueName)))
		{
			UniqueName = BaseName + FString::FromInt(DuplicatedCount++);
		}

		ModifierStack[ModifiedStackIndex].Name = FName(*UniqueName);
	}
	// Edit
	else
	{
		FName NewModifierName = ModifierStack[ModifiedStackIndex].Name;

		// Check if the new name is a duplicate
		bool bIsDuplicate = false;
		for (int32 ModifierIndex = 0; ModifierIndex < ModifierStack.Num(); ++ModifierIndex)
		{
			// Don't check ourselves
			if (ModifierIndex == ModifiedStackIndex)
			{
				continue;
			}
			
			if (ModifierStack[ModifierIndex].Name.IsEqual(NewModifierName))
			{
				bIsDuplicate = true;
				break;
			}
		}

		// If it's a duplicate then reset to the old name
		if (bIsDuplicate)
		{
			ModifierStack[ModifiedStackIndex].Name = SavedModifierStack[ModifiedStackIndex].Name;

			// Add a warning to the log
			UE_LOG(LogVCamComponent, Warning, TEXT("Unable to set Modifier Name to \"%s\" as it is already in use. Resetting Name to previous value \"%s\""),
				*NewModifierName.ToString(),
				*SavedModifierStack[ModifiedStackIndex].Name.ToString());
		}

		// Check if the generated modifier was changed and if so ensure we deinitialize the old modifier
		UVCamModifier* OldModifier = SavedModifierStack[ModifiedStackIndex].GeneratedModifier;
		const UVCamModifier* NewModifier = ModifierStack[ModifiedStackIndex].GeneratedModifier;
		if (OldModifier != NewModifier && IsValid(OldModifier))
		{
			OldModifier->Deinitialize();
		}
			
	}	
}

bool UVCamComponent::DoesNameExistInSavedStack(const FName InName) const
{
	return SavedModifierStack.ContainsByPredicate([InName](const FModifierStackEntry& StackEntry)
		{
			return StackEntry.Name.IsEqual(InName);
		}
	);
}

void UVCamComponent::FindModifiedStackEntry(int32& ModifiedStackIndex, bool& bIsNewEntry) const
{
	ModifiedStackIndex = INDEX_NONE;
	bIsNewEntry = false;

	// Deletion
	if (ModifierStack.Num() < SavedModifierStack.Num())
	{
		// Early out as there's no modified entry remaining
		return;
	}
	// Addition
	else if (ModifierStack.Num() > SavedModifierStack.Num())
	{
		bIsNewEntry = true;
	}
	
	// Try to find the modified or inserted entry
	for (int32 i = 0; i < SavedModifierStack.Num(); i++)
	{
		if (SavedModifierStack[i] != ModifierStack[i])
		{
			ModifiedStackIndex = i;
			break;
		}
	}

	// If we didn't find a difference then the new item was appended to the end
	if (ModifiedStackIndex == INDEX_NONE)
	{
		ModifiedStackIndex = ModifierStack.Num() - 1;
	}

}

#if WITH_EDITOR

void UVCamComponent::OnMapChanged(UWorld* World, EMapChangeType ChangeType)
{
	UWorld* ComponentWorld = GetWorld();
	if (World == ComponentWorld && ChangeType == EMapChangeType::TearDownWorld)
	{
		OnComponentDestroyed(true);
	}
}

void UVCamComponent::OnBeginPIE(const bool bInIsSimulating)
{
	UWorld* World = GetWorld();
	if (World && World->WorldType == EWorldType::Editor)
	{
		// Reasons:
		// 1. Output providers, like pixel streaming, may interfere
		// 2. Input will interfere
		Deinitialize();
		
		// Ensure the Editor components do not update during PIE
		bIsEditorObjectButPIEIsRunning = true;
	}
}

void UVCamComponent::OnEndPIE(const bool bInIsSimulating)
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	if (World->WorldType == EWorldType::PIE)
	{
		// If Update is called (should not happen), disabling instead of Deinitialize to ensure Update() will be skipped
		SetEnabled(false);
	}
	else if (World->WorldType == EWorldType::Editor)
	{
		bIsEditorObjectButPIEIsRunning = false;
		// Next tick because there is still some pending clean up happening this frame after OnEndPIE finishes.
		// In particular, viewports may still be associated with PIE which will cause UVPFullScreenUserWidget to not know where to add itself.
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UVCamComponent>(this)]()
		{
			if (LIKELY(WeakThis.IsValid()))
			{
				WeakThis->Initialize();
			}
		}));
	}
}

void UVCamComponent::SessionStartup(TSharedRef<IConcertClientSession> InSession)
{
	WeakSession = InSession;

	InSession->RegisterCustomEventHandler<FMultiUserVCamCameraComponentEvent>(this, &UVCamComponent::HandleCameraComponentEventData);
	PreviousUpdateTime = FPlatformTime::Seconds();
}

void UVCamComponent::SessionShutdown(TSharedRef<IConcertClientSession> /*InSession*/ )
{
	TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
	if (Session.IsValid())
	{
		Session->UnregisterCustomEventHandler<FMultiUserVCamCameraComponentEvent>(this);
		for (UVCamOutputProviderBase* Provider : OutputProviders)
		{
			if (IsValid(Provider))
			{
				Provider->RestoreOutput();
			}
		}
	}

	WeakSession.Reset();
}

FString UVCamComponent::GetNameForMultiUser() const
{
	return GetOwner()->GetPathName();
}

void UVCamComponent::HandleCameraComponentEventData(const FConcertSessionContext& InEventContext, const FMultiUserVCamCameraComponentEvent& InEvent)
{
	if (InEvent.TrackingName == GetNameForMultiUser())
	{
		// If the role matches the currently defined VP Role then we should not update the camera
		// data for this actor and the modifier stack is the "owner"
		//
		if (!IsCameraInVPRole())
		{
			InEvent.CameraData.ApplyTo(GetOwner(), GetTargetCamera());
			if (bDisableOutputOnMultiUserReceiver)
			{
				for (UVCamOutputProviderBase* Provider : OutputProviders)
				{
					Provider->SuspendOutput();
				}
			}
		}
	}
}

void UVCamComponent::MultiUserStartup()
{
	if (TSharedPtr<IConcertSyncClient> ConcertSyncClient = IMultiUserClientModule::Get().GetClient())
	{
		IConcertClientRef ConcertClient = ConcertSyncClient->GetConcertClient();

		// Both of them should either be valid or invalid
		if (!OnSessionStartupHandle.IsValid())
		{
			OnSessionStartupHandle = ConcertClient->OnSessionStartup().AddUObject(this, &UVCamComponent::SessionStartup);
		}
		if (!OnSessionShutdownHandle.IsValid())
		{
			OnSessionShutdownHandle = ConcertClient->OnSessionShutdown().AddUObject(this, &UVCamComponent::SessionShutdown);
		}

		TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
		if (ConcertClientSession.IsValid())
		{
			SessionStartup(ConcertClientSession.ToSharedRef());
		}
	}
}
void UVCamComponent::MultiUserShutdown()
{
	if (IMultiUserClientModule::IsAvailable())
	{
		if (TSharedPtr<IConcertSyncClient> ConcertSyncClient = IMultiUserClientModule::Get().GetClient())
		{
			IConcertClientRef ConcertClient = ConcertSyncClient->GetConcertClient();

			TSharedPtr<IConcertClientSession> ConcertClientSession = ConcertClient->GetCurrentSession();
			if (ConcertClientSession.IsValid())
			{
				SessionShutdown(ConcertClientSession.ToSharedRef());
			}

			ConcertClient->OnSessionStartup().Remove(OnSessionStartupHandle);
			OnSessionStartupHandle.Reset();

			ConcertClient->OnSessionShutdown().Remove(OnSessionShutdownHandle);
			OnSessionShutdownHandle.Reset();
		}
	}
}
#endif

// Multi-user support
void UVCamComponent::SendCameraDataViaMultiUser()
{
	if (!IsCameraInVPRole())
	{
		return;
	}
#if WITH_EDITOR
	// Update frequency 15 Hz
	const double LocationUpdateFrequencySeconds = UpdateFrequencyMs / 1000.0;
	const double CurrentTime = FPlatformTime::Seconds();

	double DeltaTime = CurrentTime - PreviousUpdateTime;
	SecondsSinceLastLocationUpdate += DeltaTime;

	if (SecondsSinceLastLocationUpdate >= LocationUpdateFrequencySeconds)
	{
		TSharedPtr<IConcertClientSession> Session = WeakSession.Pin();
		if (Session.IsValid())
		{
			TArray<FGuid> ClientIds = Session->GetSessionClientEndpointIds();
			FMultiUserVCamCameraComponentEvent CameraEvent{GetNameForMultiUser(),{GetOwner(),GetTargetCamera()}};
			Session->SendCustomEvent(CameraEvent, ClientIds, EConcertMessageFlags::None);
		}
		SecondsSinceLastLocationUpdate = 0;
	}
	PreviousUpdateTime = CurrentTime;
#endif
}

bool UVCamComponent::ShouldUpdateOutputProviders() const
{
	// We should only update output providers in 3 situations
	// - We're not in Multi User
	// - We have the virtual camera role
	// - We have explicitly set that we want to run Output Providers even when not in the camera role
	return ShouldEvaluateModifierStack() || !bDisableOutputOnMultiUserReceiver;
}

bool UVCamComponent::ShouldEvaluateModifierStack() const
{
	return !IsMultiUserSession() || IsCameraInVPRole();
}

bool UVCamComponent::IsMultiUserSession() const
{
#if WITH_EDITOR
	return WeakSession.IsValid();
#else
	return false;
#endif
}

bool UVCamComponent::IsCameraInVPRole() const
{
#if WITH_EDITOR
	// We are in a valid camera role if the user has not assigned a role or the current VPSettings role matches the
	// assigned role.
	UVirtualProductionRolesSubsystem* VPRolesSubsystem = GEngine->GetEngineSubsystem<UVirtualProductionRolesSubsystem>();
	return !Role.IsValid() || (VPRolesSubsystem && VPRolesSubsystem->HasActiveRole(Role));
#else
	return true;
#endif
}
