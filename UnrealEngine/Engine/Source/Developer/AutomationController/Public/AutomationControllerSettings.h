// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "IAutomationReport.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#include "AutomationControllerSettings.generated.h"

/*
* Describes a base filter for a test group.
*/
USTRUCT()
struct FAutomatedTestFilterBase
{
	GENERATED_BODY()

public:

	FAutomatedTestFilterBase(FString InContains, bool InMatchFromStart = false, bool InMatchFromEnd = false)
		: Contains(InContains), MatchFromStart(InMatchFromStart), MatchFromEnd(InMatchFromEnd)
	{
	}

	FAutomatedTestFilterBase() : FAutomatedTestFilterBase(TEXT("")) {}

	virtual ~FAutomatedTestFilterBase() {}

	/** String that the test must contain */
	UPROPERTY(Config)
		FString Contains;

	/** If true start matching from the start of the string, else anywhere */
	UPROPERTY(Config)
		bool MatchFromStart;

	/** If true start matching from the end of the string, else anywhere */
	UPROPERTY(Config)
		bool MatchFromEnd;

	virtual bool PassesFilter(const TSharedPtr< IAutomationReport >& InReport) const
	{
		bool bMeetsMatch = true;

		if (MatchFromStart || MatchFromEnd)
		{
			if (MatchFromStart)
			{
				bMeetsMatch = InReport->GetFullTestPath().StartsWith(Contains);
			}

			if (MatchFromEnd && bMeetsMatch)
			{
				bMeetsMatch = InReport->GetFullTestPath().EndsWith(Contains);
			}
		}
		else
		{
			bMeetsMatch = InReport->GetFullTestPath().Contains(Contains);
		}

		return bMeetsMatch;
	}
};

/*
* Describes a filter for a test group with exclude option.
*/
USTRUCT()
struct FAutomatedTestFilter : public FAutomatedTestFilterBase
{
	GENERATED_BODY()

public:

	FAutomatedTestFilter(FString InContains, bool InMatchFromStart = false, bool InMatchFromEnd = false)
		: FAutomatedTestFilterBase(InContains, InMatchFromStart, InMatchFromEnd)
	{
	}

	FAutomatedTestFilter() : FAutomatedTestFilter(TEXT("")) {}

	/** List of filters to exclude */
	UPROPERTY(Config)
		TArray<FAutomatedTestFilterBase> Exclude;

	virtual bool PassesFilter(const TSharedPtr< IAutomationReport >& InReport) const override
	{
		bool bMeetsMatch = Super::PassesFilter(InReport);

		if (bMeetsMatch && !Exclude.IsEmpty())
		{
			// Apply exclusion rules
			for (const FAutomatedTestFilterBase& Filter : Exclude)
			{
				if (Filter.PassesFilter(InReport))
				{
					return false;
				}
			}
		}

		return bMeetsMatch;
	}
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

	/** Whether to automatically expand Automation Tests tree subgroups that have single non-leaf item as a child (default=true) */
	UPROPERTY(Config)
	bool bAutoExpandSingleItemSubgroups;

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
