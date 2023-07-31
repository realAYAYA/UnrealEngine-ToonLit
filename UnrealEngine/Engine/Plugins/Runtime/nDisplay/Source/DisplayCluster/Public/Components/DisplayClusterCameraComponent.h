// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/IDisplayClusterComponent.h"

#include "DisplayClusterCameraComponent.generated.h"

class UBillboardComponent;
class UTexture2D;


UENUM()
enum class EDisplayClusterEyeStereoOffset : uint8
{
	None  UMETA(DisplayName = "Default"),
	Left  UMETA(DisplayName = "Left Eye"),
	Right UMETA(DisplayName = "Right Eye"),
};


/**
 * 3D point in space used to render nDisplay viewports from
 */
UCLASS(ClassGroup = (DisplayCluster), meta = (BlueprintSpawnableComponent, DisplayName = "NDisplay View Origin"))
class DISPLAYCLUSTER_API UDisplayClusterCameraComponent
	: public USceneComponent
	, public IDisplayClusterComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterCameraComponent(const FObjectInitializer& ObjectInitializer);

public:
	/**
	* Get interpupillary distance
	*
	* @return - Interpupillary distance
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	float GetInterpupillaryDistance() const
	{
		return InterpupillaryDistance;
	}

	/**
	* Set interpupillary distance
	*
	* @param Distance - New interpupillary distance
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	void SetInterpupillaryDistance(float Distance)
	{
		InterpupillaryDistance = Distance;
	}

	/**
	* Get swap eyes state
	*
	* @return - Eyes swap state. False - normal eyes left|right, true - swapped eyes right|left
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	bool GetSwapEyes() const
	{
		return bSwapEyes;
	}

	/**
	* Set swap eyes state
	*
	* @param SwapEyes - New eyes swap state. False - normal eyes left|right, true - swapped eyes right|left
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	void SetSwapEyes(bool SwapEyes)
	{
		bSwapEyes = SwapEyes;
	}

	/**
	* Toggles eyes swap state
	*
	* @return - New eyes swap state. False - normal eyes left|right, true - swapped eyes right|left
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	bool ToggleSwapEyes()
	{
		return (bSwapEyes = !bSwapEyes);
	}

	/**
	* Get stereo offset type
	*
	* @return - Current forced stereo offset type
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	EDisplayClusterEyeStereoOffset GetStereoOffset() const
	{
		return StereoOffset;
	}

	/**
	* Set stereo offset type
	*
	* @param StereoOffset - New forced stereo offset type
	*/
	UFUNCTION(BlueprintCallable, Category = "Stereo")
	void SetStereoOffset(EDisplayClusterEyeStereoOffset InStereoOffset)
	{
		StereoOffset = InStereoOffset;
	}

public:
#if WITH_EDITOR
	// Begin IDisplayClusterComponent
	virtual void SetVisualizationScale(float Scale) override;
	virtual void SetVisualizationEnabled(bool bEnabled) override;
	// End IDisplayClusterComponent
#endif

	// Begin UActorComponent
	virtual void OnRegister() override;
	// End UActorComponent

	// Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End UObject

protected:
#if WITH_EDITOR
	/** Refreshes the visual components to match the component state */
	virtual void RefreshVisualRepresentation();
#endif

#if WITH_EDITORONLY_DATA
protected:
	/** Gizmo visibility */
	UPROPERTY(EditAnywhere, Category = "Gizmo")
	uint8 bEnableGizmo : 1;

	/** Base gizmo scale */
	UPROPERTY(EditAnywhere, Category = "Gizmo", meta = (EditCondition = "bEnableGizmo"))
	FVector BaseGizmoScale;

	/** Gizmo scale multiplier */
	UPROPERTY(EditAnywhere, Category = "Gizmo", meta = (UIMin = "0", UIMax = "2.0", ClampMin = "0.01", ClampMax = "10.0", EditCondition = "bEnableGizmo"))
	float GizmoScaleMultiplier;

	UPROPERTY(Transient)
	TObjectPtr<UBillboardComponent> SpriteComponent;

	UPROPERTY()
	TObjectPtr<UTexture2D> SpriteTexture;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "Stereo")
	float InterpupillaryDistance;
	
	UPROPERTY(EditAnywhere, Category = "Stereo")
	bool bSwapEyes;
	
	UPROPERTY(EditAnywhere, Category = "Stereo")
	EDisplayClusterEyeStereoOffset StereoOffset;
};
