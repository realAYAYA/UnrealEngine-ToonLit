// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
class UXRDeviceVisualizationComponent;

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = MotionController)
class HEADMOUNTEDDISPLAY_API UMotionControllerComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

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
	uint32 bDisableLowLatencyUpdate : 1;

	/** The tracking status for the device (e.g. full tracking, inertial tracking only, no tracking) */
	UPROPERTY(BlueprintReadOnly, Category = "MotionController")
	ETrackingStatus CurrentTrackingStatus;

	/** Used to visualize this component's device */
	UXRDeviceVisualizationComponent* VisualizationComponent;

	/** Used to automatically render a model associated with the set hand. */
	UE_DEPRECATED(5.2, "bDisplayDeviceModel is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetShowDeviceModel, Category = "Visualization", meta = (DeprecatedProperty, DeprecationMessage = "bDisplayDeviceModel is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	bool bDisplayDeviceModel;

	/** Determines the source of the desired model. By default, the active XR system(s) will be queried and (if available) will provide a model for the associated device. NOTE: this may fail if there's no default model; use 'Custom' to specify your own. */
	UE_DEPRECATED(5.2, "DisplayModelSource is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetDisplayModelSource, Category = "Visualization", meta = (editcondition = "bDisplayDeviceModel", DeprecatedProperty, DeprecationMessage = "DisplayModelSource is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	FName DisplayModelSource;

	static FName CustomModelSourceId;

	/** A mesh override that'll be displayed attached to this MotionController. */
	UE_DEPRECATED(5.2, "CustomDisplayMesh is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCustomDisplayMesh, Category = "Visualization", meta = (editcondition = "bDisplayDeviceModel", DeprecatedProperty, DeprecationMessage = "CustomDisplayMesh is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	TObjectPtr<UStaticMesh> CustomDisplayMesh;

	/** Material overrides for the specified display mesh. */
	UE_DEPRECATED(5.2, "DisplayMeshMaterialOverrides is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visualization", meta = (editcondition = "bDisplayDeviceModel", DeprecatedProperty, DeprecationMessage = "DisplayMeshMaterialOverrides is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	TArray<TObjectPtr<UMaterialInterface>> DisplayMeshMaterialOverrides;

	UE_DEPRECATED(5.2, "SetShowDeviceModel is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "SetShowDeviceModel is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	void SetShowDeviceModel(const bool bShowControllerModel);

	UE_DEPRECATED(5.2, "SetDisplayModelSource is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "SetDisplayModelSource is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	void SetDisplayModelSource(const FName NewDisplayModelSource);

	UE_DEPRECATED(5.2, "SetCustomDisplayMesh is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead.")
	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "SetCustomDisplayMesh is deprecated. Please use the XRDeviceVisualizationComponent for rendering instead."))
	void SetCustomDisplayMesh(UStaticMesh* NewDisplayMesh);

	/** Whether or not this component had a valid tracked device this frame */
	UFUNCTION(BlueprintPure, Category = "MotionController")
	bool IsTracked() const
	{
		return bTracked;
	}

	UFUNCTION(BlueprintSetter, meta = (DeprecatedFunction, DeprecationMessage = "Please use the Motion Source property instead of Hand"))
	void SetTrackingSource(const EControllerHand NewSource);

	UFUNCTION(BlueprintGetter, meta = (DeprecatedFunction, DeprecationMessage = "Please use the Motion Source property instead of Hand"))
	EControllerHand GetTrackingSource() const;

	UFUNCTION(BlueprintSetter)
	void SetTrackingMotionSource(const FName NewSource);

	FName GetTrackingMotionSource();

	UFUNCTION(BlueprintSetter)
	void SetAssociatedPlayerIndex(const int32 NewPlayer);

	void BeginPlay() override;
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void BeginDestroy() override;

	// The following private properties/members are now deprecated and will be removed in later versions.
	void RefreshDisplayComponent(const bool bForceDestroy = false);
	void PostLoad() override;

	/** Callback for asynchronous display model loads (to set materials, etc.) */
	void OnDisplayModelLoaded(UPrimitiveComponent* DisplayComponent);

	UPROPERTY(Transient, BlueprintReadOnly, Category = Visualization, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPrimitiveComponent> DisplayComponent;

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

public:
	//~ UObject interface
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 

	//~ UActorComponent interface
	virtual void OnRegister() override;
	virtual void InitializeComponent() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

protected:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderTransform_Concurrent() override;
	//~ End UActorComponent Interface.

	// Cached Motion Controller that can be read by GetParameterValue. Only valid for the duration of OnMotionControllerUpdated
	IMotionController* InUseMotionController;

	/** Blueprint Implementable function for responding to updated data from a motion controller (so we can use custom parameter values from it) */
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

	/** Whether or not this component has informed the visualization component (if present) to start rendering */
	bool bHasStartedRendering;

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
};