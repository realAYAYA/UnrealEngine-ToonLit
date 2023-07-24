// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Info.h"
#include "MassReplicationTypes.h"
#include "Engine/World.h"

#include "MassClientBubbleInfoBase.generated.h"

struct FMassClientBubbleSerializerBase;

/** The info actor base class that provides the actual replication */
UCLASS()
class MASSREPLICATION_API AMassClientBubbleInfoBase : public AInfo
{
	GENERATED_BODY()

public:
	AMassClientBubbleInfoBase(const FObjectInitializer& ObjectInitializer);

	void SetClientHandle(FMassClientHandle InClientHandle);

protected:
	virtual void PostInitProperties() override;

	// Called either on PostWorldInit() or PostInitProperties()
	virtual void InitializeForWorld(UWorld& World);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues);

protected:
	FDelegateHandle OnPostWorldInitDelegateHandle;
	TArray<FMassClientBubbleSerializerBase*> Serializers;
};
