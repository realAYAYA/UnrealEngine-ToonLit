// Copyright Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestBase.h"
#include "FunctionalTestingModule.h"
#include "AutomationControllerSettings.h"


// statics
bool FFunctionalTestBase::bIsFunctionalTestRunning;
FString FFunctionalTestBase::ActiveTestName;


FFunctionalTestBase::FFunctionalTestBase(const FString& InName, const bool bInComplexTask)
	: FAutomationTestBase(InName, bInComplexTask)
{
	bSuppressLogs = false;
	bIsFunctionalTestRunning = false;
	// CDO not available at this point
	bSuppressLogErrors = false;
	bSuppressLogWarnings = false;
	bElevateLogWarningsToErrors = true;
}

void FFunctionalTestBase::SetLogErrorAndWarningHandlingToDefault()
{
	// Set to project defaults
	UAutomationControllerSettings* Settings = UAutomationControllerSettings::StaticClass()->GetDefaultObject<UAutomationControllerSettings>();
	bSuppressLogErrors = Settings->bSuppressLogErrors;
	bSuppressLogWarnings = Settings->bSuppressLogWarnings;
	bElevateLogWarningsToErrors = Settings->bElevateLogWarningsToErrors;
}

/**
	 * Marks us as actively running a functional test
	 */
void FFunctionalTestBase::SetFunctionalTestRunning(const FString& InName)
{
	if (bIsFunctionalTestRunning)
	{
		UE_LOG(LogFunctionalTest, Error, TEXT("A Functional test is already running!"));
	}

	if (InName.Len() == 0)
	{
		UE_LOG(LogFunctionalTest, Error, TEXT("No test name supplied!"));
	}

	bIsFunctionalTestRunning = true;
	ActiveTestName = InName;
}

/**
 * Marks us as no longer running a test
 */
void FFunctionalTestBase::SetFunctionalTestComplete(const FString& InName)
{
	if (!bIsFunctionalTestRunning)
	{
		UE_LOG(LogFunctionalTest, Error, TEXT("SetFunctionalComplete() called with no active test"));
	}

	if (InName != ActiveTestName)
	{
		UE_LOG(LogFunctionalTest, Error, TEXT("SetFunctionalComplete: Complete name %s did not match active name %s"), *InName, *ActiveTestName);
	}

	bIsFunctionalTestRunning = false;
	ActiveTestName = TEXT("");
}
