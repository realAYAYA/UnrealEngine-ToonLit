// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "CameraCalibrationTypes.h"
#include "LensFile.h"
#include "LiveLinkComponentController.h"

#include "LensComponent.generated.h"

class UCineCameraComponent;

/** Mode that controls where FIZ inputs are sourced from and how they are used to evaluate the LensFile */
UENUM(BlueprintType)
enum class EFIZEvaluationMode : uint8
{
	/** Evaluate the Lens File with the latest FIZ data received from LiveLink */
	UseLiveLink,
	/** Evaluate the Lens File using the current FIZ settings of the target camera */
	UseCameraSettings,
	/** Evaluate the Lens File using values recorded in a level sequence (set automatically when the sequence is opened) */
	UseRecordedValues,
	/** Do not evaluate the Lens File */
	DoNotEvaluate,
};

/** Controls whether this component can override the camera's filmback, and if so, which override to use */
UENUM(BlueprintType)
enum class EFilmbackOverrideSource : uint8
{
	/** Override the camera's filmback using the sensor dimensions recorded in the LensInfo of the LensFile */
	LensFile,
	/** Override the camera's filmback using the CroppedFilmback setting below */
	CroppedFilmbackSetting,
	/** Do not override the camera's filmback */
	DoNotOverride,
};

/** Specifies from where the distortion state information comes */
UENUM(BlueprintType)
enum class EDistortionSource : uint8
{
	/** Distortion state is evaluated using the LensFile */
	LensFile,
	/** Distortion state is inputted directly from a LiveLink subject */
	LiveLinkLensSubject,
	/** Distortion state is set manually by the user using the Distortion State setting below */
	Manual,
};

/** Component for applying a post-process lens distortion effect to a CineCameraComponent on the same actor */
UCLASS(HideCategories=(Tags, Activation, Cooking, AssetUserData, Collision), meta=(BlueprintSpawnableComponent))
class CAMERACALIBRATIONCORE_API ULensComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULensComponent();

	//~ Begin UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	//~ End UActorComponent interface

	//~ Begin UObject interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~ End UObject interface

	/** Get the LensFile picker used by this component */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	FLensFilePicker GetLensFilePicker() const;

	/** Get the LensFile used by this component */
	UFUNCTION(BlueprintPure, Category="Lens Component")
	ULensFile* GetLensFile() const;

	/** Set the LensFile picker used by this component */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetLensFilePicker(FLensFilePicker LensFile);

	/** Set the LensFile used by this component */
	UFUNCTION(BlueprintCallable, Category="Lens Component")
	void SetLensFile(ULensFile* LensFile);

	/** Get the evaluation mode used to evaluate the LensFile */
	UFUNCTION(BlueprintPure, Category="Lens Component", meta = (DisplayName = "Get FIZ Evaluation Mode"))
	EFIZEvaluationMode GetFIZEvaluationMode() const;

	/** Set the evaluation mode used to evaluate the LensFile */
	UFUNCTION(BlueprintCallable, Category="Lens Component", meta=(DisplayName="Set FIZ Evaluation Mode"))
	void SetFIZEvaluationMode(EFIZEvaluationMode Mode);

	/** Get the evaluation mode used to evaluate the LensFile */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	float GetOverscanMultiplier() const;

	/** Set the LensFile used by this component */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetOverscanMultiplier(float Multiplier);

	/** Get the filmback override setting */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	EFilmbackOverrideSource GetFilmbackOverrideSetting() const;

	/** Set the filmback override setting */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetFilmbackOverrideSetting(EFilmbackOverrideSource Setting);

	/** Get the cropped filmback (only relevant if the filmback override setting is set to use the CroppedFilmback */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	FCameraFilmbackSettings GetCroppedFilmback() const;

	/** Set the cropped filmback (only relevant if the filmback override setting is set to use the CroppedFilmback */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetCroppedFilmback(FCameraFilmbackSettings Filmback);

	/** Returns true if nodal offset will be automatically applied during this component's tick, false otherwise */
	UFUNCTION(BlueprintPure, Category="Lens Component")
	bool ShouldApplyNodalOffsetOnTick() const;

	/** Set whether nodal offset should be automatically applied during this component's tick */
	UFUNCTION(BlueprintCallable, Category="Lens Component")
	void SetApplyNodalOffsetOnTick(bool bApplyNodalOffset);

	/** Get the distortion source setting */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	EDistortionSource GetDistortionSource() const;

	/** Set the distortion source setting */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetDistortionSource(EDistortionSource Source);

	/** Whether distortion should be applied to the target camera */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	bool ShouldApplyDistortion() const;

	/** Set whether distortion should be applied to the target camera */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetApplyDistortion(bool bApply);

	/** Get the current lens model */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	TSubclassOf<ULensModel> GetLensModel() const;

	/** Set the current lens model */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetLensModel(TSubclassOf<ULensModel> Model);

	/** Get the current distortion state */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	FLensDistortionState GetDistortionState() const;

	/** Set the current distortion state */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void SetDistortionState(FLensDistortionState State);

	/** Reset the distortion state to defaults to represent "no distortion" */
	UFUNCTION(BlueprintCallable, Category = "Lens Component")
	void ClearDistortionState();

	/** Get the original (not adjusted for overscan) focal length of the camera */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	float GetOriginalFocalLength() const;

	/** Get the data used by this component to evaluate the LensFile */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	const FLensFileEvaluationInputs& GetLensFileEvaluationInputs() const;

	/** Returns true if nodal offset was applied during the current tick, false otherwise */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	bool WasNodalOffsetAppliedThisTick() const;

	/** Returns true if distortion was evaluated this tick */
	UFUNCTION(BlueprintPure, Category = "Lens Component")
	bool WasDistortionEvaluated() const;

	/** 
	 * Manually apply nodal offset to the specified component. 
	 * If bUseManualInputs is true, the input Focus and Zoom values will be used to evaluate the LensFile .
	 * If bUseManualInputs is false, the LensFile be will evaluated based on the Lens Component's evaluation mode.
	 */
	UFUNCTION(BlueprintCallable, Category="Lens Component", meta=(AdvancedDisplay=1))
	void ApplyNodalOffset(USceneComponent* ComponentToOffset, bool bUseManualInputs = false, float ManualFocusInput = 0.0f, float ManualZoomInput = 0.0f);

public:
	/** Returns the LensDistortionHandler in use for the current LensModel */
	ULensDistortionModelHandlerBase* GetLensDistortionHandler() const;

	/** Reset the tracked component back to its original tracked pose and reapply nodal offset to it by re-evaluating the LensFile */
	void ReapplyNodalOffset();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.1, "This property is deprecated. Use GetDistortionSource() and SetDistortionSource() instead.")
	FDistortionHandlerPicker GetDistortionHandlerPicker() const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:
	UE_DEPRECATED(5.1, "The use of this callback by this class has been deprecated and it is no longer registered. You can register your own delegate with FWorldDelegates::OnWorldPostActorTick")
	void OnPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);

private:
	/** Evaluate the LensFile for nodal offset (using the current evaluation mode) and apply it to the latest component to offset */
	void ApplyNodalOffset();

	/** Evaluate the focal length from the LensFile and applies the calibrated value to the camera */
	void EvaluateFocalLength(UCineCameraComponent* CineCameraComponent);

	/** If TargetCameraComponent is not set, initialize it to the first CineCameraComponent on the same actor as this component */
	void InitDefaultCamera();

	/** Remove the last distortion MID applied to the input CineCameraComponent and reset its FOV to use no overscan */
	void CleanupDistortion(UCineCameraComponent* CineCameraComponent);

	/** Register a new lens distortion handler with the camera calibration subsystem using the selected lens file */
	void CreateDistortionHandler();

	/** Register to the new LiveLink component's callback to be notified when its controller map changes */
	void OnLiveLinkComponentRegistered(ULiveLinkComponentController* LiveLinkComponent);
	
	/** Callback executed when a LiveLink component on the same actor ticks */
	void ProcessLiveLinkData(const ULiveLinkComponentController* const LiveLinkComponent, const FLiveLinkSubjectFrameData& SubjectData);

	/** Inspects the subject data and LiveLink transform controller to determine which component (if any) had tracking data applied to it */
	void UpdateTrackedComponent(const ULiveLinkComponentController* const LiveLinkComponent, const FLiveLinkSubjectFrameData& SubjectData);

	/** Inspects the subject data and LiveLink camera controller cache the FIZ that was input for the target camera */
	void UpdateLiveLinkFIZ(const ULiveLinkComponentController* const LiveLinkComponent, const FLiveLinkSubjectFrameData& SubjectData);

	/** Updates the focus and zoom inputs that will be used to evaluate the LensFile based on the evaluation mode */
	void UpdateLensFileEvaluationInputs(UCineCameraComponent* CineCameraComponent);

	/** Updates the camera's filmback based on the filmback override settings */
	void UpdateCameraFilmback(UCineCameraComponent* CineCameraComponent);

protected:
	/** Lens File used to drive distortion with current camera settings */
	UPROPERTY(EditAnywhere, Category="Lens File", meta=(ShowOnlyInnerProperties))
	FLensFilePicker LensFilePicker;

	/** Specify how the Lens File should be evaluated */
	UPROPERTY(EditAnywhere, Category="Lens File")
	EFIZEvaluationMode EvaluationMode = EFIZEvaluationMode::UseLiveLink;

	/** The CineCameraComponent on which to apply the post-process distortion effect */
	UPROPERTY(EditInstanceOnly, AdvancedDisplay, Category="Lens File", meta=(UseComponentPicker, AllowedClasses="/Script/CinematicCamera.CineCameraComponent"))
	FComponentReference TargetCameraComponent;

	/** Inputs to LensFile evaluation */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Lens File")
	FLensFileEvaluationInputs EvalInputs;

	/** Specifies from where the distortion state information comes */
	UPROPERTY(EditAnywhere, Category = "Distortion", meta = (DisplayName = "Distortion Source"))
	EDistortionSource DistortionStateSource = EDistortionSource::LensFile;

	/** Whether or not to apply distortion to the target camera component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = ShouldApplyDistortion, Setter = SetApplyDistortion, Category = "Distortion")
	bool bApplyDistortion = false;

	/** The current lens model used for distortion */
	UPROPERTY(EditAnywhere, Category = "Distortion", meta = (EditCondition = "DistortionStateSource == EDistortionSource::Manual"))
	TSubclassOf<ULensModel> LensModel;

	/** The current distortion state */
	UPROPERTY(Interp, EditAnywhere, Category = "Distortion", meta = (ShowOnlyInnerProperties))
	FLensDistortionState DistortionState;

	/** Whether to scale the computed overscan by the overscan percentage */
	UPROPERTY(AdvancedDisplay, BlueprintReadWrite, Category = "Distortion")
	bool bScaleOverscan = false;

	/** The percentage of the computed overscan that should be applied to the target camera */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = "Distortion", meta = (EditCondition = "bScaleOverscan", ClampMin = "0.0", ClampMax = "2.0"))
	float OverscanMultiplier = 1.0f;

	/** Controls whether this component can override the camera's filmback, and if so, which override to use */
	UPROPERTY(EditAnywhere, Category = "Filmback")
	EFilmbackOverrideSource FilmbackOverride = EFilmbackOverrideSource::DoNotOverride;

	/** Cropped filmback to use if the filmback override settings are set to use it */
	UPROPERTY(EditAnywhere, Category = "Filmback", meta=(EditCondition="FilmbackOverride == EFilmbackOverrideSource::CroppedFilmbackSetting"))
	FCameraFilmbackSettings CroppedFilmback;

	/** 
	 * If checked, nodal offset will be applied automatically when this component ticks. 
	 * Set to false if nodal offset needs to be manually applied at some other time (via Blueprints).
	 */
	UPROPERTY(EditAnywhere, Category="Nodal Offset")
	bool bApplyNodalOffsetOnTick = true;

	/** Serialized transform of the TrackedComponent prior to nodal offset being applied */
	UPROPERTY(Interp, VisibleAnywhere, AdvancedDisplay, Category = "Nodal Offset")
	FTransform OriginalTrackedComponentTransform;

	/** Whether a distortion effect is currently being applied to the target camera component */
	UPROPERTY()
	bool bIsDistortionSetup = false;

	/** Focal length of the target camera before any overscan has been applied */
	UPROPERTY()
	float OriginalFocalLength = 35.0f;

	/** Cached MID last applied to the target camera */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> LastDistortionMID = nullptr;

	/** Cached most recent target camera, used to clean up the old camera when the user changes the target */
	UPROPERTY()
	TObjectPtr<UCineCameraComponent> LastCameraComponent = nullptr;

	/** Map of lens models to handlers */
	UPROPERTY(Transient)
	TMap<TSubclassOf<ULensModel>, TObjectPtr<ULensDistortionModelHandlerBase>> LensDistortionHandlerMap;

	/** Scene component that should have nodal offset applied */
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> TrackedComponent;

	/** Serialized name of the TrackedComponent, used to determine which component to re-apply nodal offset to in spawnables */
	UPROPERTY()
	FString TrackedComponentName;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.1, "This property has been deprecated. The LensDistortion component no longer tracks the attached camera's original rotation.")
	UPROPERTY()
	FRotator OriginalCameraRotation_DEPRECATED;

	UE_DEPRECATED(5.1, "This property has been deprecated. The LensDistortion component no longer tracks the attached camera's original location.")
	UPROPERTY()
	FVector OriginalCameraLocation_DEPRECATED;

	UE_DEPRECATED(5.1, "This property has been deprecated. Use the DistortionStateSource to specify whether the lens file should be evaluated for distortion.")
	UPROPERTY()
	bool bEvaluateLensFileForDistortion_DEPRECATED = false;

	UE_DEPRECATED(5.1, "This property has been deprecated. Use the LensDistortionHandlerMap to get a handler for the current LensModel.")
	UPROPERTY(Transient)
	TObjectPtr<ULensDistortionModelHandlerBase> LensDistortionHandler_DEPRECATED = nullptr;

	UE_DEPRECATED(5.1, "This property has been deprecated. Producer GUIDs are no longer used to identify distortion handlers. Use the LensDistortionHandlerMap to get a handler for the current LensModel.")
	UPROPERTY(DuplicateTransient)
	FGuid DistortionProducerID_DEPRECATED;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.1, "This property has been deprecated. The handler picker is no longer used to identify a distortion handler. Use the LensDistortionHandlerMap to get a handler for the current LensModel.")
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="This property has been deprecated. Use GetDistortionSource() and SetDistortionSource() instead."))
	FDistortionHandlerPicker DistortionSource_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif //WITH_EDITORONLY_DATA

private:
	/** Latest LiveLink FIZ data, used to evaluate the LensFile */
	float LiveLinkFocus = 0.0f;
	float LiveLinkIris = 0.0f;
	float LiveLinkZoom = 0.0f;

	/** Whether LiveLink FIZ was received this tick */
	bool bWasLiveLinkFIZUpdated = false;

	/** Whether distortion was evaluated this tick */
	bool bWasDistortionEvaluated = false;

	/** Whether or not nodal offset was applied to a tracked component this tick */
	bool bWasNodalOffsetAppliedThisTick = false;

	/** Latest focal length of the target camera, used to track external changes to the original (no overscan applied) focal length */
	float LastFocalLength = -1.0f;
};
