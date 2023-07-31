// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Managers/MLAdapterManager.h"
#include "MLAdapterNoRPCManager.generated.h"

/**
 * UMLAdapterNoRPCManager won't start the RPC server and will immediately start a session and spawn the default agent. This is
 * intended to be used when there will only be a single inference agent that exists for the life of the game.
 */
UCLASS()
class MLADAPTER_API UMLAdapterNoRPCManager : public UMLAdapterManager
{
	GENERATED_BODY()
public:
	virtual void OnPostWorldInit(UWorld* World, const UWorld::InitializationValues) override;
};
