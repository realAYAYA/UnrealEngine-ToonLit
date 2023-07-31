// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImportTestFunctions/InterchangeResultImportTestFunctions.h"
#include "InterchangeResultsContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeResultImportTestFunctions)


UClass* UInterchangeResultImportTestFunctions::GetAssociatedAssetType() const
{
	return UInterchangeResultsContainer::StaticClass();
}


FInterchangeTestFunctionResult UInterchangeResultImportTestFunctions::CheckIfErrorOrWarningWasGenerated(UInterchangeResultsContainer* ResultsContainer, TSubclassOf<UInterchangeResult> ErrorOrWarningClass)
{
	FInterchangeTestFunctionResult Result;

	bool bFoundType = false;
	for (UInterchangeResult* ResultObject : ResultsContainer->GetResults())
	{
		if (ResultObject->IsA(ErrorOrWarningClass))
		{
			bFoundType = true;
			// If we matched the message type, remove it from the results container so it won't flag as an error at the end of the test.
			// Note: we can do this while iterating through results, because UInterchangeResultsContainer::GetResults passes the array by value,
			// for thread-safety purposes.
			ResultsContainer->RemoveResult(ResultObject);
		}
	}

	if (!bFoundType)
	{
		Result.AddError(FString::Printf(TEXT("No error or warning of class %s was generated."), *ErrorOrWarningClass.Get()->GetName()));
	}

	return Result;
}

