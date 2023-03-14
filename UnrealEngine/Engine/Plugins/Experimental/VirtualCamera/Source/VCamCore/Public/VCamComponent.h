// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystemInterface.h"
#include "VCamTypes.h"
#include "VCamInputSettings.h"
#include "Roles/LiveLinkCameraTypes.h"
#include "VCamOutputProviderBase.h"
#include "GameplayTagContainer.h"

#if WITH_EDITOR
#include "UnrealEdMisc.h"
#include "VCamMultiUser.h"
#endif

#include "VCamComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamComponent, Log, All);

class UCineCameraComponent;
class UVCamModifierContext;
class SWindow;
class FSceneViewport;
class UEnhancedInputComponent;

#if WITH_EDITOR
class FLevelEditorViewportClient;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnComponentReplaced, UVCamComponent*, NewComponent);

UENUM(BlueprintType, meta=(DisplayName = "VCam Target Viewport ID"))
enum class EVCamTargetViewportID : uint8
{
	CurrentlySelected = 0,
	Viewport1 = 1,
	Viewport2 = 2,
	Viewport3 = 3,
	Viewport4 = 4
};

UCLASS(Blueprintable, ClassGroup=(VCam), meta=(BlueprintSpawnableComponent))
class VCAMCORE_API UVCamComponent : public USceneComponent
{
	GENERATED_BODY()

	friend class UVCamModifier;

public:
	UVCamComponent();

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	virtual void OnAttachmentChanged() override;

	virtual void PostLoad() override;

	TSharedPtr<FSceneViewport> GetTargetSceneViewport() const;
	TWeakPtr<SWindow> GetTargetInputWindow() const;

#if WITH_EDITOR
	FLevelEditorViewportClient* GetTargetLevelViewportClient() const;
	TSharedPtr<SLevelViewport> GetTargetLevelViewport() const;

	virtual void CheckForErrors() override;
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	// There are situations in the editor where the component may be replaced by another component as part of the actor being reconstructed
	// This event will notify you of that change and give you a reference to the new component. 
	// Bindings will be copied to the new component so you do not need to rebind this event
	// 
	// Note: When the component is replaced you will need to get all properties on the component again such as Modifiers and Output Providers
	UPROPERTY(BlueprintAssignable, Category = "VirtualCamera")
	FOnComponentReplaced OnComponentReplaced;

	UFUNCTION()
	void HandleObjectReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

	bool CanUpdate() const;
	
	void Update();

	// Adds the Input Mapping Context from a modifier, if it exists, to the input system 
	void AddInputMappingContext(const UVCamModifier* Modifier);

	// Removes the Input Mapping Context from a modifier, if it exists, from the input system
	void RemoveInputMappingContext(const UVCamModifier* Modifier);

	// Adds an explicitly provided Input Mapping Context to the input system
	void AddInputMappingContext(UInputMappingContext* Context, int32 Priority);

	// Removes an explicitly provided Input Mapping Context to the input system
	void RemoveInputMappingContext(UInputMappingContext* Context);

	// Attempts to apply key mapping settings from an input profile defined in VCam Input Settings
	// Returns whether the profile was found and able to be applied
	UFUNCTION(BlueprintCallable, Category = "VCam Input")
	bool SetInputProfileFromName(const FName ProfileName);

	// Tries to add a new Input Profile to the VCam Input Settings and populates it with any currently active player mappable keys
	// Note: The set of currently active player mappable keys may be larger than the set of mappings in this component's Input Profile
	//
	// Optionally this function can update an existing profile, this will only add any mappable keys that don't currently exist in the profile but will not remove any existing mappings
	// Returns whether the profile was successfully added or updated
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

	// Sets if the VCamComponent will update every frame or not
	UFUNCTION(BlueprintSetter)
	void SetEnabled(bool bNewEnabled);

	// Returns whether or not the VCamComponent will update every frame
	UFUNCTION(BlueprintGetter)
	bool IsEnabled() const { return bEnabled; };

	// Returns the Target CineCameraComponent
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UCineCameraComponent* GetTargetCamera() const;

	// Add a modifier to the stack with a given name.
	// If that name is already in use then the modifier will not be added.
	// Returns the created modifier if the Add succeeded
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ModifierClass", DynamicOutputParam = "CreatedModifier", ReturnDisplayName = "Success"))
	bool AddModifier(const FName Name, UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifier> ModifierClass, UVCamModifier*& CreatedModifier);

	// Insert a modifier to the stack with a given name and index.
	// If that name is already in use then the modifier will not be added.
	// The index must be between zero and the number of existing modifiers inclusive
	// Returns the created modifier if the Add succeeded
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ModifierClass", DynamicOutputParam = "CreatedModifier", ReturnDisplayName = "Success"))
	bool InsertModifier(const FName Name, int32 Index, UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifier> ModifierClass, UVCamModifier*& CreatedModifier);

	// Moves an existing modifier in the stack from its current index to a new index
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (ReturnDisplayName = "Success"))
	bool SetModifierIndex(int32 OriginalIndex, int32 NewIndex);

	// Remove all Modifiers from the Stack.
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void RemoveAllModifiers();

	// Remove the given Modifier from the Stack.
	// Returns true if the modifier was removed successfully
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
    bool RemoveModifier(const UVCamModifier* Modifier);

	// Remove the Modifier at a specified index from the Stack.
	// Returns true if the modifier was removed successfully
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	bool RemoveModifierByIndex(const int ModifierIndex);

	// Remove the Modifier with a specific name from the Stack.
	// Returns true if the modifier was removed successfully
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
    bool RemoveModifierByName(const FName Name);

	// Returns the number of Modifiers in the Component's Stack
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	int32 GetNumberOfModifiers() const;

	// Returns all the Modifiers in the Component's Stack
	// Note: It's possible not all Modifiers will be valid (such as if the user has not set a class for the modifier in the details panel)
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	void GetAllModifiers(TArray<UVCamModifier*>& Modifiers) const;

	// Returns the Modifier in the Stack with the given index if it exist.
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamModifier* GetModifierByIndex(const int32 Index) const;

	// Tries to find a Modifier in the Stack with the given name.
	// The returned Modifier must be checked before it is used.
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamModifier* GetModifierByName(const FName Name) const;

	// Given a specific Modifier class, returns a list of matching Modifiers
	UFUNCTION(BlueprintPure, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ModifierClass", DynamicOutputParam = "FoundModifiers"))
	void GetModifiersByClass(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifier> ModifierClass, TArray<UVCamModifier*>& FoundModifiers) const;

	// Given a specific Interface class, returns a list of matching Modifiers
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	void GetModifiersByInterface(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UInterface> InterfaceClass, TArray<UVCamModifier*>& FoundModifiers) const;

	/*
	Sets the Modifier Context to a new instance of the provided class
	@param ContextClass The Class to create the context from
	@param CreatedContext The created Context, can be invalid if Context Class was None
	*/
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ContextClass", DynamicOutputParam = "CreatedContext", AllowAbstract = "false"))
	void SetModifierContextClass(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamModifierContext> ContextClass, UVCamModifierContext*& CreatedContext);
	
	/*
	Get the current Modifier Context
	@return Current Context
	*/
	UFUNCTION(BlueprintPure, Category = "VirtualCamera")
	UVCamModifierContext* GetModifierContext() const;

	// Output Provider access

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ProviderClass", DynamicOutputParam = "CreatedProvider", ReturnDisplayName = "Success"))
	bool AddOutputProvider(UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamOutputProviderBase> ProviderClass, UVCamOutputProviderBase*& CreatedProvider);

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (DeterminesOutputType = "ProviderClass", DynamicOutputParam = "CreatedProvider", ReturnDisplayName = "Success"))
	bool InsertOutputProvider(int32 Index, UPARAM(meta = (AllowAbstract = "false")) TSubclassOf<UVCamOutputProviderBase> ProviderClass, UVCamOutputProviderBase*& CreatedProvider);

	// Moves an existing Output Provider in the stack from its current index to a new index
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", Meta = (ReturnDisplayName = "Success"))
	bool SetOutputProviderIndex(int32 OriginalIndex, int32 NewIndex);

	// Remove all Output Providers from the Component.
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

private:
	// Enabled state of the component
	UPROPERTY(EditAnywhere, BlueprintSetter = SetEnabled, BlueprintGetter = IsEnabled, Category = "VirtualCamera")
	bool bEnabled = true;

public:
	/**
	 * The role of this virtual camera.  If this value is set and the corresponding tag set on the editor matches this value, then this
	 * camera is the sender and the authority in the case when connected to a multi-user session.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VirtualCamera")
	FGameplayTag Role;

	// LiveLink subject name for the incoming camera transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualCamera")
	FLiveLinkSubjectName LiveLinkSubject;

	// If true, render the viewport from the point of view of the parented CineCamera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualCamera")
	bool bLockViewportToCamera = false;

	// If true, the component will force bEnabled to false when it is part of a spawnable in Sequencer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	bool bDisableComponentWhenSpawnedBySequencer = true;

	/** Do we disable the output if the virtual camera is in a Multi-user session and the camera is a "receiver" from multi-user */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "VirtualCamera")
	bool bDisableOutputOnMultiUserReceiver = true;

	/**
	 * Indicates the frequency which camera updates are sent when in Multi-user mode. This has a minimum value of
	 * 11ms. Using values below 30ms is discouraged. When higher refresh rates are needed consider using LiveLink
	 * rebroadcast to stream camera data.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="VirtualCamera", meta=(ForceUnits=ms, ClampMin = "11.0"), DisplayName="Update Frequencey")
	float UpdateFrequencyMs = 66.6f;

	// Which viewport to use for this VCam
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualCamera")
	EVCamTargetViewportID TargetViewport = EVCamTargetViewportID::CurrentlySelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetInputProfile, Category = "VirtualCamera")
	FVCamInputProfile InputProfile;

	// Call this after modifying the InputProfile in code to update the player mapped keys
	void ApplyInputProfile();
	
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly, Category="VirtualCamera")
	void SetInputProfile(const FVCamInputProfile& NewInputProfile);

	// List of Output Providers (executed in order)
	UPROPERTY(EditAnywhere, Instanced, Category="VirtualCamera")
	TArray<TObjectPtr<UVCamOutputProviderBase>> OutputProviders;

	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	void GetLiveLinkDataForCurrentFrame(FLiveLinkCameraBlueprintData& LiveLinkData);

	/* Registers the given object with the VCamComponent's Input Component
	 * This allows dynamic input bindings such as input events in blueprints to work correctly
	 * Note: Ensure you call UnregisterObjectForInput when you are finished with the object
	 * otherwise input events will still fire until GC actually destroys the object
	 *
	 * @param Object The object to register
	 */
	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	void RegisterObjectForInput(UObject* Object);

	/* Unregisters the given object with the VCamComponent's Input Component
	 *
	 * @param Object The object to unregister
	 */
	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	void UnregisterObjectForInput(UObject* Object) const;

	/**
	 * Returns a list of all player mappable keys that have been registered
	 */
	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	TArray<FEnhancedActionKeyMapping> GetPlayerMappableKeys() const;

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

private:	
	static void CopyLiveLinkDataToCamera(const FLiveLinkCameraBlueprintData& LiveLinkData, UCineCameraComponent* CameraComponent);

	float GetDeltaTime();
	void SetActorLock(bool bNewActorLock) { bLockViewportToCamera = bNewActorLock; UpdateActorLock(); }
	void UpdateActorLock();
	void DestroyOutputProvider(UVCamOutputProviderBase* Provider);
	void ResetAllOutputProviders();

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

	/** Is the camera currently in a role assigned to the session. */
	bool IsCameraInVPRole() const;

	/** Send the current camera state via Multi-user if connected and in a */
	void SendCameraDataViaMultiUser();

	/** Are we in a multi-user session. */
	bool IsMultiUserSession() const;

	/** Can the modifier stack be evaluated. */
	bool CanEvaluateModifierStack() const;

	/** Output Providers should only update outside of MU or if we have the correct VP Role */
	bool ShouldUpdateOutputProviders() const;

	// When another component replaces us, get a notification so we can clean up
	void NotifyComponentWasReplaced(UVCamComponent* ReplacementComponent);

	/*
	 * Gets the input interface for the currently active input method.
	 * Will switch between editor and player based input as needed
	 */
	IEnhancedInputSubsystemInterface* GetEnhancedInputSubsystemInterface() const;

	double LastEvaluationTime;

	TWeakObjectPtr<AActor> Backup_ActorLock;
	TWeakObjectPtr<AActor> Backup_ViewTarget;

	TArray<UVCamOutputProviderBase*> SavedOutputProviders;
	TArray<FModifierStackEntry> SavedModifierStack;

	// Modifier Context object that can be accessed by the Modifier Stack
	UPROPERTY(EditAnywhere, Instanced, Category = "VirtualCamera")
	TObjectPtr<UVCamModifierContext> ModifierContext;

	// List of Modifiers (executed in order)
	UPROPERTY(EditAnywhere, Category = "VirtualCamera")
	TArray<FModifierStackEntry> ModifierStack;

	// Variable used for pausing update on editor objects while PIE is running
	bool bIsEditorObjectButPIEIsRunning = false;

	UPROPERTY(Transient)
	bool bIsLockedToViewport = false;

	// From Ben H: Mark this as Transient/DuplicateTransient so that it is saved on the BP CDO and nowhere else and
	// handled correctly during duplication operations (copy/paste etc)
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UInputComponent> InputComponent;

	// Utility functions for registering and unregistering our input component with the correct input system
	void RegisterInputComponent();
	void UnregisterInputComponent();

	// Store the Input Mapping Contexts that have been added via this component
	UPROPERTY(Transient, DuplicateTransient)
	TArray<TObjectPtr<const UInputMappingContext>> AppliedInputContexts;

	bool bIsInputRegistered = false;
};
