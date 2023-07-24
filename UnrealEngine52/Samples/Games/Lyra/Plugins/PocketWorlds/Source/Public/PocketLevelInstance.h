// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/BoxSphereBounds.h"

#include "UObject/ObjectPtr.h"
#include "PocketLevelInstance.generated.h"

class UPocketLevelSubsystem;

class ULevelStreamingDynamic;
class ULocalPlayer;
class UPocketLevel;
class UPocketLevelInstance;
class UWorld;
struct FFrame;

DECLARE_MULTICAST_DELEGATE_OneParam(FPocketLevelInstanceEvent, UPocketLevelInstance*);

/**
 *
 */
UCLASS(Within = PocketLevelSubsystem, BlueprintType)
class POCKETWORLDS_API UPocketLevelInstance : public UObject
{
	GENERATED_BODY()

public:
	UPocketLevelInstance();

	virtual void BeginDestroy() override;

	void StreamIn();
	void StreamOut();

	FDelegateHandle AddReadyCallback(FPocketLevelInstanceEvent::FDelegate Callback);
	void RemoveReadyCallback(FDelegateHandle CallbackToRemove);

	virtual class UWorld* GetWorld() const override { return World; }

private:
	bool Initialize(ULocalPlayer* LocalPlayer, UPocketLevel* PocketLevel, FVector SpawnPoint);

	UFUNCTION()
	void HandlePocketLevelLoaded();

	UFUNCTION()
	void HandlePocketLevelShown();

private:
	UPROPERTY()
	TObjectPtr<ULocalPlayer> LocalPlayer;

	UPROPERTY()
	TObjectPtr<UPocketLevel> PocketLevel;

	UPROPERTY()
	TObjectPtr<UWorld> World;

	UPROPERTY()
	TObjectPtr<ULevelStreamingDynamic> StreamingPocketLevel;

	FPocketLevelInstanceEvent OnReadyEvent;

	FBoxSphereBounds Bounds;

	friend class UPocketLevelSubsystem;
};
