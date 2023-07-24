// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Math/MathFwd.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ClientPilotBlackboard.generated.h"

UCLASS()
class CLIENTPILOT_API UClientPilotBlackboard : public UObject
{
GENERATED_BODY()
public:
	virtual void InitializeFromProfile(FString CategoryToUse);

	void AddOrUpdateValue(FString KeyName, float Value);
	void AddOrUpdateValue(FString KeyName, int Value);
	void AddOrUpdateValue(FString KeyName, FString Value);
	void AddOrUpdateValue(FString KeyName, FVector Value);

	FString GetStringValue(FString KeyName);
	int GetIntValue(FString KeyName);
	float GetFloatValue(FString KeyName);
	FVector GetVectorValue(FString KeyName);

	void RemoveKey(FString Key) 
	{ 
		Blackboard.Remove(Key);
	}
	void ResetBlackboard()
	{
		Blackboard.Empty();
	}

protected:
	TMap<FString, FString> Blackboard;
};