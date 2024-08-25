// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "MassSubsystemBase.generated.h"


namespace UE::Mass::Subsystems
{
	struct FInitializationState
	{
		uint8 bInitializeCalled : 1 = false;
		uint8 bPostInitializeCalled : 1 = false;
		uint8 bOnWorldBeginPlayCalled : 1 = false;
	};
}

/** 
 * The sole responsibility of this world subsystem class is to serve functionality common to all 
 * Mass-related UWorldSubsystem-based subsystems, like whether the subsystems should get created at all. 
 */
UCLASS(Abstract)
class MASSENTITY_API UMassSubsystemBase : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool AreRuntimeMassSubsystemsAllowed(UObject* Outer);

protected:
	//~USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~End of USubsystem interface

	/**
	 * Needs to be called in Initialize for subsystems we want to behave properly when dynamically added after UWorld::BeginPlay
	 * (for example via GameplayFeatureActions). This is required for subsystems relying on their PostInitialize and/or OnWorldBeginPlay called.
	 */
	void HandleLateCreation();

	/**
	 * Tracks which initialization function had already been called. Requires the child classes to call Super implementation
	 * for their Initialize, PostInitialize, Deinitialize and OnWorldBeginPlayCalled overrides
	 */
	UE::Mass::Subsystems::FInitializationState InitializationState;
};

/**
 * The sole responsibility of this tickable world subsystem class is to serve functionality common to all
 * Mass-related UTickableWorldSubsystem-based subsystems, like whether the subsystems should get created at all.
 */
UCLASS(Abstract)
class MASSENTITY_API UMassTickableSubsystemBase : public UTickableWorldSubsystem
{
	GENERATED_BODY()

protected:
	//~USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~End of USubsystem interface

	/**
	 * Needs to be called in Initialize for subsystems we want to behave properly when dynamically added after UWorld::BeginPlay
	 * (for example via GameplayFeatureActions). This is required for subsystems relying on their PostInitialize and/or OnWorldBeginPlay called.
	 */
	void HandleLateCreation();

private:
	/** 
	 * Tracks which initialization function had already been called. Requires the child classes to call Super implementation
	 * for their Initialize, PostInitialize, Deinitialize and OnWorldBeginPlayCalled overrides
	 */
	UE::Mass::Subsystems::FInitializationState InitializationState;
};
