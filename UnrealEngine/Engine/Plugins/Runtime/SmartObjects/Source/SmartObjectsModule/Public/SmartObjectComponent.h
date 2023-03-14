// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "SmartObjectTypes.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectComponent.generated.h"

class UAbilitySystemComponent;
struct FSmartObjectRuntime;

UCLASS(Blueprintable, ClassGroup = Gameplay, meta = (BlueprintSpawnableComponent), config = Game, HideCategories = (Activation, AssetUserData, Collision, Cooking, HLOD, Lighting, LOD, Mobile, Mobility, Navigation, Physics, RayTracing, Rendering, Tags, TextureStreaming))
class SMARTOBJECTSMODULE_API USmartObjectComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	explicit USmartObjectComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FBox GetSmartObjectBounds() const;

	const USmartObjectDefinition* GetDefinition() const { return DefinitionAsset; }
	void SetDefinition(USmartObjectDefinition* Definition) { DefinitionAsset = Definition; }

	FSmartObjectHandle GetRegisteredHandle() const { return RegisteredHandle; }
	void SetRegisteredHandle(const FSmartObjectHandle Value) { RegisteredHandle = Value; }

	void OnRuntimeInstanceCreated(FSmartObjectRuntime& RuntimeInstance);
	void OnRuntimeInstanceDestroyed();
	void OnRuntimeInstanceBound(FSmartObjectRuntime& RuntimeInstance);
	void OnRuntimeInstanceUnbound(FSmartObjectRuntime& RuntimeInstance);

protected:
	friend struct FSmartObjectComponentInstanceData;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

	void RegisterToSubsystem();

	void BindTagsDelegates(FSmartObjectRuntime& RuntimeInstance, UAbilitySystemComponent& AbilitySystemComponent);
	void UnbindComponentTagsDelegate();
	void UnbindRuntimeInstanceTagsDelegate(FSmartObjectRuntime& RuntimeInstance);

	UPROPERTY(EditAnywhere, Category = SmartObject, BlueprintReadWrite)
	TObjectPtr<USmartObjectDefinition> DefinitionAsset;

	/** RegisteredHandle != FSmartObjectHandle::Invalid when registered into a collection by SmartObjectSubsystem */
	UPROPERTY(Transient, VisibleAnywhere, Category = SmartObject)
	FSmartObjectHandle RegisteredHandle;

	FDelegateHandle OnComponentTagsModifiedHandle;
	bool bInstanceTagsDelegateBound = false;
};


/** Used to store SmartObjectComponent data during RerunConstructionScripts */
USTRUCT()
struct FSmartObjectComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()

public:
	FSmartObjectComponentInstanceData() = default;

	explicit FSmartObjectComponentInstanceData(const USmartObjectComponent* SourceComponent, USmartObjectDefinition* Asset)
		: FActorComponentInstanceData(SourceComponent)
		, DefinitionAsset(Asset)
	{}

	USmartObjectDefinition* GetDefinitionAsset() const { return DefinitionAsset; }

protected:
	virtual bool ContainsData() const override;
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;

	UPROPERTY()
	TObjectPtr<USmartObjectDefinition> DefinitionAsset = nullptr;
};