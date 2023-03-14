// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraComponentPool.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "NiagaraVariant.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "Particles/ParticlePerfStats.h"
#include "Particles/ParticleSystemComponent.h"

#include "NiagaraComponent.generated.h"

class FMeshElementCollector;
class FNiagaraRenderer;
class UNiagaraSystem;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;
class FNiagaraSystemSimulation;
class FNiagaraGpuComputeDispatchInterface;

// Called when the particle system is done
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNiagaraSystemFinished, class UNiagaraComponent*, PSystem);

/**
* UNiagaraComponent is the primitive component for a Niagara System.
* @see ANiagaraActor
* @see UNiagaraSystem
*/
UCLASS(ClassGroup = (Rendering, Common), Blueprintable, hidecategories = Object, hidecategories = Physics, hidecategories = Collision, showcategories = Trigger, editinlinenew, meta = (BlueprintSpawnableComponent, DisplayName = "Niagara Particle System Component"))
class NIAGARA_API UNiagaraComponent : public UFXSystemComponent
{
	friend struct FNiagaraScalabilityManager;
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	DECLARE_MULTICAST_DELEGATE(FOnSystemInstanceChanged);
	DECLARE_MULTICAST_DELEGATE(FOnSynchronizedWithAssetParameters);
#endif

public:

	/********* UFXSystemComponent *********/
	void SetBoolParameter(FName ParameterName, bool Param) override;
	void SetIntParameter(FName ParameterName, int Param) override;
	void SetFloatParameter(FName ParameterName, float Param) override;
	void SetVectorParameter(FName ParameterName, FVector Param) override;
	void SetColorParameter(FName ParameterName, FLinearColor Param) override;
	void SetActorParameter(FName ParameterName, class AActor* Param) override;

	virtual UFXSystemAsset* GetFXSystemAsset() const override;
	void SetEmitterEnable(FName EmitterName, bool bNewEnableState) override;
	void ReleaseToPool() override;
	uint32 GetApproxMemoryUsage() const override;
	virtual void ActivateSystem(bool bFlagAsJustAttached = false) override;
	/********* UFXSystemComponent *********/

	virtual FName GetFNameForStatID() const override;

private:
	UPROPERTY(EditAnywhere, Category="Niagara", meta = (DisplayName = "Niagara System Asset"))
	TObjectPtr<UNiagaraSystem> Asset;

	/** Allows you to control how Niagara selects the tick group, changing this while an instance is active will result in not change as it is cached. */
	UPROPERTY(EditAnywhere, Category = "Niagara", meta = (DisplayName = "Niagara Tick Behavior"))
	ENiagaraTickBehavior TickBehavior = ENiagaraTickBehavior::UsePrereqs;

	/**
	 * Offsets the deterministic random seed of all emitters. Used to achieve variety between components, while still achieving determinism.
	 * WARNINGS:
	 * - If this value is set in a non-deterministic way, it has the potential to break determinism of the entire system.
	 * - This value is applied when emitters are activated/reset, and changing them while the emitter is active has no effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Randomness")
	int32 RandomSeedOffset;

	UPROPERTY()
	FNiagaraUserRedirectionParameterStore OverrideParameters;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FName, bool> EditorOverridesValue_DEPRECATED;

	UPROPERTY(EditAnywhere, Category="Niagara")
	TMap<FNiagaraVariableBase, FNiagaraVariant> TemplateParameterOverrides;

	UPROPERTY(EditAnywhere, Category="Niagara")
	TMap<FNiagaraVariableBase, FNiagaraVariant> InstanceParameterOverrides;

	FOnSystemInstanceChanged OnSystemInstanceChangedDelegate;

	FOnSynchronizedWithAssetParameters OnSynchronizedWithAssetParametersDelegate;
#endif

	/**
	When true, this component's system will be force to update via a slower "solo" path rather than the more optimal batched path with other instances of the same system.
	*/
	UPROPERTY(EditAnywhere, Category = Parameters)
	uint32 bForceSolo : 1;

	/** When true the GPU simulation debug display will enabled, allowing information used during simulation to be visualized. */
	UPROPERTY(EditAnywhere, Category = Parameters)
	uint32 bEnableGpuComputeDebug : 1;

	FNiagaraSystemInstanceControllerPtr SystemInstanceController;

	/** Defines the mode use when updating the System age. */
	ENiagaraAgeUpdateMode AgeUpdateMode;

	/** The desired age of the System instance.  This is only relevant when using the DesiredAge age update mode. */
	float DesiredAge;

	/** The last desired age value that was handled by the tick function.  This is only relevant when using the DesiredAgeNoSeek age update mode. */
	float LastHandledDesiredAge;

	/** Whether or not the component can render while seeking to the desired age. */
	bool bCanRenderWhileSeeking;

	/** The delta time used when seeking to the desired age.  This is only relevant when using the DesiredAge age update mode. */
	float SeekDelta;

	bool bLockDesiredAgeDeltaTimeToSeekDelta;

	/** The maximum amount of time in seconds to spend seeking to the desired age in a single frame. */
	float MaxSimTime;

	/** Whether or not the component is currently seeking to the desired time. */
	bool bIsSeeking;

	UPROPERTY()
	uint32 bAutoDestroy : 1;

	UPROPERTY()
	uint32 bRenderingEnabled : 1;

	//~ Begin UActorComponent Interface.
protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override; 
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	virtual void OnEndOfFrameUpdateDuringTick() override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void BeginDestroy() override;
	//virtual void OnAttachmentChanged() override;

public:
	/**
	* True if we should automatically attach to AutoAttachParent when activated, and detach from our parent when completed.
	* This overrides any current attachment that may be present at the time of activation (deferring initial attachment until activation, if AutoAttachParent is null).
	* When enabled, detachment occurs regardless of whether AutoAttachParent is assigned, and the relative transform from the time of activation is restored.
	* This also disables attachment on dedicated servers, where we don't actually activate even if bAutoActivate is true.
	* @see AutoAttachParent, AutoAttachSocketName, AutoAttachLocationType
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Attachment)
	uint32 bAutoManageAttachment : 1;

	/**
	 * Option for how we handle bWeldSimulatedBodies when we attach to the AutoAttachParent, if bAutoManageAttachment is true.
	 * @see bAutoManageAttachment
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attachment, meta=(EditCondition="bAutoManageAttachment"))
	uint32 bAutoAttachWeldSimulatedBodies : 1;

	/**
	 * Time between forced UpdateTransforms for systems that use dynamically calculated bounds,
	 * Which is effectively how often the bounds are shrunk.
	 */
	UPROPERTY()
	float MaxTimeBeforeForceUpdateTransform;

	/** How to handle pooling for this component instance. */
	ENCPoolMethod PoolingMethod;

	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual void DeactivateImmediate() override;

	FORCEINLINE ENiagaraExecutionState GetRequestedExecutionState() const { return SystemInstanceController ? SystemInstanceController->GetRequestedExecutionState() : ENiagaraExecutionState::Complete; }
	FORCEINLINE ENiagaraExecutionState GetExecutionState() const { return SystemInstanceController ? SystemInstanceController->GetActualExecutionState() : ENiagaraExecutionState::Complete; }

	FORCEINLINE bool IsComplete() const { return SystemInstanceController ? SystemInstanceController->IsComplete() : true; }

private:

	//Internal versions that can be called from the scalability code.
	//These will behave as expected but will keep the component registered with the scalability manager.
	void ActivateInternal(bool bReset, bool bIsScalabilityCull);
	void DeactivateInternal(bool bIsScalabilityCull);
	void DeactivateImmediateInternal(bool bIsScalabilityCull);

	bool ShouldPreCull();
	void RegisterWithScalabilityManager();
	void UnregisterWithScalabilityManager();

	void PostSystemTick_GameThread();
	void OnSystemComplete(bool bExternalCompletion);

public:

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual const UObject* AdditionalStatObject() const override;
	virtual bool IsReadyForOwnerToAutoDestroy() const override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnComponentCreated() override;
	virtual void PostApplyToComponent() override;
	//~ End UActorComponent Interface.

	//~ Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual void GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const override;
	virtual void OnAttachmentChanged() override;
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface
	virtual void OnChildAttached(USceneComponent* ChildComponent) override;
	virtual void OnChildDetached(USceneComponent* ChildComponent) override;
	virtual bool IsVisible() const override;
	//~ Begin USceneComponent Interface

	UE_DEPRECATED(5.0, "This interface is no longer safe to access directly. Use the interface provided by GetSystemInstanceController instead.")
	TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> GetSystemSimulation();

	bool InitializeSystem();
	void DestroyInstance();

	void OnPooledReuse(UWorld* NewWorld);

	/*
	Switch which asset the component is using.
	This requires Niagara to wait for concurrent execution and the override parameter store to be synchronized with the new asset.
	By default existing parameters are reset when we call SetAsset, modify bResetExistingOverrideParameters to leave existing parameter data as is.
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara System Asset"))
	void SetAsset(UNiagaraSystem* InAsset, bool bResetExistingOverrideParameters = true);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara System Asset"))
	UNiagaraSystem* GetAsset() const { return Asset; }

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Forced Solo Mode"))
	void SetForceSolo(bool bInForceSolo);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Is In Forced Solo Mode"))
	bool GetForceSolo()const { return bForceSolo; }

private:
	bool RequiresSoloMode() const;
	void UpdateInstanceSoloMode();
public:

	UFUNCTION(BlueprintCallable, Category = Niagara)
	void SetGpuComputeDebug(bool bEnableDebug);

	/**
	Sets the custom time dilation value for the component.
	Note: This is only available on components that are in solo mode currently.
	*/
	UFUNCTION(BlueprintCallable, Category=Niagara)
	void SetCustomTimeDilation(float Dilation = 1.0f);

	UFUNCTION(BlueprintCallable, Category = Niagara)
	float GetCustomTimeDilation() const { return CustomTimeDilation; }

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Age Update Mode"))
	ENiagaraAgeUpdateMode GetAgeUpdateMode() const;

	/** Sets the age update mode for the System instance. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Age Update Mode"))
	void SetAgeUpdateMode(ENiagaraAgeUpdateMode InAgeUpdateMode);

	/** Gets the desired age of the System instance.  This is only relevant when using the DesiredAge age update mode. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Desired Age"))
	float GetDesiredAge() const;

	/** Sets the desired age of the System instance.  This is only relevant when using the DesiredAge age update mode. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Desired Age"))
	void SetDesiredAge(float InDesiredAge);

	/** Sets the desired age of the System instance and designates that this change is a seek.  When seeking to a desired age the
	    The component can optionally prevent rendering. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Seek to Desired Age"))
	void SeekToDesiredAge(float InDesiredAge);

	/** Sets whether or not the system can render while seeking. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Can Render While Seeking"))
	void SetCanRenderWhileSeeking(bool bInCanRenderWhileSeeking);

	/** Gets the delta value which is used when seeking from the current age, to the desired age.  This is only relevant
	when using the DesiredAge age update mode. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Desired Age Seek Delta"))
	float GetSeekDelta() const;

	/** Sets the delta value which is used when seeking from the current age, to the desired age.  This is only relevant
	when using the DesiredAge age update mode. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Desired Age Seek Delta"))
	void SetSeekDelta(float InSeekDelta);

	/** Gets whether or not the delta time used to tick the system instance when using desired age is locked to the seek delta.  When true, the system instance
	will only be ticked when the desired age has changed by more than the seek delta.  When false the system instance will be ticked by the change in desired
	age when not seeking. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get whether or not to lock the desired age delta time to the seek delta."))
	bool GetLockDesiredAgeDeltaTimeToSeekDelta() const;

	/** Sets whether or not the delta time used to tick the system instance when using desired age is locked to the seek delta.  When true, the system instance
	will only be ticked when the desired age has changed by more than the seek delta.  When false the system instance will be ticked by the change in desired
	age when not seeking. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set whether or not to lock the desired age delta time to the seek delta."))
	void SetLockDesiredAgeDeltaTimeToSeekDelta(bool bLock);

	/**
	Get the maximum CPU time in seconds we will simulate to the desired age, when we go beyond this limit ticks will be processed in the next frame.
	This is only relevant when using the DesiredAge age update mode.
	Note: The componet will not be visibile if we have ticks to process and SetCanRenderWhileSeeking is set to true
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Max Desired Age Tick Delta"))
	float GetMaxSimTime() const;

	/**
	Sets the maximum CPU time in seconds we will simulate to the desired age, when we go beyond this limit ticks will be processed in the next frame.
	This is only relevant when using the DesiredAge age update mode.
	Note: The componet will not be visibile if we have ticks to process and SetCanRenderWhileSeeking is set to true
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Max Desired Age Tick Delta"))
	void SetMaxSimTime(float InMaxTime);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Auto Destroy"))
	void SetAutoDestroy(bool bInAutoDestroy);

	UE_DEPRECATED(5.0, "This interface is no longer safe to access directly. Use the interface provided by GetSystemInstanceController instead.")
	FNiagaraSystemInstance* GetSystemInstance() const { return SystemInstanceController ? SystemInstanceController->GetSystemInstance_Unsafe() : nullptr; }

	FNiagaraSystemInstanceControllerPtr GetSystemInstanceController() { return SystemInstanceController; }
	FNiagaraSystemInstanceControllerConstPtr GetSystemInstanceController() const { return SystemInstanceController; }

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Tick Behavior"))
	void SetTickBehavior(ENiagaraTickBehavior NewTickBehavior);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Tick Behavior"))
	ENiagaraTickBehavior GetTickBehavior() const { return TickBehavior; }

	/**
	Sets the fixed bounds for the system instance, this overrides all other bounds.
	The box is expected to be in local space not world space.
	A default uninitialized box will clear the fixed bounds and revert back to system fixed / dynamic bounds.
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara)
	void SetSystemFixedBounds(FBox LocalBounds);

	/**
	Gets the fixed bounds for the system instance.
	This will return the user set fixed bounds if set, or the systems fixed bounds if set.
	Note: The returned box may be invalid if no fixed bounds exist
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara)
	FBox GetSystemFixedBounds() const;

	/** Clear any previously set fixed bounds for the system instance. */
	UFUNCTION(BlueprintCallable, Category = Niagara)
	void ClearSystemFixedBounds();

	/**
	Sets the fixed bounds for an emitter instance, this overrides all other bounds.
	The box is expected to be in local space not world space.
	A default uninitialized box will clear the fixed bounds and revert back to emitter fixed / dynamic bounds.
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara)
	void SetEmitterFixedBounds(FName EmitterName, FBox LocalBounds);

	/**
	Gets the fixed bounds for an emitter instance.
	This will return the user set fixed bounds if set, or the emitters fixed bounds if set.
	Note: The returned box may be invalid if no fixed bounds exist
	*/
	UFUNCTION(BlueprintCallable, Category = Niagara)
	FBox GetEmitterFixedBounds(FName EmitterName) const;

	/** Clear any previously set fixed bounds for the emitter instance. */
	UFUNCTION(BlueprintCallable, Category = Niagara)
	void ClearEmitterFixedBounds(FName EmitterName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Random Seed Offset"))
	void SetRandomSeedOffset(int32 NewRandomSeedOffset);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Random Seed Offset"))
	int32 GetRandomSeedOffset() const { return RandomSeedOffset; }

	/** Sets a Niagara FLinearColor parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (LinearColor)"))
	void SetNiagaraVariableLinearColor(const FString& InVariableName, const FLinearColor& InValue);

	/** Sets a Niagara FLinearColor parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (LinearColor)"))
	void SetVariableLinearColor(FName InVariableName, const FLinearColor& InValue);


	/** Sets a Niagara Vector4 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Vector4)"))
	void SetNiagaraVariableVec4(const FString& InVariableName, const FVector4& InValue);

	/** Sets a Niagara Vector4 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Vector4)"))
	void SetVariableVec4(FName InVariableName, const FVector4& InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Quaternion)"))
	void SetNiagaraVariableQuat(const FString& InVariableName, const FQuat& InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Quaternion)"))
	void SetVariableQuat(FName InVariableName, const FQuat& InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Matrix)"))
	void SetNiagaraVariableMatrix(const FString& InVariableName, const FMatrix& InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Matrix)"))
	void SetVariableMatrix(FName InVariableName, const FMatrix& InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Vector3)"))
	void SetNiagaraVariableVec3(const FString& InVariableName, FVector InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Vector3)"))
	void SetVariableVec3(FName InVariableName, FVector InValue);

	/** Sets a Niagara Position parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Position)"))
	void SetNiagaraVariablePosition(const FString& InVariableName, FVector InValue);

	/** Sets a Niagara Position parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Position)"))
	void SetVariablePosition(FName InVariableName, FVector InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Vector2)"))
	void SetNiagaraVariableVec2(const FString& InVariableName, FVector2D InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Vector2)"))
	void SetVariableVec2(FName InVariableName, FVector2D InValue);

	/** Sets a Niagara float parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Float)"))
	void SetNiagaraVariableFloat(const FString& InVariableName, float InValue);

	/** Sets a Niagara float parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Float)"))
	void SetVariableFloat(FName InVariableName, float InValue);

	/** Sets a Niagara int parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Int32)"))
	void SetNiagaraVariableInt(const FString& InVariableName, int32 InValue);

	/** Sets a Niagara int parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Int32)"))
	void SetVariableInt(FName InVariableName, int32 InValue);

	/** Sets a Niagara float parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Bool)"))
	void SetNiagaraVariableBool(const FString& InVariableName, bool InValue);

	/** Sets a Niagara float parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Bool)"))
	void SetVariableBool(FName InVariableName, bool InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Actor)"))
	void SetNiagaraVariableActor(const FString& InVariableName, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Actor)"))
	void SetVariableActor(FName InVariableName, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable By String (Object)"))
	void SetNiagaraVariableObject(const FString& InVariableName, UObject* Object);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Object)"))
	void SetVariableObject(FName InVariableName, UObject* Object);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Material)"))
	void SetVariableMaterial(FName InVariableName, UMaterialInterface* Object);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Static Mesh)"))
	void SetVariableStaticMesh(FName InVariableName, UStaticMesh* InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Texture)"))
	void SetVariableTexture(FName InVariableName, class UTexture* Texture);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (TextureRenderTarget)"))
	void SetVariableTextureRenderTarget(FName InVariableName, class UTextureRenderTarget* TextureRenderTarget);

	/** Debug accessors for getting positions in blueprints. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara Emitter Positions", DeprecatedFunction, DeprecationMessage = "Get Niagara Emitter Positions is deprecated, use the particle export DI inside your emitter instead."))
	TArray<FVector> GetNiagaraParticlePositions_DebugOnly(const FString& InEmitterName);

	/** Debug accessors for getting a float attribute array in blueprints.  The attribute name should be without namespaces. For example for "Particles.Position", send "Position". */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara Emitter Float Attrib", DeprecatedFunction, DeprecationMessage = "Get Niagara Emitter Float Attrib is deprecated, use the particle export DI inside your emitter instead."))
	TArray<float> GetNiagaraParticleValues_DebugOnly(const FString& InEmitterName, const FString& InValueName);

	/** Debug accessors for getting a FVector attribute array in blueprints. The attribute name should be without namespaces. For example for "Particles.Position", send "Position". */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara Emitter Vec3 Attrib", DeprecatedFunction, DeprecationMessage = "Get Niagara Emitter Vec3 Attrib is deprecated, use the particle export DI inside your emitter instead."))
	TArray<FVector> GetNiagaraParticleValueVec3_DebugOnly(const FString& InEmitterName, const FString& InValueName);

	/** Resets the System to it's initial pre-simulated state. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Reset System"))
	void ResetSystem();

	/** Called on when an external object wishes to force this System to reinitialize itself from the System data.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Reinitialize System"))
	void ReinitializeSystem();

	/** Gets whether or not rendering is enabled for this component. */
	bool GetRenderingEnabled() const;

	/** Sets whether or not rendering is enabled for this component. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Rendering Enabled"))
	void SetRenderingEnabled(bool bInRenderingEnabled);

	/** Advances this system's simulation by the specified number of ticks and delta time. */
	UFUNCTION(BlueprintCallable, Category = Niagara)
	void AdvanceSimulation(int32 TickCount, float TickDeltaSeconds);

	/** Advances this system's simulation by the specified time in seconds and delta time. Advancement is done in whole ticks of TickDeltaSeconds so actual simulated time will be the nearest lower multiple of TickDeltaSeconds. */
	UFUNCTION(BlueprintCallable, Category = Niagara)
	void AdvanceSimulationByTime(float SimulateTime, float TickDeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = Niagara)
	void SetPaused(bool bInPaused);

	UFUNCTION(BlueprintCallable, Category = Niagara)
	bool IsPaused() const;

	UE_DEPRECATED(4.27, "This method will be removed in a future release. Use the method provided by the Niagara Function Library instead.")
	UFUNCTION(BlueprintCallable, Category = Niagara)
	UNiagaraDataInterface* GetDataInterface(const FString &Name);

	/**
		The significant index for this component. i.e. this is the Nth most significant instance of it's system in the scene.
		Passed to the script to allow us to scale down internally for less significant systems instances.
	*/
	FORCEINLINE void SetSystemSignificanceIndex(int32 InIndex) 	{ if(SystemInstanceController) SystemInstanceController->SetSystemSignificanceIndex(InIndex); }

	//~ Begin UObject Interface.
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	  * Find the value of an overridden parameter.  The value returned may not be the current value being used by the simulation but
	  * will reflect the last value which has been set through the editor on a component placed in a level, or on a component in the 
	  * blueprint editor.
	  * Returns null if the parameter isn't overridden by this component.
	  */
	FNiagaraVariant FindParameterOverride(const FNiagaraVariableBase& InKey) const;

	/** Gets the current value of a parameter which is being used by the simulation.  This value will reflect runtime changes such as
	  * those made by sequencer, or in PIE through code or blueprint. */
	FNiagaraVariant GetCurrentParameterValue(const FNiagaraVariableBase& InKey) const;

	bool HasParameterOverride(const FNiagaraVariableBase& InKey) const;
	void SetParameterOverride(const FNiagaraVariableBase& InKey, const FNiagaraVariant& InValue);

	/** Remove an override for a given parameter if one exists. */
	void RemoveParameterOverride(const FNiagaraVariableBase& InKey);

	void UpgradeDeprecatedParameterOverrides();
	void EnsureOverrideParametersConsistent() const;
#endif
	//~ End UObject Interface

	UFUNCTION(BlueprintCallable, Category = Preview, meta = (Keywords = "preview LOD Distance scalability"))
	void SetPreviewLODDistance(bool bEnablePreviewLODDistance, float PreviewLODDistance, float PreviewMaxDistance);

	UFUNCTION(BlueprintCallable, Category = Preview, meta = (Keywords = "preview LOD Distance scalability"))
	FORCEINLINE bool GetPreviewLODDistanceEnabled()const;

	UFUNCTION(BlueprintCallable, Category = Preview, meta = (Keywords = "preview LOD Distance scalability"))
	FORCEINLINE float GetPreviewLODDistance()const;

	/**
	Initializes this component for capturing a performance baseline.
	This will do things such as disabling distance culling and setting a LODDistance of 0 to ensure the effect is at it's maximum cost.
	*/
	UFUNCTION(BlueprintCallable, Category = Performance, meta = (Keywords = "Niagara Performance"))
	void InitForPerformanceBaseline();

	FORCEINLINE void SetLODDistance(float InLODDistance, float InMaxLODDistance);

#if WITH_EDITOR
	void PostLoadNormalizeOverrideNames();
	FOnSystemInstanceChanged& OnSystemInstanceChanged() { return OnSystemInstanceChangedDelegate; }

	FOnSynchronizedWithAssetParameters& OnSynchronizedWithAssetParameters() { return OnSynchronizedWithAssetParametersDelegate; }
#endif

	FNiagaraUserRedirectionParameterStore& GetOverrideParameters() { return OverrideParameters; }

	const FNiagaraParameterStore& GetOverrideParameters() const { return OverrideParameters; }

	bool IsWorldReadyToRun() const;

	//~ End UObject Interface.

	// Called when the particle system is done
	UPROPERTY(BlueprintAssignable)
	FOnNiagaraSystemFinished OnSystemFinished;

	/** Removes all local overrides and replaces them with the values from the source System - note: this also removes the editor overrides from the component as it is used by the pooling mechanism to prevent values leaking between different instances. */
	void SetUserParametersToDefaultValues();

	/** Is this an effect on or linked to the local player. */
	bool IsLocalPlayerEffect()const;

private:
	/** Compare local overrides with the source System. Remove any that have mismatched types or no longer exist on the System.*/
	void SynchronizeWithSourceSystem();

	void FixInvalidUserParameterOverrideData();

	void AssetExposedParametersChanged();

	void CopyParametersFromAsset(bool bResetExistingOverrideParameters = true);

#if WITH_EDITOR
	void SetOverrideParameterStoreValue(const FNiagaraVariableBase& InKey, const FNiagaraVariant& InValue);
	void ApplyOverridesToParameterStore();

	/**
	 * Ensures that the data interfaces in the template parameter overrides, instance parameter overrides, and in the override parameters
	 * parameter store have this component as their outer and have their archetype cleared.  This is required to hack around issues with
	 * subobject handling on components when they're spawned from templates which can cause invalid cross package references.
 	 */
	void FixDataInterfaceOuters();
#endif

public:
	/**
	 * Component we automatically attach to when activated, if bAutoManageAttachment is true.
	 * If null during registration, we assign the existing AttachParent and defer attachment until we activate.
	 * @see bAutoManageAttachment
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category=Attachment, meta=(EditCondition="bAutoManageAttachment"))
	TWeakObjectPtr<USceneComponent> AutoAttachParent;

	/**
	 * Socket we automatically attach to on the AutoAttachParent, if bAutoManageAttachment is true.
	 * @see bAutoManageAttachment
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Attachment, meta=(EditCondition="bAutoManageAttachment"))
	FName AutoAttachSocketName;

	/**
	 * Options for how we handle our location when we attach to the AutoAttachParent, if bAutoManageAttachment is true.
	 * @see bAutoManageAttachment, EAttachmentRule
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attachment, meta = (EditCondition = "bAutoManageAttachment"))
	EAttachmentRule AutoAttachLocationRule;

	/**
	 * Options for how we handle our rotation when we attach to the AutoAttachParent, if bAutoManageAttachment is true.
	 * @see bAutoManageAttachment, EAttachmentRule
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attachment, meta = (EditCondition = "bAutoManageAttachment"))
	EAttachmentRule AutoAttachRotationRule;

	/**
	 * Options for how we handle our scale when we attach to the AutoAttachParent, if bAutoManageAttachment is true.
	 * @see bAutoManageAttachment, EAttachmentRule
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attachment, meta = (EditCondition = "bAutoManageAttachment"))
	EAttachmentRule AutoAttachScaleRule;


	/**
	 * Set AutoAttachParent, AutoAttachSocketName, AutoAttachLocationRule, AutoAttachRotationRule, AutoAttachScaleRule to the specified parameters. Does not change bAutoManageAttachment; that must be set separately.
	 * @param  Parent			Component to attach to.
	 * @param  SocketName		Socket on Parent to attach to.
	 * @param  LocationRule		Option for how we handle our location when we attach to Parent.
	 * @param  RotationRule		Option for how we handle our rotation when we attach to Parent.
	 * @param  ScaleRule		Option for how we handle our scale when we attach to Parent.
	 * @see bAutoManageAttachment, AutoAttachParent, AutoAttachSocketName, AutoAttachLocationRule, AutoAttachRotationRule, AutoAttachScaleRule
	 */
	void SetAutoAttachmentParameters(USceneComponent* Parent, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule) override;

	virtual void SetUseAutoManageAttachment(bool bAutoManage) override { bAutoManageAttachment = bAutoManage; }

	float PreviewLODDistance;
	float PreviewMaxDistance;
	uint32 bEnablePreviewLODDistance : 1;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Compilation)
	uint32 bWaitForCompilationOnActivate : 1;
#endif

	/**
	Sets the simulation cache to use for the component.
	A null SimCache parameter will clear the active simulation cache.
	When clearing a simulation cache that has been running you may wish to reset or continue, this option is only
	valid when using a full simulation cache.  A renderer only cache will always reset as we can not continue the
	simulation due to missing simulation data.
	*/
	UFUNCTION(BlueprintCallable, Category = SimCache)
	void SetSimCache(UNiagaraSimCache* SimCache, bool bResetSystem = false);

	/**
	Get the active simulation cache, will return null if we do not have an active one.
	*/
	UFUNCTION(BlueprintCallable, Category = SimCache)
	UNiagaraSimCache* GetSimCache() const;

	/**
	Clear any active simulation cache.
	When clearing a simulation cache that has been running you may wish to reset or continue, this option is only
	valid when using a full simulation cache.  A renderer only cache will always reset as we can not continue the
	simulation due to missing simulation data.
	*/
	UFUNCTION(BlueprintCallable, Category = SimCache)
	void ClearSimCache(bool bResetSystem = false) { SetSimCache(nullptr, bResetSystem); }

	/** Set whether this component is allowed to perform scalability checks and potentially be culled etc. Occasionally it is useful to disable this for specific components. E.g. Effects on the local player. */
	UFUNCTION(BlueprintSetter, Category = Scalability, meta = (Keywords = "LOD scalability"))
	void SetAllowScalability(bool bAllow);
	
	UFUNCTION(BlueprintGetter)
	bool GetAllowScalability()const;
	
	UFUNCTION(BlueprintSetter, Category = Scalability, meta = (Keywords = "LOD scalability"))
	void SetForceLocalPlayerEffect(bool bIsPlayerEffect);
	
	UFUNCTION(BlueprintGetter)
	bool GetForceLocalPlayerEffect()const;

	FORCEINLINE bool IsRegisteredWithScalabilityManager()const { return ScalabilityManagerHandle != INDEX_NONE; }
	FORCEINLINE int32 GetScalabilityManagerHandle()const { return ScalabilityManagerHandle; }

	FORCEINLINE void BeginUpdateContextReset(){ bDuringUpdateContextReset = true; }
	FORCEINLINE void EndUpdateContextReset(){ bDuringUpdateContextReset = false; }

#if WITH_NIAGARA_DEBUGGER	
	//Cache our scalability state in the component so we have access to it easily and also after it has been removed from the scalability manager.
	FNiagaraScalabilityState DebugCachedScalabilityState;
#endif

	FORCEINLINE bool IsUsingCullProxy()const { return CullProxy != nullptr; }

	bool ResolveOwnerAllowsScalability(bool bRegister=true);

private:
	/** Did we try and activate but fail due to the asset being not yet ready. Keep looping.*/
	uint32 bAwaitingActivationDueToNotReady : 1;
	/** Should we try and reset when ready? */
	uint32 bActivateShouldResetWhenReady : 1;

	/** Did we auto attach during activation? Used to determine if we should restore the relative transform during detachment. */
	uint32 bDidAutoAttach : 1;

	/** Controls whether we allow scalability culling for this component. If enabled, this component's FX may be culled due to things such as distance, visibility, instance counts and performance. */
	UPROPERTY(EditAnywhere, Category=Niagara, BlueprintGetter=GetAllowScalability, BlueprintSetter=SetAllowScalability)
	uint32 bAllowScalability : 1;

	/** Flag allowing us to force this Effect to be considered a LocalPlayer Effect. This can prevent it being scalabiltiy culled in some situations etc. */
	//UPROPERTY(EditAnywhere, Category = Niagara, BlueprintGetter = GetForceLocalPlayerEffect, BlueprintSetter = SetForceLocalPlayerEffect)
	uint32 bForceLocalPlayerEffect : 1;

	/** Whether the owner of this component allows it to be scalability culled. We optionally do not allow scalability culling for components on or attached to the local player. */
	uint32 bOwnerAllowsScalabiltiy : 1;

	/** True if this component has been culled by the scalability manager. */
	uint32 bIsCulledByScalability : 1;

	/** Flag to mark us as currently changing auto attachment as part of Activate/Deactivate so we don't reset in the OnAttachmentChanged() callback. */
	//uint32 bIsChangingAutoAttachment : 1;

	/** True if we're currently inside an update context reset. This will prevent us from doing some completion work such as releaseing to the pool or auto destroy etc during a reset. */
	uint32 bDuringUpdateContextReset : 1;

	/** Restore relative transform from auto attachment and optionally detach from parent (regardless of whether it was an auto attachment). */
	void CancelAutoAttachment(bool bDetachFromParent);


	/** Saved relative transform before auto attachment. Used during detachment to restore the transform if we had automatically attached. */
	FVector SavedAutoAttachRelativeLocation;
	FRotator SavedAutoAttachRelativeRotation;
	FVector SavedAutoAttachRelativeScale3D;

	FDelegateHandle AssetExposedParametersChangedHandle;

	UNiagaraEffectType* ScalabilityEffectType = nullptr;
	int32 ScalabilityManagerHandle;

	float ForceUpdateTransformTime;
	FBox CurrLocalBounds;

	FBox SystemFixedBounds;
	TMap<FName, FBox> EmitterFixedBounds;


	float CustomTimeDilation = 1.0f;

	UPROPERTY(transient)
	TObjectPtr<class UNiagaraSimCache> SimCache;

	UPROPERTY(transient)
	TObjectPtr<class UNiagaraCullProxyComponent> CullProxy;

	void CreateCullProxy(bool bForce = false);
	void DestroyCullProxy();

public:
	FORCEINLINE FParticlePerfStatsContext GetPerfStatsContext(){ return FParticlePerfStatsContext(GetWorld(), Asset, this); }
};

FORCEINLINE bool UNiagaraComponent::GetPreviewLODDistanceEnabled()const { return bEnablePreviewLODDistance; }
FORCEINLINE float UNiagaraComponent::GetPreviewLODDistance()const { return bEnablePreviewLODDistance ? PreviewLODDistance : 0.0f; }

/**
* Scene proxy for drawing niagara particle simulations.
*/
class NIAGARA_API FNiagaraSceneProxy : public FPrimitiveSceneProxy
{
public:
	virtual SIZE_T GetTypeHash() const override;

	FNiagaraSceneProxy(UNiagaraComponent* InComponent);
	~FNiagaraSceneProxy();

	/** Retrieves the render data for a single system */
	FNiagaraSystemRenderData* GetSystemRenderData() { return RenderData; }

	/** Called to allow renderers to free render state */
	void DestroyRenderState_Concurrent();

	/** Gets whether or not this scene proxy should be rendered. */
	bool GetRenderingEnabled() const;

	/** Sets whether or not this scene proxy should be rendered. */
	void SetRenderingEnabled(bool bInRenderingEnabled);

	FNiagaraGpuComputeDispatchInterface* GetComputeDispatchInterface() const { return ComputeDispatchInterface; }

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;
	virtual bool IsRayTracingRelevant() const override { return true; }
	virtual bool HasRayTracingRepresentation() const override { return true; }
#endif

	FORCEINLINE const FMatrix& GetLocalToWorldInverse() const { return LocalToWorldInverse; }

	const FVector3f& GetLWCRenderTile() const { return RenderData ? RenderData->LWCRenderTile : FVector3f::ZeroVector; }

	TUniformBuffer<FPrimitiveUniformShaderParameters>* GetCustomUniformBufferResource(bool bHasVelocity, const FBox& InstanceBounds = FBox(ForceInitToZero)) const;
	FRHIUniformBuffer* GetCustomUniformBuffer(bool bHasVelocity, const FBox& InstanceBounds = FBox(ForceInitToZero)) const;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	/** Some proxy wide dynamic settings passed down with the emitter dynamic data. */
	struct FDynamicData
	{
		bool bUseCullProxy = false;
		float LODDistanceOverride = -1.0f;
#if WITH_PARTICLE_PERF_STATS
		FParticlePerfStatsContext PerfStatsContext;
#endif
	};
	const FDynamicData& GetProxyDynamicData()const { return DynamicData; }
	void SetProxyDynamicData(const FDynamicData& NewData) { DynamicData = NewData; }

private:
	void ReleaseRenderThreadResources();

	void ReleaseUniformBuffers(bool bEmpty);

	//~ Begin FPrimitiveSceneProxy Interface.
	virtual void CreateRenderThreadResources() override;

	//virtual void OnActorPositionChanged() override;
	virtual void OnTransformChanged() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;


	virtual bool CanBeOccluded() const override
	{
		// TODO account for MaterialRelevance.bDisableDepthTest and MaterialRelevance.bPostMotionBlurTranslucency as well
		return !ShouldRenderCustomDepth();
	}


	/** Callback from the renderer to gather simple lights that this proxy wants renderered. */
	virtual void GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const override;

	virtual uint32 GetMemoryFootprint() const override;

	uint32 GetAllocatedSize() const;

private:
	/** Custom Uniform Buffers, allows us to have renderer specific data packed inside such as pre-skinned bounds. */
	mutable TMap<uint64, TUniformBuffer<FPrimitiveUniformShaderParameters>*> CustomUniformBuffers;

	/** The data required to render a single instance of a NiagaraSystem */
	FNiagaraSystemRenderData* RenderData = nullptr;

	FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = nullptr;

	FMatrix LocalToWorldInverse;

	TStatId SystemStatID;

	FDynamicData DynamicData;
};

FORCEINLINE void UNiagaraComponent::SetLODDistance(float InLODDistance, float InMaxLODDistance)
{
	if (!bEnablePreviewLODDistance && SystemInstanceController)
	{
		SystemInstanceController->SetLODDistance(InLODDistance, InMaxLODDistance, false);
	}
}

