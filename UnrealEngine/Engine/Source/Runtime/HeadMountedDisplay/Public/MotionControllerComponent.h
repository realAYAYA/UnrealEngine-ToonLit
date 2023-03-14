// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "SceneViewExtension.h"
#include "IMotionController.h"
#include "LateUpdateManager.h"
#include "IIdentifiableXRDevice.h" // for FXRDeviceId
#include "MotionControllerComponent.generated.h"

class FPrimitiveSceneInfo;
class FRHICommandListImmediate;
class FSceneView;
class FSceneViewFamily;

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = MotionController)
class HEADMOUNTEDDISPLAY_API UMotionControllerComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	void BeginDestroy() override;

	/** Which player index this motion controller should automatically follow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetAssociatedPlayerIndex, Category = "MotionController")
	int32 PlayerIndex;

	/** DEPRECATED (use MotionSource instead) Which hand this component should automatically follow */
	UPROPERTY(BlueprintSetter = SetTrackingSource, BlueprintGetter = GetTrackingSource, Category = "MotionController")
	EControllerHand Hand_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetTrackingMotionSource, Category = "MotionController")
	FName MotionSource;

	/** If false, render transforms within the motion controller hierarchy will be updated a second time immediately before rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	uint32 bDisableLowLatencyUpdate:1;

	/** The tracking status for the device (e.g. full tracking, inertial tracking only, no tracking) */
	UPROPERTY(BlueprintReadOnly, Category = "MotionController")
	ETrackingStatus CurrentTrackingStatus;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Whether or not this component had a valid tracked device this frame */
	UFUNCTION(BlueprintPure, Category = "MotionController")
	bool IsTracked() const
	{
		return bTracked;
	}

	/** Used to automatically render a model associated with the set hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetShowDeviceModel, Category="Visualization")
	bool bDisplayDeviceModel;

	UFUNCTION(BlueprintSetter)
	void SetShowDeviceModel(const bool bShowControllerModel);

	/** Determines the source of the desired model. By default, the active XR system(s) will be queried and (if available) will provide a model for the associated device. NOTE: this may fail if there's no default model; use 'Custom' to specify your own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetDisplayModelSource, Category="Visualization", meta=(editcondition="bDisplayDeviceModel"))
	FName DisplayModelSource;

	static FName CustomModelSourceId;
	UFUNCTION(BlueprintSetter)
	void SetDisplayModelSource(const FName NewDisplayModelSource);

	/** A mesh override that'll be displayed attached to this MotionController. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetCustomDisplayMesh, Category="Visualization", meta=(editcondition="bDisplayDeviceModel"))
	TObjectPtr<UStaticMesh> CustomDisplayMesh;

	UFUNCTION(BlueprintSetter)
	void SetCustomDisplayMesh(UStaticMesh* NewDisplayMesh);

	/** Material overrides for the specified display mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visualization", meta=(editcondition="bDisplayDeviceModel"))
	TArray<TObjectPtr<UMaterialInterface>> DisplayMeshMaterialOverrides;

	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "Please use the Motion Source property instead of Hand"))
	void SetTrackingSource(const EControllerHand NewSource);

	UFUNCTION(BlueprintGetter, meta = (DeprecatedFunction, DeprecationMessage = "Please use the Motion Source property instead of Hand"))
	EControllerHand GetTrackingSource() const;

	UFUNCTION(BlueprintSetter)
	void SetTrackingMotionSource(const FName NewSource);

	UFUNCTION(BlueprintSetter)
	void SetAssociatedPlayerIndex(const int32 NewPlayer);

public:
	//~ UObject interface
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 

public:
	//~ UActorComponent interface
	virtual void OnRegister() override;
	virtual void InitializeComponent() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

protected:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderTransform_Concurrent() override;
	//~ End UActorComponent Interface.

	void RefreshDisplayComponent(const bool bForceDestroy = false);

	// Cached Motion Controller that can be read by GetParameterValue. Only valid for the duration of OnMotionControllerUpdated
	IMotionController* InUseMotionController;

	/** Blueprint Implementable function for reponding to updated data from a motion controller (so we can use custom paramater values from it) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Motion Controller Update")
	void OnMotionControllerUpdated();

	// Returns the value of a custom parameter on the current in use Motion Controller (see member InUseMotionController). Only valid for the duration of OnMotionControllerUpdated 
	UFUNCTION(BlueprintCallable, Category = "Motion Controller Update")
	float GetParameterValue(FName InName, bool& bValueFound);

	UFUNCTION(BlueprintCallable, Category = "Motion Controller Update")
	FVector GetHandJointPosition(int jointIndex, bool& bValueFound);

private:

	/** Whether or not this component had a valid tracked controller associated with it this frame*/
	bool bTracked;

	/** Whether or not this component has authority within the frame*/
	bool bHasAuthority;

	/** If true, the Position and Orientation args will contain the most recent controller state */
	bool PollControllerState(FVector& Position, FRotator& Orientation, float WorldToMetersScale);

	void OnModularFeatureUnregistered(const FName& Type, class IModularFeature* ModularFeature);
	IMotionController* PolledMotionController_GameThread;
	IMotionController* PolledMotionController_RenderThread;
	FCriticalSection PolledMotionControllerMutex;


	FTransform RenderThreadRelativeTransform;
	FVector RenderThreadComponentScale;

	/** View extension object that can persist on the render thread without the motion controller component */
	class FViewExtension : public FSceneViewExtensionBase
	{
	public:
		FViewExtension(const FAutoRegister& AutoRegister, UMotionControllerComponent* InMotionControllerComponent);
		virtual ~FViewExtension() {}

		/** ISceneViewExtension interface */
		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
		virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}
		virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
		virtual int32 GetPriority() const override { return -10; }
		virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const;

	private:
		friend class UMotionControllerComponent;

		/** Motion controller component associated with this view extension */
		UMotionControllerComponent* MotionControllerComponent;
		FLateUpdateManager LateUpdate;
	};
	TSharedPtr< FViewExtension, ESPMode::ThreadSafe > ViewExtension;	
 
	UPROPERTY(Transient, BlueprintReadOnly, Category=Visualization, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPrimitiveComponent> DisplayComponent;

	/** Callback for asynchronous display model loads (to set materials, etc.) */
	void OnDisplayModelLoaded(UPrimitiveComponent* DisplayComponent);

	enum class EModelLoadStatus : uint8
	{
		Unloaded,
		Pending,
		InProgress,
		Complete
	};
	EModelLoadStatus DisplayModelLoadState = EModelLoadStatus::Unloaded;

	FXRDeviceId DisplayDeviceId;
#if WITH_EDITOR
	int32 PreEditMaterialCount = 0;
#endif
};
