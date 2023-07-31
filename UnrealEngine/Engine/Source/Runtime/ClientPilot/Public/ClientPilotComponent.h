// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClientPilotBlackboardManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ClientPilotComponent.generated.h"

class UClientPilotBlackboard;

UCLASS()
class CLIENTPILOT_API UClientPilotComponent : public UObject
{
	GENERATED_BODY()
public:
	UClientPilotComponent();
	UClientPilotBlackboard* GetBlackboardInstance();
	virtual void ThinkAndAct();

};