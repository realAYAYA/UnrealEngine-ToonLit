// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"
#include "CineCameraComponent.h"
#include "VPFullScreenUserWidget.h"

#include "VCamOutputProviderBase.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVCamOutputProvider, Log, All);

class UUserWidget;
class UVPFullScreenUserWidget;
class SWindow;
class FSceneViewport;

#if WITH_EDITOR
class FLevelEditorViewportClient;
#endif

UCLASS(BlueprintType, Abstract, EditInlineNew)
class VCAMCORE_API UVCamOutputProviderBase : public UObject
{
	GENERATED_BODY()

public:
	UVCamOutputProviderBase();
	~UVCamOutputProviderBase();

	virtual void BeginDestroy() override;

	// Called when the provider is brought online such as after instantiating or loading a component containing this provider 
	// Use Initialize for any setup logic that needs to survive between Start / Stop cycles such as spawning transient objects 
	// 
	// If bForceInitialization is true then it will force a reinitialization even if the provider was already initialized
	virtual void Initialize();

	// Called when the provider is being shutdown such as before changing level or on exit
	virtual void Deinitialize();

	// Called when the provider is Activated
	virtual void Activate();

	// Called when the provider is Deactivated
	virtual void Deactivate();

	virtual void Tick(const float DeltaTime);

	// Called to turn on or off this output provider
	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetActive(const bool bInActive);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Returns if this output provider is currently active or not
	UFUNCTION(BlueprintPure, Category = "Output")
	bool IsActive() const { return bIsActive; };

	// Returns if this output provider has been initialized or not
	UFUNCTION(BlueprintPure, Category = "Output")
	bool IsInitialized() const { return bInitialized; };

	// Sets the TargetCamera parameter
	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetTargetCamera(const UCineCameraComponent* InTargetCamera);

	// Sets the UMG class to be rendered in this output provider
	UFUNCTION(BlueprintCallable, Category = "Output")
	void SetUMGClass(const TSubclassOf<UUserWidget> InUMGClass);

	// The UMG class to be rendered in this output provider
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayName="UMG Overlay", DisplayPriority = "2"))
	TSubclassOf<UUserWidget> UMGClass;

	// Override the default output resolution with a custom value - NOTE you must toggle bIsActive off then back on for this to take effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "3"))
	bool bUseOverrideResolution = false;

	// When bUseOverrideResolution is set, use this custom resolution
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "4"), meta = (EditCondition = "bUseOverrideResolution", ClampMin = 1))
	FIntPoint OverrideResolution = { 2048, 1536 };

	UVPFullScreenUserWidget* GetUMGWidget() { return UMGWidget; };

	/** Temporarily disable the output.  Caller must eventually call RestoreOutput. */
	void SuspendOutput()
	{
		if (IsActive())
		{
			bWasActive = true;
			SetActive(false);
		}
	}

	/** Restore the output state from previous call to disable output. */
	void RestoreOutput()
	{
		if (bWasActive && !IsActive())
		{
			SetActive(true);
		}
		bWasActive = false;
	}

	/** Calls the VCamModifierInterface on the widget if it exists and also requests any child VCam Widgets to reconnect */
	void NotifyWidgetOfComponentChange() const;

protected:
	// If set, this output provider will execute every frame
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (DisplayPriority = "1"))
	bool bIsActive = false;

	UPROPERTY(Transient)
	bool bInitialized = false;

	EVPWidgetDisplayType DisplayType = EVPWidgetDisplayType::PostProcess;

	virtual void CreateUMG();

	void DisplayUMG();
	void DestroyUMG();

	UVCamOutputProviderBase* GetOtherOutputProviderByIndex(int32 Index) const;

	TSharedPtr<FSceneViewport> GetTargetSceneViewport() const;
	TWeakPtr<SWindow> GetTargetInputWindow() const;

#if WITH_EDITOR
	FLevelEditorViewportClient* GetTargetLevelViewportClient() const;
	TSharedPtr<SLevelViewport> GetTargetLevelViewport() const;
#endif

	UPROPERTY(Transient)
	TObjectPtr<UVPFullScreenUserWidget> UMGWidget = nullptr;

private:
	bool IsOuterComponentEnabled() const;

	TSoftObjectPtr<UCineCameraComponent> TargetCamera;
	bool bWasActive = false;
};
