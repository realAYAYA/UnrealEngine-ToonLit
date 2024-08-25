// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EVCamTargetViewportID.h"
#include "Input/VCamInputDeviceConfig.h"
#include "Input/VCamInputSettings.h"
#include "Util/VCamViewportLocker.h"
#include "Modifier/ModifierStackEntry.h"
#include "VCamSubsystem.h"

#include "GameplayTagContainer.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "Subsystems/SubsystemCollection.h"
#if WITH_EDITOR
#include "UnrealEdMisc.h"
#endif

#include "VCamComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamComponent, Log, All);

class IEnhancedInputSubsystemInterface;
class UCineCameraComponent;
class UInputModifier;
class UInputTrigger;
class UInputVCamSubsystem;
class UVCamModifierContext;
class UVCamOutputProviderBase;

struct FEnhancedActionKeyMapping;
struct FVCamComponentInstanceData;

#if WITH_EDITOR
class FLevelEditorViewportClient;
class FObjectPreSaveContext;
class FObjectPostSaveContext;
class IConcertClientSession;
struct FConcertSessionContext;
struct FMultiUserVCamCameraComponentEvent;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComponentReplaced, UVCamComponent*, NewComponent);

/**
 * Provides a modular system for editing a UCineCameraComponent using user widgets.
 * This component must be attached as a direct child of UCineCameraComponent.
 *
 * This component implements a Model-View-Controller architecture where modifiers are the model, output providers the view, and
 * this component the controller.
 *
 * There are three aspects to this component:
 * - Modifiers implement logic for changing properties on the UCineCameraComponent. Modifiers contain ConnectionPoints.
 *   ConnectionPoints can optionally expose UInputActions (Enhanced Input) that can be invoked by widgets.
 * - Output providers create and render widgets (possibly streaming them). Usually output providers create UVCamWidgets, which
 *   are special widgets that can connect to ConnectionPoints. Widgets interact with modifiers in two ways:
 *     - Simple: trigger input actions that modifiers are subscribed to and expose via connections.
 *     - Advanced: query whether modifiers implement certain custom defined interfaces.
 *       UVCamWidget Connections can be configured with required and optional interfaces.
 * - UVCamSubsystems exist for as long as a UVCamComponent is enabled (this is comparable to ULocalPlayerSubsystem).
 *   One notable such system is the UInputVCamSubsystem which allows UVCamComponents to bind to input devices similarly to how
 *   APlayerControllers do in shipped games.
 */
UCLASS(Blueprintable, ClassGroup = VCam, HideCategories=(Mobility), meta=(BlueprintSpawnableComponent))
class VCAMCORE_API UVCamComponent : public USceneComponent
{
	GENERATED_BODY()
	friend class UVCamModifier;
	friend class UVCamBlueprintAssetUserData;
	friend struct FVCamComponentInstanceData;
public:

	/**
	 * There are situations in the editor where the component may be replaced by another component as part of the actor being reconstructed
	 * This event will notify you of that change and give you a reference to the new component.
	 * Bindings will be copied to the new component so you do not need to rebind this event
	 * 
	 * Note: When the component is replaced you will need to get all properties on the component again such as Modifiers and Output Providers
	 */
	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera")
	FOnComponentReplaced OnComponentReplaced;

	//~ Begin UActorComponent Interface
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnRegister() override;
	virtual void BeginDestroy() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
	virtual void OnAttachmentChanged() override;
	//~ End USceneComponent Interface
	
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void CheckForErrors() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	void OnOutputProvidersEdited(FPropertyChangedChainEvent& PropertyChangedEvent);
	void OnTargetViewportEdited();
#endif // WITH_EDITOR

	/** Applies the component instance cache */
	void ApplyComponentInstanceData(FVCamComponentInstanceData& ComponentInstanceData, ECacheApplyPhase CacheApplyPhase);

	bool CanUpdate() const;
	void Update();

	/******************** Input ********************/
	
	/** Adds the Input Mapping Context from a modifier, if it exists, to the input system */
	void AddInputMappingContext(const UVCamModifier* Modifier);
	/** Removes the Input Mapping Context from a modifier, if it exists, from the input system */
	void RemoveInputMappingContext(const UVCamModifier* Modifier);
	/** Adds an explicitly provided Input Mapping Context to the input system */
	void AddInputMappingContext(UInputMappingContext* Context, int32 Priority);
	/** Removes an explicitly provided Input Mapping Context to the input system */
	void RemoveInputMappingContext(UInputMappingContext* Context);
	
	/**
	 * Attempts to apply key mapping settings from an input profile defined in VCam Input Settings
	 * Returns whether the profile was found and able to be applied
	 */
	UFUNCTION(BlueprintCallable, Category = "VCam Input")
	bool SetInputProfileFromName(const FName ProfileName);

	/**
	 * Tries to add a new Input Profile to the VCam Input Settings and populates it with any currently active player mappable keys
	 * Note: The set of currently active player mappable keys may be larger than the set of mappings in this component's Input Profile
	 *
	 * Optionally this function can update an existing profile, this will only add any mappable keys that don't currently exist in the profile but will not remove any existing mappings
	 * Returns whether the profile was successfully added or updated
	 */
	UFUNCTION(BlueprintCallable, Category="VCam Input", meta=(ReturnDisplayName = "Success"))
	bool AddInputProfileWithCurrentlyActiveMappings(const FName ProfileName, bool bUpdateIfProfileAlreadyExists = true);

	// Saves the current input profile settings to the VCam Input Settings using the provided Profile Name
	UFUNCTION(BlueprintCallable, Category="VCam Input", meta=(ReturnDisplayName = "Success"))
	bool SaveCurrentInputProfileToSettings(const FName ProfileName) const;
	
	// Searches the currently active input system for all registered key mappings that are marked as Player Mappable.
	UFUNCTION(BlueprintCallable, Category="VCam Input", meta=(ReturnDisplayName = "PlayerMappableActionKeyMappings"))
	TArray<FEnhancedActionKeyMapping> GetAllPlayerMappableActionKeyMappings() const;

	// Searches the currently active input system for the current key mapped to a given input mapping
	// If there is not a player mapped key, then this will return EKeys::Invalid.
	UFUNCTION(BlueprintCallable, Category="VCam Input", meta=(ReturnDisplayName = "Key"))
	FKey GetPlayerMappedKey(const FName MappingName) const;


	/** Sets whether the VCamComponent will update every frame */
	UFUNCTION(BlueprintSetter)
	void SetEnabled(bool bNewEnabled);
	/** @return Whether the VCamComponent will update every frame */
	UFUNCTION(BlueprintGetter)
	bool IsEnabled() const { return bEnabled; };

	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UCineCameraComponent* GetTargetCamera() const;

	
	/******************** Modifier ********************/
	
	/**
	 * Add a modifier to the stack with a given name.
	 * If that name is already in use then the modifier will not be added.
	 * Returns the created modifier if the Add succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ModifierClass", DynamicOutputParam = "CreatedModifier", ReturnDisplayName = "Success"))
	bool AddModifier(const FName Name, UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifier> ModifierClass, UVCamModifier*& CreatedModifier);
	/**
	 * Insert a modifier to the stack with a given name and index.
	 * If that name is already in use then the modifier will not be added.
	 * The index must be between zero and the number of existing modifiers inclusive
	 * Returns the created modifier if the Add succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ModifierClass", DynamicOutputParam = "CreatedModifier", ReturnDisplayName = "Success"))
	bool InsertModifier(const FName Name, int32 Index, UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifier> ModifierClass, UVCamModifier*& CreatedModifier);
	// Moves an existing modifier in the stack from its current index to a new index
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (ReturnDisplayName = "Success"))
	bool SetModifierIndex(int32 OriginalIndex, int32 NewIndex);

	/** Remove all Modifiers from the Stack. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void RemoveAllModifiers();
	/** @return Whether the modifier was removed successfully */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
    bool RemoveModifier(const UVCamModifier* Modifier);
	/** @return Whether the modifier was removed successfully */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	bool RemoveModifierByIndex(const int ModifierIndex);
	/** @return Whether the modifier was removed successfully. */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
    bool RemoveModifierByName(const FName Name);

	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	int32 GetNumberOfModifiers() const;

	/**
	 * Returns all the Modifiers in the Component's Stack
	 * Note: It's possible not all Modifiers will be valid (such as if the user has not set a class for the modifier in the details panel)
	 */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	void GetAllModifiers(TArray<UVCamModifier*>& Modifiers) const;
	
	/** Returns all the modifier names used to identifying connection points. */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	TArray<FName> GetAllModifierNames() const;

	const TArray<FModifierStackEntry>& GetModifierStack() const { return ModifierStack; }

	/** Returns the Modifier in the Stack with the given index if it exist. */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamModifier* GetModifierByIndex(const int32 Index) const;
	/** Tries to find a Modifier in the Stack with the given name. The returned Modifier must be checked before it is used. */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamModifier* GetModifierByName(const FName Name) const;
	/** Given a specific Modifier class, returns a list of matching Modifiers */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ModifierClass", DynamicOutputParam = "FoundModifiers"))
	void GetModifiersByClass(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifier> ModifierClass, TArray<UVCamModifier*>& FoundModifiers) const;
	/** Given a specific Interface class, returns a list of matching Modifiers */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	void GetModifiersByInterface(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UInterface> InterfaceClass, TArray<UVCamModifier*>& FoundModifiers) const;

	/**
	 * Sets the Modifier Context to a new instance of the provided class
	 * @param ContextClass The Class to create the context from
	 * @param CreatedContext The created Context, can be invalid if Context Class was None
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ContextClass", DynamicOutputParam = "CreatedContext", AllowAbstract = "false"))
	void SetModifierContextClass(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifierContext> ContextClass, UVCamModifierContext*& CreatedContext);
	
	/**
	 * Get the current Modifier Context
	 * @return Current Context
	 */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamModifierContext* GetModifierContext() const;
	
	
	/******************** Output Provider access ********************/

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ProviderClass", DynamicOutputParam = "CreatedProvider", ReturnDisplayName = "Success"))
	bool AddOutputProvider(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamOutputProviderBase> ProviderClass, UVCamOutputProviderBase*& CreatedProvider);
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ProviderClass", DynamicOutputParam = "CreatedProvider", ReturnDisplayName = "Success"))
	bool InsertOutputProvider(int32 Index, UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamOutputProviderBase> ProviderClass, UVCamOutputProviderBase*& CreatedProvider);
	// Moves an existing Output Provider in the stack from its current index to a new index
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (ReturnDisplayName = "Success"))
	bool SetOutputProviderIndex(int32 OriginalIndex, int32 NewIndex);
	
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void RemoveAllOutputProviders();
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	bool RemoveOutputProvider(const UVCamOutputProviderBase* Provider);
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	bool RemoveOutputProviderByIndex(const int32 ProviderIndex);
	
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	int32 GetNumberOfOutputProviders() const;
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	void GetAllOutputProviders(TArray<UVCamOutputProviderBase*>& Providers) const;

	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamOutputProviderBase* GetOutputProviderByIndex(const int32 ProviderIndex) const;
	UFUNCTION(BlueprintPure, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ProviderClass", DynamicOutputParam = "FoundProviders"))
	void GetOutputProvidersByClass(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamOutputProviderBase> ProviderClass, TArray<UVCamOutputProviderBase*>& FoundProviders) const;

	/** Call this after modifying the InputProfile in code to update the player mapped keys */
	void ApplyInputProfile();

	/** @return Whether LiveLinkData has a valid result. */
	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	bool GetLiveLinkDataForCurrentFrame(FLiveLinkCameraBlueprintData& LiveLinkData);

	/**
	 * Registers the given object with the VCamComponent's Input Component
	 * This allows dynamic input bindings such as input events in blueprints to work correctly
	 * Note: Ensure you call UnregisterObjectForInput when you are finished with the object
	 * otherwise input events will still fire until GC actually destroys the object
	 *
	 * @param Object The object to register
	 */
	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	void RegisterObjectForInput(UObject* Object);

	/**
	 * Unregisters the given object with the VCamComponent's Input Component
	 *
	 * @param Object The object to unregister
	 */
	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	void UnregisterObjectForInput(UObject* Object) const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * Returns a list of all player mappable keys that have been registered
	 */
	UE_DEPRECATED(5.3, "GetPlayerMappableKeys has been deprecated. Please use UEnhancedPlayerMappableKeyProfile::GetPlayerMappingRows instead.")
	UFUNCTION(BlueprintCallable, Category="VirtualCamera", meta=(DeprecatedFunction, DeprecationMessage="GetPlayerMappableKeys has been deprecated. Please use UEnhancedPlayerMappableKeyProfile::GetPlayerMappingRows instead."))
	TArray<FEnhancedActionKeyMapping> GetPlayerMappableKeys() const;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	/**
	 * Injects an input action. 
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", meta = (AutoCreateRefTerm = "Modifiers,Triggers"))
	virtual void InjectInputForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);
	
	/**
	 * Injects an input vector for action.
	 */
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", meta = (AutoCreateRefTerm = "Modifiers,Triggers"))
	virtual void InjectInputVectorForAction(const UInputAction* Action, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);

	
	/******************** Blueprint Getters & Setters ********************/


	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	FGameplayTag GetRole() const { return Role; }
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetRole(FGameplayTag Value) { Role = Value; }
	
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	FLiveLinkSubjectName GetLiveLinkSubobject() const { return LiveLinkSubject; }
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetLiveLinkSubobject(FLiveLinkSubjectName Value) { LiveLinkSubject = Value; }
	
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	bool GetDisableComponentWhenSpawnedBySequencer() const { return bDisableComponentWhenSpawnedBySequencer; }
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetDisableComponentWhenSpawnedBySequencer(bool bValue) { bDisableComponentWhenSpawnedBySequencer = bValue; }
	
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	bool GetDisableOutputOnMultiUserReceiver() const { return bDisableOutputOnMultiUserReceiver; }
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetDisableOutputOnMultiUserReceiver(bool bValue) { bDisableOutputOnMultiUserReceiver = bValue; }
	
	UFUNCTION(BlueprintPure, Category = "VCam Input")
	const FVCamInputProfile& GetInputProfile() const { return InputProfile; }
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category="VirtualCamera")
	void SetInputProfile(const FVCamInputProfile& NewInputProfile);

	/** Gets the current input device settings being used (if this component is enabled) or that will be used (if not enabled). */
	UFUNCTION(BlueprintPure, Category = "VCam Input")
	const FVCamInputDeviceConfig& GetInputDeviceSettings() const;
	/** Propagates devices settings all the way to the player input. Causes input to be filtered / consumed differently. */
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category="VirtualCamera")
	void SetInputDeviceSettings(const FVCamInputDeviceConfig& NewInputProfile);

	
	/******************** Subsystem Queries ********************/

	/** Gets all subsystems implementing this interface */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	TArray<UVCamSubsystem*> GetSubsystemArray(const TSubclassOf<UVCamSubsystem>& Class) const;

	/** Gets the subsystem responsible for input handling. */
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UInputVCamSubsystem* GetInputVCamSubsystem() const;
	
	template <typename TSubsystemClass>
	TSubsystemClass* GetSubsystem(const TSubclassOf<TSubsystemClass>& SubsystemClass) const { return SubsystemCollection.GetSubsystem(SubsystemClass); }
	
	template <typename TSubsystemClass>
	const TArray<TSubsystemClass*>& GetSubsystemArray(const TSubclassOf<TSubsystemClass>& SubsystemClass) const { return SubsystemCollection.GetSubsystemArray(SubsystemClass); }


	/******************** Misc ********************/
	
	/** Updates all actor Locks on viewports to be as configured. */
	void UpdateActorViewportLocks();

	/** Whether this component is initialized, i.e. the input subsystem is set up and the output providers are allowed to be active. */
	bool IsInitialized() const { return bIsInitialized; }
	
private:
	
	/** Whether the VCamComponent will update every frame */
	UPROPERTY(EditAnywhere, BlueprintSetter = SetEnabled, BlueprintGetter = IsEnabled, Category = "VirtualCamera", meta = (DisplayPriority = "1"))
	bool bEnabled = true;
	
	/**
	 * The role of this virtual camera.  If this value is set and the corresponding tag set on the editor matches this value, then this
	 * camera is the sender and the authority in the case when connected to a multi-user session.
	 */
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetRole", BlueprintSetter = "SetRole", Category = "VirtualCamera", meta = (DisplayPriority = "2"))
	FGameplayTag Role;

	/** LiveLink subject name for the incoming camera transform */
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetLiveLinkSubobject", BlueprintSetter = "SetLiveLinkSubobject", Category="VirtualCamera", meta = (DisplayPriority = "3"))
	FLiveLinkSubjectName LiveLinkSubject;
	
	/**
	 * If true, render the viewport from the point of view of the parented CineCamera
	 * 
	 * This was moved to UVCamOutputProvider::TargetViewport.
	 * See FVCamCoreCustomVersion::MoveTargetViewportFromComponentToOutput.
	 */
	UPROPERTY()
	bool bLockViewportToCamera_DEPRECATED = false;
	
	/** Sync with output providers keeping track of which viewports are locked. */
	UPROPERTY(EditAnywhere, Category = "VirtualCamera", meta = (DisplayPriority = "4"))
	FVCamViewportLocker ViewportLocker;

	/** If true, the component will force bEnabled to false when it is part of a spawnable in Sequencer */
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetDisableComponentWhenSpawnedBySequencer", BlueprintSetter = "SetDisableComponentWhenSpawnedBySequencer", Category = "VirtualCamera", meta = (AllowPrivateAccess = "true", DisplayPriority = "5"))
	bool bDisableComponentWhenSpawnedBySequencer = true;

	/** Do we disable the output if the virtual camera is in a Multi-user session and the camera is a "receiver" from multi-user */
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetDisableOutputOnMultiUserReceiver", BlueprintSetter = "SetDisableOutputOnMultiUserReceiver", AdvancedDisplay, Category = "VirtualCamera", meta = (DisplayPriority = "6"))
	bool bDisableOutputOnMultiUserReceiver = true;

	/**
	 * Indicates the frequency which camera updates are sent when in Multi-user mode. This has a minimum value of
	 * 11ms. Using values below 30ms is discouraged. When higher refresh rates are needed consider using LiveLink
	 * rebroadcast to stream camera data.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="VirtualCamera", meta=(ForceUnits=ms, ClampMin = "11.0"), DisplayName="Update Frequencey")
	float UpdateFrequencyMs = 66.6f;

public:
	
	/**
	 * Which viewport to use for this VCam.
	 * 
	 * This was moved to UVCamOutputProvider::TargetViewport.
	 * See FVCamCoreCustomVersion::MoveTargetViewportFromComponentToOutput.
	 */
	UPROPERTY()
	EVCamTargetViewportID TargetViewport_DEPRECATED = EVCamTargetViewportID::Viewport1;

private:
	
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetInputProfile", BlueprintSetter = "SetInputProfile", Category = "VirtualCamera")
	FVCamInputProfile InputProfile;

	UPROPERTY(EditAnywhere, BlueprintGetter = "GetInputDeviceSettings", BlueprintSetter = "SetInputDeviceSettings", Category = "VirtualCamera")
	FVCamInputDeviceConfig InputDeviceSettings;

	/** List of Output Providers (executed in order) */
	UPROPERTY(EditAnywhere, Instanced, Category = "VirtualCamera")
	TArray<TObjectPtr<UVCamOutputProviderBase>> OutputProviders;
	
	/** Modifier Context object that can be accessed by the Modifier Stack */
	UPROPERTY(EditAnywhere, Instanced, Category = "VirtualCamera")
	TObjectPtr<UVCamModifierContext> ModifierContext;

	/** List of Modifiers (executed in order) */
	UPROPERTY(EditAnywhere, Category = "VirtualCamera")
	TArray<FModifierStackEntry> ModifierStack;
	
	/**
	 * From Ben H: Mark this as Transient/DuplicateTransient so that it is saved on the BP CDO and nowhere else and 
	 * handled correctly during duplication operations (copy/paste etc)
	 */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UInputComponent> InputComponent;

	/** Store the Input Mapping Contexts that have been added via this component */
	UPROPERTY(Transient, DuplicateTransient)
	TArray<TObjectPtr<UInputMappingContext>> AppliedInputContexts;
	
	double LastEvaluationTime;

	TArray<UVCamOutputProviderBase*> SavedOutputProviders;
	TArray<FModifierStackEntry> SavedModifierStack;

#if WITH_EDITOR
	/** Variable used for pausing update on editor objects while PIE is running */
	enum class EPIEState
	{
		/** Default state when no PIE transition is needed. */
		Normal,
		/** We de-initialized this VCam before starting PIE because it was initialized. Initialize this VCam when PIE ends. */
		WasInitializedBeforePIE,
		/** This VCam was not initialized when PIE was started hence it need to be re-initialized when PIE ends. */
		WasNotInitializedBeforePIE,
	} PIEMode = EPIEState::Normal;
#endif

	/** Initialize and deinitialize calls match our  */
	FObjectSubsystemCollection<UVCamSubsystem> SubsystemCollection;

	/** Whether Initialize was called but not Deinitialize yet. */
	bool bIsInitialized = false;

	/**
	 * Creates the InputComponent and binds global delegates.
	 * It is safe to call this multiple times.
	 */
	void SetupVCamSystemsIfNeeded();
	void CleanupRegisteredDelegates();

	/** Calls Initialize if not already initialized and this component is enabled. */
	void EnsureInitializedIfAllowed();
	/** Initializes the input system, modifiers, output providers, and locks the viewport if needed. */
	virtual void Initialize();
	/** De-initializes all systems initialized in Initialize(). */
	virtual void Deinitialize();

	/** Called as part of applying component instance data */
	void ReinitializeInput(TArray<TObjectPtr<UInputMappingContext>> InputContextsToReapply);

	void SyncInputSettings();
	
	void TickModifierStack(float DeltaTime);
	void TickOutputProviders(float DeltaTime);
	void TickSubsystems(float DeltaTime);
	
	static void CopyLiveLinkDataToCamera(const FLiveLinkCameraBlueprintData& LiveLinkData, UCineCameraComponent* CameraComponent);

	float GetDeltaTime();
	void UnlockAllViewports();
	void DestroyOutputProvider(UVCamOutputProviderBase* Provider);

	// Use the Saved Modifier Stack from PreEditChange to find the modified entry and then ensure the modified entry's name is unique
	// If a new modifier has been created then its name will be defaulted to BaseName
	void ValidateModifierStack(const FString BaseName = "NewModifier");
	bool DoesNameExistInSavedStack(const FName InName) const;
	void FindModifiedStackEntry(int32& ModifiedStackIndex, bool& bIsNewEntry) const;

#if WITH_EDITOR
	void OnMapChanged(UWorld* World, EMapChangeType ChangeType);

	void OnBeginPIE(const bool bInIsSimulating);
	void OnEndPIE(const bool bInIsSimulating);

	// Multi-user support
	void HandleCameraComponentEventData(const FConcertSessionContext& InEventContext, const FMultiUserVCamCameraComponentEvent& InEvent);

	void SessionStartup(TSharedRef<IConcertClientSession> InSession);
	void SessionShutdown(TSharedRef<IConcertClientSession> InSession);

	FString GetNameForMultiUser() const;

	void MultiUserStartup();
	void MultiUserShutdown();

	/** Delegate handle for a the callback when a session starts up */
	FDelegateHandle OnSessionStartupHandle;

	/** Delegate handle for a the callback when a session shuts down */
	FDelegateHandle OnSessionShutdownHandle;

	/** Weak pointer to the client session with which to send events. May be null or stale. */
	TWeakPtr<IConcertClientSession> WeakSession;

	double SecondsSinceLastLocationUpdate = 0;
	double PreviousUpdateTime = 0;
#endif
	
	/** Send the current camera state via Multi-user if connected and in a */
	void SendCameraDataViaMultiUser();
	
	/** Can return false if we're in a multi-user session and the user configured us not to update. */
	bool ShouldUpdateOutputProviders() const;
	/** Can return false if we're in a multi-user session and the user configured us not to update. */
	bool ShouldEvaluateModifierStack() const;
	bool IsMultiUserSession() const;
	bool IsCameraInVPRole() const;

	/** Detect when this instance replaces an old instance and calls NotifyComponentWasReplaced on the old instance. */
	void HandleObjectReplaced(const TMap<UObject*, UObject*>& ReplacementMap);
	/** When another component replaces us, get a notification so we can clean up */
	void NotifyComponentWasReplaced(UVCamComponent* ReplacementComponent);

	/** Utility functions for registering and unregistering our input component with the correct input system */
	virtual void RegisterInputComponent();
	virtual void UnregisterInputComponent();

	/** Called after an undo, or applying instance cache. Calls Initialize or Deinitialize if needed based on bEnabled. */
	void RefreshInitializationState();

	// Special support for undoing placement of Blueprint created VCam
#if WITH_EDITOR
	/** Removes the transient asset users data so it is not saved into the map. */
	void OnPreSaveWorld(UWorld* World, FObjectPreSaveContext ObjectPreSaveContext);
	/** Adds back the transient asset users data that was removed in OnPreSaveWorld. */
	void OnPostSaveWorld(UWorld* World, FObjectPostSaveContext ObjectPostSaveContext);

	/** Adds UVCamBlueprintAssetUserData to this component if it was created by Blueprints. Allows detection of undoing placing the owning actor. */
	void AddAssetUserDataConditionally();
	void RemoveAssetUserData();

	/** Deinitializes this VCam if the owning actor was removed by an undo operation. */
	void OnAssetUserDataPostEditUndo();
#endif
};
