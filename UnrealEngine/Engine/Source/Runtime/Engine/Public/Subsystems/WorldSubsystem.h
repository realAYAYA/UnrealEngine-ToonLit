// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystem.h"
#include "Engine/EngineTypes.h"
#include "Tickable.h"
#include "WorldSubsystem.generated.h"

/**
 * UWorldSubsystem
 * Base class for auto instanced and initialized systems that share the lifetime of a UWorld
 */
UCLASS(Abstract)
class ENGINE_API UWorldSubsystem : public USubsystem
{
	GENERATED_BODY()

public:
	UWorldSubsystem();

	virtual UWorld* GetWorld() const override final;

	/**
	 * Returns a reference to the UWorld this subsystem is contained within.
	 * @note This should not be called on default object since the method assumes a valid outer world.
	 */
	UWorld& GetWorldRef() const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Called once all UWorldSubsystems have been initialized */
	virtual void PostInitialize() {}
	
	/** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
	virtual void OnWorldBeginPlay(UWorld& InWorld) {}

	/** Called after world components (e.g. line batcher and all level components) have been updated */
	virtual void OnWorldComponentsUpdated(UWorld& World) {}

	/** Updates sub-system required streaming levels (called by world's UpdateStreamingState function) */
	virtual void UpdateStreamingState() {}

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const;
};

/**
 * UTickableWorldSubsystem
 * Base class for auto instanced and initialized systems that share the lifetime of a UWorld and are ticking along with it
 */
UCLASS(Abstract)
class ENGINE_API UTickableWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UTickableWorldSubsystem();

	// FTickableGameObject implementation Begin
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsAllowedToTick() const override final;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override PURE_VIRTUAL(UTickableWorldSubsystem::GetStatId, return TStatId(););
	// FTickableGameObject implementation End

	// USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem implementation End

	bool IsInitialized() const { return bInitialized; }

private:
	bool bInitialized = false;
};
