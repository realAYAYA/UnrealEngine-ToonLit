// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "IIdentifiableXRDevice.h" // for FXRDeviceId
#include "UObject/ObjectMacros.h"
#include "XRDeviceVisualizationComponent.generated.h"

class UMaterialInterface;
class UMotionControllerComponent;
class UStaticMesh;

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = MotionController)
class XRBASE_API UXRDeviceVisualizationComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Whether the visualization offered by this component is being used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetIsVisualizationActive, Category = "Visualization")
	bool bIsVisualizationActive;

	UFUNCTION(BlueprintSetter)
	void SetIsVisualizationActive(bool bNewVisualizationState);

	/** Determines the source of the desired model. By default, the active XR system(s) will be queried and (if available) will provide a model for the associated device. NOTE: this may fail if there's no default model; use 'Custom' to specify your own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetDisplayModelSource, Category = "Visualization")
	FName DisplayModelSource; 

	static FName CustomModelSourceId;
	UFUNCTION(BlueprintSetter)
	void SetDisplayModelSource(const FName NewDisplayModelSource); 

	/** A mesh override that'll be displayed attached to this MotionController. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCustomDisplayMesh, Category = "Visualization")
	TObjectPtr<UStaticMesh> CustomDisplayMesh; 

	UFUNCTION(BlueprintSetter)
	void SetCustomDisplayMesh(UStaticMesh* NewDisplayMesh); 

	/** Material overrides for the specified display mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visualization")
	TArray<TObjectPtr<UMaterialInterface>> DisplayMeshMaterialOverrides; 

	/** Callback for asynchronous display model loads (to set materials, etc.) */
	void OnDisplayModelLoaded(UPrimitiveComponent* DisplayComponent); 

	/** Set by the parent MotionController once tracking kicks in. */
	bool bIsRenderingActive;

	/** Whether this component can be displayed, depending on whether the parent MotionController has activated its rendering. */
	bool CanDeviceBeDisplayed();

	FName MotionSource;

	enum class EModelLoadStatus : uint8
	{
		Unloaded,
		Pending,
		InProgress,
		Complete
	};
	EModelLoadStatus DisplayModelLoadState = EModelLoadStatus::Unloaded;

	FXRDeviceId DisplayDeviceId;

	void RefreshMesh();
	UMotionControllerComponent* FindParentMotionController();
	void SetMaterials(int32 MatCount);
	void OnRegister() override;
	void OnUnregister() override;
#if WITH_EDITOR
	void OnCloseVREditor();
#endif

public:

	UFUNCTION(BlueprintSetter, Category="MotionController")
	void SetIsRenderingActive(bool bRenderingIsActive);

private:
	void OnInteractionProfileChanged();

	bool bInteractionProfilePresent = false;
};