// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamComponent.h"

#include "CineCameraComponent.h"
#include "ILiveLinkClient.h"
#include "VCamModifier.h"
#include "VCamModifierContext.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkTransformRole.h"
#include "GameDelegates.h"
#include "Engine/GameEngine.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/InputDelegateBinding.h"
#include "InputMappingContext.h"

#include "GameFramework/InputSettings.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"

#include "IConcertModule.h"
#include "IConcertClient.h"
#include "IConcertSession.h"
#include "IConcertSyncClient.h"
#include "IMultiUserClientModule.h"

#include "EnhancedInputEditorSubsystem.h"

#include "VPSettings.h"
#include "VPRolesSubsystem.h"
#endif


DEFINE_LOG_CATEGORY(LogVCamComponent);

namespace UE::VCamCore::VCamComponent::Private
{
	static const FName LevelEditorName(TEXT("LevelEditor"));

	static bool IsArchetype(const UVCamComponent& Component)
	{
		// Flags explained:
		// 1. The Blueprint editor has two objects:
		//	1.1 The "real" one which saves the property data - this one is RF_ArchetypeObject
		//	1.2 The preview one (which I assume is displayed in the viewport) - this one is RF_Transient.
		// 2. When you drag-create an actor, level editor creates a RF_Transient template actor. After you release the mouse, a real one is created (not RF_Transient).
		const bool bHasDefaultFlags = Component.HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient);
		// The above flags do not always cover the Blueprint editor world
		const bool bIsInBlueprintEditor = !Component.GetWorld() || Component.GetWorld()->WorldType == EWorldType::EditorPreview;
	
		return bHasDefaultFlags || bIsInBlueprintEditor;
	}
}

UVCamComponent::UVCamComponent()
{
	using namespace UE::VCamCore::VCamComponent::Private;
	
	// For temporary objects, the InputComponent should not be created and neither should we subscribe to global callbacks
	if (!IsArchetype(*this) && !GIsCookerLoadingPackage)
	{
		// Hook into the Live Link Client for our Tick
		IModularFeatures& ModularFeatures = IModularFeatures::Get();

		if (ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			ILiveLinkClient& LiveLinkClient = ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkClient.OnLiveLinkTicked().AddUObject(this, &UVCamComponent::Update);
		}
		else
		{
			UE_LOG(LogVCamComponent, Error, TEXT("LiveLink is not available. Some VCamCore features may not work as expected"));
		}

#if WITH_EDITOR
		// Add the necessary event listeners so we can start/end properly
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(LevelEditorName))
		{
			LevelEditorModule->OnMapChanged().AddUObject(this, &UVCamComponent::OnMapChanged);
		}

		FEditorDelegates::BeginPIE.AddUObject(this, &UVCamComponent::OnBeginPIE);
		FEditorDelegates::EndPIE.AddUObject(this, &UVCamComponent::OnEndPIE);

		MultiUserStartup();
		FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UVCamComponent::HandleObjectReplaced);
#endif

		// Setup Input
		InputComponent = NewObject<UInputComponent>(this, UInputSettings::GetDefaultInputComponentClass(), TEXT("VCamInput0"), RF_Transient);

		// Apply the Default Input profile if possible
		if (const UVCamInputSettings* VCamInputSettings = GetDefault<UVCamInputSettings>())
		{
			SetInputProfileFromName(VCamInputSettings->DefaultInputProfile);
		}
		
	}
}

void UVCamComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	bLockViewportToCamera = false;
	UpdateActorLock();

	for (UVCamOutputProviderBase* Provider : OutputProviders)
	{
		if (Provider)
		{
			Provider->Deinitialize();
		}
	}

	for (FModifierStackEntry& ModifierEntry : ModifierStack)
	{
		TObjectPtr<UVCamModifier>& Modifier = ModifierEntry.GeneratedModifier;
		if (Modifier && !Modifier->DoesRequireInitialization())
		{
			Modifier->Deinitialize();
		}
	}
	
	// Hotfix 5.1.1. Remove if you find this on any other version and instead remove ~FModifierStackEntry!
	// Fix for 5.1 only (in 5.2 just remove the destructor)... ~FModifierStackEntry can cause a crash set to nullptr so it is skipped
	ModifierStack.Empty();
	SavedModifierStack.Empty();

	UnregisterInputComponent();

#if WITH_EDITOR
	// Remove all event listeners
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(UE::VCamCore::VCamComponent::Private::LevelEditorName))
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
	// This function should only ever be called when we have a valid component replacing us
	check(ReplacementComponent);

	// Make sure to copy over our delegate bindings to the component replacing us
	ReplacementComponent->OnComponentReplaced = OnComponentReplaced;

	OnComponentReplaced.Clear();
	// Ensure all modifiers and output providers get deinitialized
	SetEnabled(false);

	// Refresh the enabled state on the new component to prevent any stale state from the replacement
	if (ReplacementComponent->IsEnabled())
	{
		ReplacementComponent->SetEnabled(false);
		ReplacementComponent->SetEnabled(true);
	}

	// There's a current issue where FKeys will be nulled when the component reconstructs so we'll explicitly
	// pass the Input Profile to the new component to avoid this
	ReplacementComponent->InputProfile = InputProfile;
	ReplacementComponent->ApplyInputProfile();
	
	DestroyComponent();
}

IEnhancedInputSubsystemInterface* UVCamComponent::GetEnhancedInputSubsystemInterface() const
{
	if (const UWorld* World = GetWorld(); IsValid(World) && World->IsGameWorld())
	{
		
		if (const ULocalPlayer* FirstLocalPlayer = World->GetFirstLocalPlayerFromController())
		{
			return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(FirstLocalPlayer);
		}
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>();
	}
#endif
	return nullptr;
}

void UVCamComponent::RegisterInputComponent()
{
	// Ensure we start from a clean slate
	UnregisterInputComponent();
	
	if (const UWorld* World = GetWorld(); IsValid(World) && World->IsGameWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->PushInputComponent(InputComponent);
			bIsInputRegistered = true;
		}
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		if (UEnhancedInputEditorSubsystem* EditorInputSubsystem = GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>())
		{
			EditorInputSubsystem->PushInputComponent(InputComponent);
			EditorInputSubsystem->StartConsumingInput();
			bIsInputRegistered = true;
		}
	}
#endif
}

void UVCamComponent::UnregisterInputComponent()
{
	// Removes the component from both editor and runtime input systems if possible
	//
	// Note: Despite the functions being called "Pop" it's actually just removing our specific input component from
	// the stack rather than blindly popping the top component
	if (const UWorld* World = GetWorld(); IsValid(World) && World->IsGameWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->PopInputComponent(InputComponent);
		}
	}
#if WITH_EDITOR
	if (GEditor)
	{
		if (UEnhancedInputEditorSubsystem* EditorInputSubsystem = GEditor->GetEditorSubsystem<UEnhancedInputEditorSubsystem>())
		{
			EditorInputSubsystem->PopInputComponent(InputComponent);
		}
	}
#endif
	AppliedInputContexts.Reset();
	bIsInputRegistered = false;
}

bool UVCamComponent::CanUpdate() const
{
	UWorld* World = GetWorld();
	if (bEnabled && IsValid(this) && !bIsEditorObjectButPIEIsRunning && World)
	{
		// Check that we only update in a valid world type
		// This prevents us updating in asset editors or invalid worlds
		constexpr int ValidWorldTypes = EWorldType::Game | EWorldType::PIE | EWorldType::Editor;
		if ((World->WorldType & ValidWorldTypes) != EWorldType::None)
		{
			if (const USceneComponent* ParentComponent = GetAttachParent())
			{
				if (ParentComponent->IsA<UCineCameraComponent>())
				{
					// Component is valid to use if it is enabled, has a parent and that parent is a CineCamera derived component
					return true;
				}
			}
		}
	}
	return false;

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

void UVCamComponent::PostLoad()
{
	Super::PostLoad();

	// Ensure the input profile is applied when this component is loaded
	ApplyInputProfile();
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
		else if (MemberPropertyName == NAME_Enabled
			// No enabling archetypes
			&& !UE::VCamCore::VCamComponent::Private::IsArchetype(*this))
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
	// No enabling archetypes
	if (!UE::VCamCore::VCamComponent::Private::IsArchetype(*this)
		&& Property
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_LockViewportToCamera = GET_MEMBER_NAME_CHECKED(UVCamComponent, bLockViewportToCamera);
		static FName NAME_Enabled = GET_MEMBER_NAME_CHECKED(UVCamComponent, bEnabled);
		static FName NAME_ModifierStack = GET_MEMBER_NAME_CHECKED(UVCamComponent, ModifierStack);
		static FName NAME_TargetViewport = GET_MEMBER_NAME_CHECKED(UVCamComponent, TargetViewport);
		static FName NAME_InputProfile = GET_MEMBER_NAME_CHECKED(UVCamComponent, InputProfile);

		const FName PropertyName = Property->GetFName();

		if (PropertyName == NAME_LockViewportToCamera)
		{
			UpdateActorLock();
		}
		else if (PropertyName == NAME_Enabled)
		{
			// Only act here if we are a struct (like FModifierStackEntry)
			if (!Property->GetOwner<UClass>())
			{
				SetEnabled(bEnabled);
			}
		}
		else if (PropertyName == NAME_ModifierStack)
		{
			ValidateModifierStack();
			SavedModifierStack.Reset();
		}
		else if (PropertyName == NAME_TargetViewport)
		{
			if (bEnabled)
			{
				SetEnabled(false);
				SetEnabled(true);

				if (bLockViewportToCamera)
				{
					SetActorLock(false);
					SetActorLock(true);
				}
			}
		}
		else if (PropertyName == NAME_InputProfile)
		{
			ApplyInputProfile();
		}

		for (const UVCamOutputProviderBase* OutputProvider : OutputProviders)
		{
			if (IsValid(OutputProvider))
			{
				OutputProvider->NotifyWidgetOfComponentChange();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UVCamComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.PropertyChain.GetActiveNode()->GetValue();
	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_OutputProviders = GET_MEMBER_NAME_CHECKED(UVCamComponent, OutputProviders);

		if (Property->GetFName() == NAME_OutputProviders)
		{
			FProperty* ActualProperty = PropertyChangedEvent.PropertyChain.GetActiveNode()->GetNextNode() ? PropertyChangedEvent.PropertyChain.GetActiveNode()->GetNextNode()->GetValue() : nullptr;
			if (ActualProperty == nullptr)
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

			// We created this in PreEditChange, so we need to always get rid of it
			SavedOutputProviders.Empty();
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UVCamComponent::AddInputMappingContext(const UVCamModifier* Modifier)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
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
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
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
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
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
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
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

	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
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
				if (Mapping.bIsPlayerMappable)
				{
					const FName MappingName = Mapping.PlayerMappableOptions.Name;

					// Prefer to use the current mapped key but fallback to the default if no key is mapped
					FKey CurrentKey = EnhancedInputSubsystemInterface->GetPlayerMappedKey(MappingName);
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
	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
	{
		return EnhancedInputSubsystemInterface->GetAllPlayerMappableActionKeyMappings();
	}
	return TArray<FEnhancedActionKeyMapping>();
}

FKey UVCamComponent::GetPlayerMappedKey(const FName MappingName) const
{
	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
	{
		return EnhancedInputSubsystemInterface->GetPlayerMappedKey(MappingName);
	}
	return EKeys::Invalid;
}


void UVCamComponent::Update()
{
	if (!CanUpdate())
	{
		return;
	}

	// Ensure we register for input if we've not previously registered
	if (!bIsInputRegistered)
	{
		RegisterInputComponent();
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
	UCineCameraComponent* CameraComponent = GetTargetCamera();

	if (!CameraComponent)
	{
		UE_LOG(LogVCamComponent, Error, TEXT("Parent component wasn't valid for Update"));
		return;
	}

	const float DeltaTime = GetDeltaTime();

	if (CanEvaluateModifierStack())
	{
		// Ensure the actor lock reflects the state of the lock property
		// This is needed as UActorComponent::ConsolidatedPostEditChange will cause the component to be reconstructed on PostEditChange
		// if the component is inherited
		if (bLockViewportToCamera != bIsLockedToViewport)
		{
			UpdateActorLock();
		}

		FLiveLinkCameraBlueprintData InitialLiveLinkData;
		GetLiveLinkDataForCurrentFrame(InitialLiveLinkData);

		CopyLiveLinkDataToCamera(InitialLiveLinkData, CameraComponent);

		for (FModifierStackEntry& ModifierStackEntry : ModifierStack)
		{
			if (UVCamModifier* Modifier = ModifierStackEntry.GeneratedModifier; IsValid(Modifier))
			{
				if (ModifierStackEntry.bEnabled)
				{
					// Initialize the Modifier if required
					if (Modifier->DoesRequireInitialization())
					{
						Modifier->Initialize(ModifierContext, InputComponent);
						AddInputMappingContext(Modifier);
					}

					Modifier->Apply(ModifierContext, CameraComponent, DeltaTime);
				}
				else
				{
					// If the modifier is initialized but not enabled then we deinitialize it
					if (!Modifier->DoesRequireInitialization())
					{
						Modifier->Deinitialize();
					}
				}
			}
		}

		SendCameraDataViaMultiUser();
	}

	if (ShouldUpdateOutputProviders())
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
}

void UVCamComponent::SetEnabled(bool bNewEnabled)
{
	// Disable all outputs and modifiers if we're no longer enabled
	// NOTE this must be done BEFORE setting the actual bEnabled variable because OutputProviderBase now checks the component enabled state
	if (!bNewEnabled)
	{
		for (UVCamOutputProviderBase* Provider : OutputProviders)
		{
			if (IsValid(Provider))
			{
				Provider->Deinitialize();
			}
		}

		// There's no need to call Initialize if we are being enabled as they'll get automatically intialized
		// the next time that the stack is evaluated
		for (FModifierStackEntry& ModifierEntry : ModifierStack)
		{
			if (IsValid(ModifierEntry.GeneratedModifier))
			{
				ModifierEntry.GeneratedModifier->Deinitialize();
			}
		}
	}

	bEnabled = bNewEnabled;

	// Enable any outputs that are set to active
	// NOTE this must be done AFTER setting the actual bEnabled variable because OutputProviderBase now checks the component enabled state
	if (bNewEnabled)
	{
		if (ShouldUpdateOutputProviders() && CanUpdate())
		{
			for (UVCamOutputProviderBase* Provider : OutputProviders)
			{
				if (IsValid(Provider))
				{
					Provider->Initialize();
				}
			}
		}

		// Register the input component now we're enabled
		RegisterInputComponent();
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
	if (const IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
	{
		return EnhancedInputSubsystemInterface->GetAllPlayerMappableActionKeyMappings();
	}
	return {};
}

void UVCamComponent::InjectInputForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
	{
		EnhancedInputSubsystemInterface->InjectInputForAction( Action, RawValue, Modifiers, Triggers);
	}
}

void UVCamComponent::InjectInputVectorForAction(const UInputAction* Action, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if (IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface())
	{
		EnhancedInputSubsystemInterface->InjectInputVectorForAction(Action, Value, Modifiers, Triggers);
	}
}

void UVCamComponent::ApplyInputProfile()
{
	IEnhancedInputSubsystemInterface* EnhancedInputSubsystemInterface = GetEnhancedInputSubsystemInterface();
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
				EnhancedInputSubsystemInterface->AddPlayerMappedKey(MappingName, NewKey);
			}
		}
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

void UVCamComponent::UpdateActorLock()
{
	if (GetTargetCamera() == nullptr)
	{
		UE_LOG(LogVCamComponent, Warning, TEXT("UpdateActorLock has been called, but there is no valid TargetCamera!"));
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
#if WITH_EDITOR
		if (Context.WorldType == EWorldType::Editor)
		{
			if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
			{
				if (bLockViewportToCamera)
				{
					Backup_ActorLock = LevelViewportClient->GetActiveActorLock();
					LevelViewportClient->SetActorLock(GetTargetCamera()->GetOwner());
					// If bLockedCameraView is not true then the viewport is locked to the actor's transform and not the camera component
					LevelViewportClient->bLockedCameraView = true;
					bIsLockedToViewport = true;
				}
				else if (Backup_ActorLock.IsValid())
				{
					LevelViewportClient->SetActorLock(Backup_ActorLock.Get());
					Backup_ActorLock = nullptr;
					// If bLockedCameraView is not true then the viewport is locked to the actor's transform and not the camera component
					LevelViewportClient->bLockedCameraView = true;
					bIsLockedToViewport = false;
				}
				else
				{
					LevelViewportClient->SetActorLock(nullptr);
					bIsLockedToViewport = false;
				}
			}
		}
		else
#endif
		{
			UWorld* ActorWorld = Context.World();
			if (ActorWorld && ActorWorld->GetGameInstance())
			{
				APlayerController* PlayerController = ActorWorld->GetGameInstance()->GetFirstLocalPlayerController(ActorWorld);
				if (PlayerController)
				{
					if (bLockViewportToCamera)
					{
						Backup_ViewTarget = PlayerController->GetViewTarget();
						PlayerController->SetViewTarget(GetTargetCamera()->GetOwner());
						bIsLockedToViewport = true;
					}
					else if (Backup_ViewTarget.IsValid())
					{
						PlayerController->SetViewTarget(Backup_ViewTarget.Get());
						Backup_ViewTarget = nullptr;
						bIsLockedToViewport = false;
					}
					else
					{
						PlayerController->SetViewTarget(nullptr);
						bIsLockedToViewport = false;
					}
				}
			}
		}
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

void UVCamComponent::ResetAllOutputProviders()
{
	for (UVCamOutputProviderBase* Provider : OutputProviders)
	{
		if (Provider)
		{
			// Initialization will also recover active state 
			Provider->Deinitialize();

			// We only Initialize a provider if they're able to be updated
			// If they later become able to be updated then they will be
			// Initialized inside the Update() loop
			if (ShouldUpdateOutputProviders())
			{
				Provider->Initialize();
			}
		}
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

TSharedPtr<FSceneViewport> UVCamComponent::GetTargetSceneViewport() const
{
	TSharedPtr<FSceneViewport> SceneViewport;

#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				if (SlatePlayInEditorSession)
				{
					if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
					{
						TSharedPtr<IAssetViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
						SceneViewport = DestinationLevelViewport->GetSharedActiveViewport();
					}
					else if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						SceneViewport = SlatePlayInEditorSession->SlatePlayInEditorWindowViewport;
					}

					// If PIE is active always choose it
					break;
				}
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
				{
					TSharedPtr<SEditorViewport> ViewportWidget = LevelViewportClient->GetEditorViewportWidget();
					if (ViewportWidget.IsValid())
					{
						SceneViewport = ViewportWidget->GetSceneViewport();
					}
				}
			}
		}
	}
#else
	if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		SceneViewport = GameEngine->SceneViewport;
	}
#endif

	return SceneViewport;
}

TWeakPtr<SWindow> UVCamComponent::GetTargetInputWindow() const
{
	TWeakPtr<SWindow> InputWindow;

#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				if (SlatePlayInEditorSession)
				{
					if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
					{
						TSharedPtr<IAssetViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
						InputWindow = FSlateApplication::Get().FindWidgetWindow(DestinationLevelViewport->AsWidget());
					}
					else if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						InputWindow = SlatePlayInEditorSession->SlatePlayInEditorWindow;
					}

					// If PIE is active always choose it
					break;
				}
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				if (FLevelEditorViewportClient* LevelViewportClient = GetTargetLevelViewportClient())
				{
					TSharedPtr<SEditorViewport> ViewportWidget = LevelViewportClient->GetEditorViewportWidget();
					if (ViewportWidget.IsValid())
					{
						InputWindow = FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());
					}
				}
			}
		}
	}
#else
	if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		InputWindow = GameEngine->GameViewportWindow;
	}
#endif

	return InputWindow;
}

#if WITH_EDITOR
FLevelEditorViewportClient* UVCamComponent::GetTargetLevelViewportClient() const
{
	FLevelEditorViewportClient* OutClient = nullptr;

	TSharedPtr<SLevelViewport> LevelViewport = GetTargetLevelViewport();
	if (LevelViewport.IsValid())
	{
		OutClient = &LevelViewport->GetLevelViewportClient();
	}

	return OutClient;
}

TSharedPtr<SLevelViewport> UVCamComponent::GetTargetLevelViewport() const
{
	TSharedPtr<SLevelViewport> OutLevelViewport = nullptr;

	if (TargetViewport == EVCamTargetViewportID::CurrentlySelected)
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(UE::VCamCore::VCamComponent::Private::LevelEditorName))
		{
			OutLevelViewport = LevelEditorModule->GetFirstActiveLevelViewport();
		}
	}
	else
	{
		if (GEditor)
		{
			for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
			{
				// We only care about the fully rendered 3D viewport...seems like there should be a better way to check for this
				if (!Client->IsOrtho())
				{
					TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(Client->GetEditorViewportWidget());
					if (LevelViewport.IsValid())
					{
						const FString WantedViewportString = FString::Printf(TEXT("Viewport %d.Viewport"), (int32)TargetViewport);
						const FString ViewportConfigKey = LevelViewport->GetConfigKey().ToString();
						if (ViewportConfigKey.Contains(*WantedViewportString, ESearchCase::CaseSensitive, ESearchDir::FromStart))
						{
							OutLevelViewport = LevelViewport;
							break;
						}
					}
				}
			}
		}
	}

	return OutLevelViewport;
}

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

	if (!World)
	{
		return;
	}

	if (World->WorldType == EWorldType::Editor)
	{
		// Deinitialize all output providers in the editor world
		for (UVCamOutputProviderBase* Provider : OutputProviders)
		{
			if (Provider)
			{
				Provider->Deinitialize();
			}
		}

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
		// Disable all output providers in the PIE world
		for (UVCamOutputProviderBase* Provider : OutputProviders)
		{
			if (Provider)
			{
				Provider->Deinitialize();
			}
		}
	}
	else if (World->WorldType == EWorldType::Editor)
	{
		// Allow the Editor components to start updating again
		bIsEditorObjectButPIEIsRunning = false;
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

		OnSessionStartupHandle = ConcertClient->OnSessionStartup().AddUObject(this, &UVCamComponent::SessionStartup);
		OnSessionShutdownHandle = ConcertClient->OnSessionShutdown().AddUObject(this, &UVCamComponent::SessionShutdown);

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

bool UVCamComponent::IsCameraInVPRole() const
{
#if WITH_EDITOR
	UVPSettings* Settings = UVPSettings::GetVPSettings();
	// We are in a valid camera role if the user has not assigned a role or the current VPSettings role matches the
	// assigned role.
	//
	UVirtualProductionRolesSubsystem* VPRolesSubsystem = GEngine->GetEngineSubsystem<UVirtualProductionRolesSubsystem>();
	return !Role.IsValid() || (VPRolesSubsystem && VPRolesSubsystem->HasActiveRole(Role));
#else
	return true;
#endif
}

bool UVCamComponent::CanEvaluateModifierStack() const
{
	return !IsMultiUserSession() || (IsMultiUserSession() && IsCameraInVPRole());
}

bool UVCamComponent::ShouldUpdateOutputProviders() const
{
	// We should only update output providers in 3 situations
	// - We're not in Multi User
	// - We have the virtual camera role
	// - We have explicitly set that we want to run Output Providers even when not in the camera role
	return !IsMultiUserSession() || IsCameraInVPRole() || !bDisableOutputOnMultiUserReceiver;
}

bool UVCamComponent::IsMultiUserSession() const
{
#if WITH_EDITOR
	return WeakSession.IsValid();
#else
	return false;
#endif
}

