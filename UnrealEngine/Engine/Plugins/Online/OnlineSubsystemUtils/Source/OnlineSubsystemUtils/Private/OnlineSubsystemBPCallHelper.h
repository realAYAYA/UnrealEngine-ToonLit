// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/CoreOnline.h"

class APlayerController;
class IOnlineSubsystem;

// Helper class for various methods to reduce the call hierarchy
struct FOnlineSubsystemBPCallHelper
{
public:
	FOnlineSubsystemBPCallHelper(const TCHAR* CallFunctionContext, UObject* WorldContextObject, FName SystemName = NAME_None);

	void QueryIDFromPlayerController(APlayerController* PlayerController);

	bool IsValid() const
	{
		return UserID.IsValid() && (OnlineSub != nullptr);
	}

public:
	FUniqueNetIdPtr UserID;
	IOnlineSubsystem* const OnlineSub;
	const TCHAR* FunctionContext;
};

#define INVALID_INDEX -1



