// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RuntimeGen/GenSources/PCGGenSourceBase.h"

#include "Components/ActorComponent.h"

#include "PCGGenSourceComponent.generated.h"

namespace EEndPlayReason { enum Type : int; }

class FPCGGenSourceManager;

/**
 * UPCGGenSourceComponent makes the actor this is attached to act as a PCG runtime generation source.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), DisplayName = "PCG Generation Source", meta = (BlueprintSpawnableComponent, PrioritizeCategories = "PCG"))
class PCG_API UPCGGenSourceComponent : public UActorComponent, public IPCGGenSourceBase
{
	GENERATED_BODY()

	UPCGGenSourceComponent(const FObjectInitializer& InObjectInitializer) : Super(InObjectInitializer) {}
	~UPCGGenSourceComponent();

public:
	//~Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~End UActorComponent Interface

	//~Begin IPCGGenSourceInterface
	/** Returns the world space position of this gen source. */
	virtual TOptional<FVector> GetPosition() const override;

	/** Returns the normalized forward vector of this gen source. */
	virtual TOptional<FVector> GetDirection() const override;
	//~End IPCGGenSourceInterface

protected:
	FPCGGenSourceManager* GetGenSourceManager() const;
};
