// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Tickable.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimManager.generated.h"

class UContextualAnimSceneActorComponent;
class UContextualAnimSceneAsset;
class UContextualAnimSceneInstance;
class AActor;
class UWorld;

UCLASS()
class CONTEXTUALANIMATION_API UContextualAnimManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	friend class FContextualAnimViewModel;

	UContextualAnimManager(const FObjectInitializer& ObjectInitializer);

	virtual UWorld* GetWorld() const override;

	// FTickableGameObject begin
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override;
	// FTickableGameObject end

	static UContextualAnimManager* Get(const UWorld* World);

	void RegisterSceneActorComponent(UContextualAnimSceneActorComponent* SceneActorComp);

	void UnregisterSceneActorComponent(UContextualAnimSceneActorComponent* SceneActorComp);

	/** Starts an scene instance with the supplied actors for each role ignoring selection criteria */
	UContextualAnimSceneInstance* ForceStartScene(const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimStartSceneParams& Params);
	
	/** Attempts to start an scene instance with the supplied actors using the first valid set based on selection criteria */
	UContextualAnimSceneInstance* TryStartScene(const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimStartSceneParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Manager")
	bool TryStopSceneWithActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Manager")
	bool IsActorInAnyScene(AActor* Actor) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Manager")
	UContextualAnimSceneInstance* GetSceneWithActor(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Manager", meta = (WorldContext = "WorldContextObject"))
	static UContextualAnimManager* GetContextualAnimManager(UObject* WorldContextObject);

	FORCEINLINE const TSet<TObjectPtr<UContextualAnimSceneActorComponent>>& GetSceneActorCompContainer() const { return SceneActorCompContainer; };

	/** Attempts to start an scene instance with the supplied actors using the first valid set based on selection criteria */
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Manager", meta = (DisplayName = "Try Start Scene"))
	UContextualAnimSceneInstance* BP_TryStartScene(const UContextualAnimSceneAsset* SceneAsset, const FContextualAnimStartSceneParams& Params);

protected:

	/** Container for all SceneActorComps in the world */
	UPROPERTY()
	TSet<TObjectPtr<UContextualAnimSceneActorComponent>> SceneActorCompContainer;

	UPROPERTY()
	TArray<TObjectPtr<UContextualAnimSceneInstance>> Instances;

	UFUNCTION()
	void OnSceneInstanceEnded(UContextualAnimSceneInstance* SceneInstance);
};