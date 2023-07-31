// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeImportTestStepBase.h"
#include "InterchangeImportTestData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeImportTestStepBase)


bool UInterchangeImportTestStepBase::PerformTests(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo)
{
	// Add the Interchange results container to the list of UObject* we pass to the test functions.
	// This way we can match against tests which can operate on the UInterchangeResultsContainer too.
	TArray<UObject*> ResultObjects = Data.ResultObjects;
	ResultObjects.Add(Data.InterchangeResults);

	bool bSuccess = true;

	for (FInterchangeTestFunction& Test : Tests)
	{
		FInterchangeTestFunctionResult Result = Test.Invoke(ResultObjects);

		for (const FString& Warning : Result.GetWarnings())
		{
			ExecutionInfo.AddWarning(Warning);
		}

		for (const FString& Error : Result.GetErrors())
		{
			ExecutionInfo.AddError(Error);
		}

		bSuccess &= Result.IsSuccess();
	}

	return bSuccess;
}

