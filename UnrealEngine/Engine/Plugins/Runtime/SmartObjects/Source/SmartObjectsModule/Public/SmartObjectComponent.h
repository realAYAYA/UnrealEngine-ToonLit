// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectDefinitionReference.h"
#include "SmartObjectComponent.generated.h"

namespace EEndPlayReason { enum Type : int; }

class UAbilitySystemComponent;
struct FSmartObjectRuntime;
struct FSmartObjectComponentInstanceData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSmartObjectComponentEventSignature, const FSmartObjectEventData&, EventData, const AActor*, Interactor);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSmartObjectComponentEventNativeSignature, const FSmartObjectEventData& EventData, const AActor* Interactor);

enum class ESmartObjectRegistrationType : uint8
{
	/** Not registered yet */
	NotRegistered,

	/**
	 * Registered and bound to a SmartObject already created from a persistent collection entry or from method CreateSmartObject.
	 * Lifetime of the SmartObject is not bound to the component unregistration but by method UnregisterCollection in the case of 
	 * a collection entry or by method DestroySmartObject when CreateSmartObject was used.
	 */
	BindToExistingInstance,

	/**
	 * Component is registered and bound to a newly created SmartObject.
	 * The lifetime of the SmartObject is bound to the component unregistration will be unbound/destroyed by UnregisterSmartObject/RemoveSmartObject.
	 */
	Dynamic,
	
	None UE_DEPRECATED(5.4, "Use NotRegistered enumeration value instead.") = NotRegistered,
	WithCollection UE_DEPRECATED(5.4, "Use NotRegistered enumeration value instead.") = BindToExistingInstance,
};

enum class ESmartObjectUnregistrationType : uint8
{
	/**
	 * Component registered by a collection (WithCollection) will be unbound from the simulation but its associated runtime data will persist.
	 * Otherwise (Dynamic), runtime data will also be destroyed.
	 */
	RegularProcess,
	/** Component will be unbound from the simulation and its runtime data will be destroyed regardless of the registration type */
	ForceRemove
};

UCLASS(Blueprintable, ClassGroup = Gameplay, meta = (BlueprintSpawnableComponent), config = Game, HideCategories = (Activation, AssetUserData, Collision, Cooking, HLOD, Lighting, LOD, Mobile, Mobility, Navigation, Physics, RayTracing, Rendering, Tags, TextureStreaming))
class SMARTOBJECTSMODULE_API USmartObjectComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSmartObjectChanged, const USmartObjectComponent& /*Instance*/);

	explicit USmartObjectComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FBox GetSmartObjectBounds() const;

	/** @return Smart Object Definition with parameters applied. */
	UFUNCTION(BlueprintGetter)
	const USmartObjectDefinition* GetDefinition() const;

	/** @return Smart Object Definition without applied parameters. */
	const USmartObjectDefinition* GetBaseDefinition() const;

	/** Sets the Smart Object Definition. */
	UFUNCTION(BlueprintSetter)
	void SetDefinition(USmartObjectDefinition* DefinitionAsset);

	bool GetCanBePartOfCollection() const { return bCanBePartOfCollection; }

	ESmartObjectRegistrationType GetRegistrationType() const { return RegistrationType; }
	FSmartObjectHandle GetRegisteredHandle() const { return RegisteredHandle; }
	void SetRegisteredHandle(const FSmartObjectHandle Value, const ESmartObjectRegistrationType InRegistrationType);
	void InvalidateRegisteredHandle();

	void OnRuntimeInstanceBound(FSmartObjectRuntime& RuntimeInstance);
	void OnRuntimeInstanceUnbound(FSmartObjectRuntime& RuntimeInstance);

	/**
	 * Enables or disables the smart object using the default reason (i.e. Gameplay).
	 * @return false if it was not possible to change the enabled state (ie. if it's not registered or there is no smart object subsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Set SmartObject Enabled (default reason: Gameplay)", ReturnDisplayName="Status changed"))
	bool SetSmartObjectEnabled(const bool bEnable) const;

	/**
	 * Enables or disables the smart object for the specified reason.
	 * @param ReasonTag Valid Tag to specify the reason for changing the enabled state of the object. Method will ensure if not valid (i.e. None).
	 * @param bEnabled If true enables the smart object, disables otherwise.
	 * @return false if it was not possible to change the enabled state (ie. if it's not registered or there is no smart object subsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Set SmartObject Enabled (specific reason)", ReturnDisplayName="Status changed"))
	bool SetSmartObjectEnabledForReason(FGameplayTag ReasonTag, const bool bEnabled) const;

	/**
	 * Returns the enabled state of the smart object regardless of the disabled reason.
	 * @return True when associated smart object is set to be enabled. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Is SmartObject Enabled (for any reason)", ReturnDisplayName="Enabled"))
	bool IsSmartObjectEnabled() const;

	/**
	 * Returns the enabled state of the smart object based on a specific reason.
	 * @param ReasonTag Valid Tag to test if enabled for a specific reason. Method will ensure if not valid (i.e. None).
	 * @return True when associated smart object is set to be enabled. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Is SmartObject Enabled (for specific reason)", ReturnDisplayName="Enabled"))
	bool IsSmartObjectEnabledForReason(FGameplayTag ReasonTag) const;

	FSmartObjectComponentEventNativeSignature& GetOnSmartObjectEventNative() { return OnSmartObjectEventNative; }

	/** Returns true if the Smart Object component is registered to the Smart Object subsystem. Depending on the update order, sometimes it is possible that the subsystem gets enabled after the component. */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	bool IsBoundToSimulation() const { return EventDelegateHandle.IsValid(); }

#if WITH_EDITORONLY_DATA
	static FOnSmartObjectChanged& GetOnSmartObjectChanged() { return OnSmartObjectChanged; }
#endif // WITH_EDITORONLY_DATA

protected:
	friend FSmartObjectComponentInstanceData;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void OnRuntimeEventReceived(const FSmartObjectEventData& Event);
	
	UPROPERTY(BlueprintAssignable, Category = SmartObject, meta=(DisplayName = "OnSmartObjectEvent"))
	FSmartObjectComponentEventSignature OnSmartObjectEvent;

	/** Native version of OnSmartObjectEvent. */
	FSmartObjectComponentEventNativeSignature OnSmartObjectEventNative;

	UFUNCTION(BlueprintImplementableEvent, Category = SmartObject, meta=(DisplayName = "OnSmartObjectEventReceived"))
	void ReceiveOnEvent(const FSmartObjectEventData& EventData, const AActor* Interactor);

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	void RegisterToSubsystem();
	void UnregisterFromSubsystem(const ESmartObjectUnregistrationType UnregistrationType);

	/** Reference to Smart Object Definition Asset with parameters. */
	UPROPERTY(EditAnywhere, Category = SmartObject, Replicated, meta = (DisplayName="Definition"))
	FSmartObjectDefinitionReference DefinitionRef;

	/** RegisteredHandle != FSmartObjectHandle::Invalid when registered into a collection by SmartObjectSubsystem */
	UPROPERTY(Transient, VisibleAnywhere, Category = SmartObject, BlueprintReadOnly, Replicated)
	FSmartObjectHandle RegisteredHandle;

	ESmartObjectRegistrationType RegistrationType = ESmartObjectRegistrationType::NotRegistered;

	FDelegateHandle EventDelegateHandle;
	
	/** 
	 * Controls whether a given SmartObject can be aggregated in SmartObjectPersistentCollections. SOs in collections
	 * can be queried and reasoned about even while the actual Actor and its components are not streamed in.
	 * By default SmartObjects are not placed in collections and are active only as long as the owner-actor remains
	 * loaded and active (i.e. not streamed out).
	 */
	UPROPERTY(config, EditAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bCanBePartOfCollection = false;

#if WITH_EDITORONLY_DATA
	static FOnSmartObjectChanged OnSmartObjectChanged;
#endif // WITH_EDITORONLY_DATA

private:
	// Do not use directly from native code, use GetDefinition() / SetDefinition() instead.
	// Also Keeping blueprint accessors for convenience and deprecation purposes.
	UPROPERTY(Transient, Category = SmartObject, BlueprintSetter = SetDefinition, BlueprintGetter = GetDefinition, meta = (DisplayName="Definition Asset"))
	mutable TObjectPtr<USmartObjectDefinition> CachedDefinitionAssetVariation = nullptr;

#if WITH_EDITORONLY_DATA
	/** return true if applied or false if already applied */
	bool ApplyDeprecation();

	/** return true if applied or  false if already applied */
	bool ApplyParentDeprecation();

	/** flag to keep track of the deprecation status of the object */
	UPROPERTY()
	bool bDeprecationApplied = false;

	UPROPERTY(Category = SmartObject, BlueprintSetter = SetDefinition, BlueprintGetter = GetDefinition, meta = (DeprecatedProperty, DisplayName="Deprecated Definition Asset", BlueprintPrivate))
	TObjectPtr<USmartObjectDefinition> DefinitionAsset_DEPRECATED;
#endif//WITH_EDITOR
};


/** Used to store SmartObjectComponent data during RerunConstructionScripts */
USTRUCT()
struct FSmartObjectComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()

public:
	FSmartObjectComponentInstanceData() = default;

	explicit FSmartObjectComponentInstanceData(const USmartObjectComponent* SourceComponent, const FSmartObjectDefinitionReference& Ref)
		: FActorComponentInstanceData(SourceComponent)
		, SmartObjectDefinitionRef(Ref)
	{}

	const FSmartObjectDefinitionReference& GetSmartObjectDefinitionReference() const { return SmartObjectDefinitionRef; }

protected:
	virtual bool ContainsData() const override;
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;

	UPROPERTY()
	FSmartObjectDefinitionReference SmartObjectDefinitionRef;
};
