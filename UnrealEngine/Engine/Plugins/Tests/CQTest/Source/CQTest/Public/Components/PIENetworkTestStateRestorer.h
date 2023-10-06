// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/SoftObjectPath.h"
#include "Templates/SubclassOf.h"

class UWorld;
class AGameModeBase;

//Object which saves the state of the game before starting the test,
//Then restores it after the test is complete.
class CQTEST_API FPIENetworkTestStateRestorer
{
public:
	FPIENetworkTestStateRestorer() = default;
	FPIENetworkTestStateRestorer(const FSoftClassPath InGameInstanceClass, TSubclassOf<AGameModeBase> InGameMode);

	void Restore();

private:
	bool SetWasLoadedFlag = false;
	FSoftClassPath OriginalGameInstance = FSoftClassPath();
	TSubclassOf<AGameModeBase> OriginalGameMode = nullptr;
};
