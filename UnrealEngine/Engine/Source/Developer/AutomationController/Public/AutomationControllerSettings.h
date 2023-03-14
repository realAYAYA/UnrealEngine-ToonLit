// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#include "AutomationControllerSettings.generated.h"

/*
* Describes a filter for a test group.
*/
USTRUCT()
struct FAutomatedTestFilter
{
	GENERATED_USTRUCT_BODY()

public:

	FAutomatedTestFilter(FString InContains, bool InMatchFromStart = false, bool InMatchFromEnd = false)
		: Contains(InContains), MatchFromStart(InMatchFromStart), MatchFromEnd(InMatchFromEnd)
	{
	}

	FAutomatedTestFilter() : FAutomatedTestFilter(TEXT("")) {}

	/** String that the test must contain */
	UPROPERTY(Config)
		FString Contains;

	/** If true start matching from the start of the string, else anywhere */
	UPROPERTY(Config)
		bool MatchFromStart;

	/** If true start matching from the end of the string, else anywhere */
	UPROPERTY(Config)
		bool MatchFromEnd;	
};

/*
 *	Describes a group of tests. Each group has a name and a set of filters that determine group membership
 */
USTRUCT()
struct FAutomatedTestGroup
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(Config)
		FString Name;

	UPROPERTY(Config)
		TArray<FAutomatedTestFilter> Filters;
};



/**
 * Implements the Editor's user settings.
 */
UCLASS(config = Engine, defaultconfig)
class AUTOMATIONCONTROLLER_API UAutomationControllerSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	
	/** List of user-defined test groups */
	UPROPERTY(Config)
	TArray<FAutomatedTestGroup> Groups;

	/** Whether to suppress log from test results (default=false) */
	UPROPERTY(Config)
	bool bSuppressLogErrors;

	/** Whether to suppress log warnings from test results (default=false) */
	UPROPERTY(Config)
	bool bSuppressLogWarnings;

	/** Whether to treat log warnings as log errors (default=true) */
	UPROPERTY(Config)
	bool bElevateLogWarningsToErrors;

	/** Log categories where warnings/errors will not affect the result of tests. A finer-grained way of preventing rogue systems from leading to test warnings/errors */
	UPROPERTY(Config)
	TArray<FString> SuppressedLogCategories;

	/** Whether to keep the PIE Open in the editor at the end of a test pass (default=false) */
	UPROPERTY(Config)
	bool bKeepPIEOpen;

private:
	/** Whether to treat log warnings as test errors (default=true) */
	UPROPERTY(Config, Meta = (DeprecatedProperty, DeprecationMessage = "Use bElevateLogWarningsToErrors instead."))
	bool bTreatLogWarningsAsTestErrors;
	
public:
	/** How long to wait between test updates (default=1sec)*/
	UPROPERTY(Config)
	float CheckTestIntervalSeconds;
	
	/** The maximum response wait time for detecting a lost game instance (default=300sec)*/
	UPROPERTY(Config)
	float GameInstanceLostTimerSeconds;

	/** Path to where telemetry files are saved (default=<project>/Saved/Automation/Telemetry/)*/
	UPROPERTY(Config)
	FString TelemetryDirectory;

	/** Whether to reset data stored in telemetry file (default=false) */
	UPROPERTY(Config)
	bool bResetTelemetryStorageOnNewSession;

	virtual void PostInitProperties() override;
};
