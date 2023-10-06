// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "EVCamTargetViewportID.h"
#include "UI/WidgetSnapshots.h"
#include "Widgets/VPFullScreenUserWidget.h"
#include "VCamOutputProviderBase.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamOutputProvider, Log, All);

class APlayerController;
class FSceneViewport;
class SWindow;
class UGameplayViewTargetPolicy;
class UUserWidget;
class UVCamWidget;
class UVPFullScreenUserWidget;

#if WITH_EDITOR
struct FEditorViewportViewModifierParams;
struct FSceneViewExtensionContext;
class FLevelEditorViewportClient;
class ISceneViewExtension;
#endif

UCLASS(Abstract, BlueprintType, EditInlineNew)
class VCAMCORE_API UVCamOutputProviderBase : public UObject
{
	GENERATED_BODY()
public:

	DECLARE_MULTICAST_DELEGATE_OneParam(FActivationDelegate, bool /*bNewIsActive*/);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActivationDelegate_Blueprint, bool, bNewIsActive);
	FActivationDelegate OnActivatedDelegate; 
	/** Called when the activation state of this output provider changes. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "OnActivated"))
	FActivationDelegate_Blueprint OnActivatedDelegate_Blueprint;

	/** Override the default output resolution with a custom value - NOTE you must toggle bIsActive off then back on for this to take effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "5"))
	bool bUseOverrideResolution = false;

	/** When bUseOverrideResolution is set, use this custom resolution */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "6"), meta = (EditCondition = "bUseOverrideResolution", ClampMin = 1))
	FIntPoint OverrideResolution = { 2048, 1536 };

	UVCamOutputProviderBase();
	
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	/**
	 * Called when the provider is brought online such as after instantiating or loading a component containing this provider
	 * Use Initialize for any setup logic that needs to survive between Start / Stop cycles such as spawning transient objects
	 */
	virtual void Initialize();
	/** Called when the provider is being shutdown such as before changing level or on exit */
	virtual void Deinitialize();
	
	/** Called when the provider is Activated */
	virtual void OnActivate();
	/** Called when the provider is Deactivated */
	virtual void OnDeactivate();
	
	/** Called to create the UMG overlay widget. */
	virtual void CreateUMG();
	
	virtual void Tick(const float DeltaTime);
	
	/** @return Whether this output provider should require the viewport to be locked to the camera in order to function correctly. */
	virtual bool NeedsForceLockToViewport() const;
	
	/** Temporarily disable the output.  Caller must eventually call RestoreOutput. */
	void SuspendOutput();
	/** Restore the output state from previous call to disable output. */
	void RestoreOutput();
	
	/** Calls the VCamModifierInterface on the widget if it exists and also requests any child VCam Widgets to reconnect */
	void NotifyAboutComponentChange();

	/** Called to turn on or off this output provider */
	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetActive(const bool bInActive);
	/** Returns if this output provider is currently active or not */
	UFUNCTION(BlueprintPure, Category = "Output")
	bool IsActive() const { return bIsActive; };

	/** Returns if this output provider has been initialized or not */
	UFUNCTION(BlueprintPure, Category = "Output")
	bool IsInitialized() const { return bInitialized; };

	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetTargetCamera(const UCineCameraComponent* InTargetCamera);

	UFUNCTION(BlueprintPure, Category = "Output")
	EVCamTargetViewportID GetTargetViewport() const { return TargetViewport; }
	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetTargetViewport(EVCamTargetViewportID Value);
	
	UFUNCTION(BlueprintPure, Category = "Output")
	TSubclassOf<UUserWidget> GetUMGClass() const { return UMGClass; }
	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetUMGClass(const TSubclassOf<UUserWidget> InUMGClass);

	UVPFullScreenUserWidget* GetUMGWidget() { return UMGWidget; };

	/** Utility that gets the owning VCam component and gets another output provider by its index. */
	UVCamOutputProviderBase* GetOtherOutputProviderByIndex(int32 Index) const;

	/** Gets the index of this output provider in the owning UVCamComponent::OutputProviders array. */
	int32 FindOwnIndexInOwner() const;

	/** Reapplies the override resolution or restores back to the viewport settings. */
	void ReapplyOverrideResolution();

	/** Gets the scene viewport identified by the currently configured TargetViewport. */
	TSharedPtr<FSceneViewport> GetTargetSceneViewport() const { return GetSceneViewport(TargetViewport); }
	/** Gets the viewport identified by the passed in parameters. */
	TSharedPtr<FSceneViewport> GetSceneViewport(EVCamTargetViewportID InTargetViewport) const;
	TWeakPtr<SWindow> GetTargetInputWindow() const;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	//~ End UObject Interface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
#endif
	
	static FName GetIsActivePropertyName()			{ return GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, bIsActive); }
	static FName GetTargetViewportPropertyName()	{ return GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, TargetViewport); }
	static FName GetUMGClassPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UVCamOutputProviderBase, UMGClass); }

protected:
	
	/** Defines how the overlay widget should be added to the viewport. This should set as early as possible: in the constructor. */
	UPROPERTY(Transient)
	EVPWidgetDisplayType DisplayType = EVPWidgetDisplayType::Inactive;
	
	/**
	 * In game worlds, such as PIE or shipped games, determines which a player controller whose view target should be set to the owning cine camera.
	 * 
	 * Note that multiple output providers may have a policy set and policies might choose the same player controllers to set the view target for.
	 * This conflict is resolved as follows: if a player controller already has the cine camera as view target, the policy is not used.
	 * Hence, you can order your output providers array in the VCamComponent. The first policies will get automatically get higher priority.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Output", meta = (DisplayPriority = "99"))
	TObjectPtr<UGameplayViewTargetPolicy> GameplayViewTargetPolicy;
	
	/** Removes the override resolution from the given viewport. */
	void RestoreOverrideResolutionForViewport(EVCamTargetViewportID ViewportToRestore);
	/** Applies OverrideResolution to the passed in viewport - bUseOverrideResolution was already checked. */
	void ApplyOverrideResolutionForViewport(EVCamTargetViewportID Viewport);
	void DisplayUMG();
	void DestroyUMG();

#if WITH_EDITOR
	FLevelEditorViewportClient* GetTargetLevelViewportClient() const;
	TSharedPtr<SLevelViewport> GetTargetLevelViewport() const;
#endif
	
	UVPFullScreenUserWidget* GetUMGWidget() const { return UMGWidget; }
	
private:
	
	/** If set, this output provider will execute every frame */
	UPROPERTY(EditAnywhere, BlueprintGetter = "IsActive", BlueprintSetter = "SetActive", Category = "Output", meta = (DisplayPriority = "1"))
	bool bIsActive = false;

	/** Which viewport to use for this VCam */
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetTargetViewport", BlueprintSetter = "SetTargetViewport", Category = "Output", meta = (DisplayPriority = "2"))
	EVCamTargetViewportID TargetViewport = EVCamTargetViewportID::Viewport1;
	
	/** The UMG class to be rendered in this output provider */
	UPROPERTY(EditAnywhere, BlueprintGetter = "GetUMGClass", BlueprintSetter = "SetUMGClass", Category = "Output", meta = (DisplayName="UMG Overlay", DisplayPriority = "3"))
	TSubclassOf<UUserWidget> UMGClass;
	
	/** FOutputProviderLayoutCustomization allows remapping connections and their bound widgets. This is used to persist those overrides since UUserWidgets cannot be saved. */
	UPROPERTY()
	FWidgetTreeSnapshot WidgetSnapshot;
	
	UPROPERTY(Transient)
	bool bInitialized = false;

	/** Valid when active and if UMGClass is valid. */
	UPROPERTY(Transient)
	TObjectPtr<UVPFullScreenUserWidget> UMGWidget = nullptr;

#if WITH_EDITORONLY_DATA
	/** We call UVPFullScreenUserWidget::SetCustomPostProcessSettingsSource(this), which will cause these settings to be discovered. They are later passed down to FEditorViewportViewModifierDelegate. */
	UPROPERTY(Transient)
	FPostProcessSettings PostProcessSettingsForWidget;
	/** Handle to ModifyViewportPostProcessSettings */
	FDelegateHandle ModifyViewportPostProcessSettingsDelegateHandle;
#endif
	
	UPROPERTY(Transient)
	TSoftObjectPtr<UCineCameraComponent> TargetCamera;

	/** SuspendOutput can disable output while we're active. This flag indicates whether we should reactivate when RestoreOutput is called. */
	UPROPERTY(Transient)
	bool bWasOutputSuspendedWhileActive = false;

	/** If in a game world, these player controllers must have their view targets reverted when this output provider is deactivated. */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<APlayerController>> PlayersWhoseViewTargetsWereSet; 

	bool IsActiveAndOuterComponentEnabled() const { return bIsActive && IsOuterComponentEnabled(); }
	bool IsOuterComponentEnabled() const;

#if WITH_EDITOR
	/** Passed to FEditorViewportClient::ViewModifiers whenever DisplayType == EVPWidgetDisplayType::PostProcessWithBlendMaterial. */
	void ModifyViewportPostProcessSettings(FEditorViewportViewModifierParams& EditorViewportViewModifierParams);
	/** Callback when DisplayType == EVPWidgetDisplayType::PostProcessSceneViewExtension that decides whether a given viewport should be rendered to. */
	TOptional<bool> GetRenderWidgetStateInContext(const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context);
	
	void StartDetectAndSnapshotWhenConnectionsChange();
	void StopDetectAndSnapshotWhenConnectionsChange();
	void OnConnectionReinitialized(TWeakObjectPtr<UVCamWidget> Widget);
#endif

	void ConditionallySetUpGameplayViewTargets();
	void CleanUpGameplayViewTargets();
};
