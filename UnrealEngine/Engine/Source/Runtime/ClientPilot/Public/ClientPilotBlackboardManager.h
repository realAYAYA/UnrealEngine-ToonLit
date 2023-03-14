// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClientPilotBlackboard.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "ClientPilotBlackboardManager.generated.h"

class UClientPilotBlackboard;


UCLASS()
class CLIENTPILOT_API UClientPilotBlackboardManager : public UObject
{
GENERATED_BODY()
protected:
	static UClientPilotBlackboardManager * ObjectInstance;

public:
	static UClientPilotBlackboardManager * GetInstance();
	UPROPERTY()
	TObjectPtr<UClientPilotBlackboard> PilotBlackboard;
};