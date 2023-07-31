// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeImportTestStepBase.h"
//#include "InterchangeTestFunction.h"
#include "InterchangeImportTestStepReimport.generated.h"

struct FInterchangeTestFunction;


UCLASS(BlueprintType, Meta = (DisplayName = "Reimport a file"))
class INTERCHANGETESTS_API UInterchangeImportTestStepReimport : public UInterchangeImportTestStepBase
{
	GENERATED_BODY()

public:

	/** The source file to import (path relative to the json script) */
	UPROPERTY(EditAnywhere, Category = General)
	FFilePath SourceFileToReimport;

	/** The type of the asset to reimport. In the case that only one such asset were imported, this is unambiguous */
	UPROPERTY(EditAnywhere, Category = General)
	TSubclassOf<UObject> AssetTypeToReimport;

	/** If there were multiple assets of the above type imported, specify the concrete name here */
	UPROPERTY(EditAnywhere, Category = General)
	FString AssetNameToReimport;

	/** Whether imported assets should be saved and freshly reloaded after import */
	UPROPERTY(EditAnywhere, Category = General)
	bool bSaveThenReloadImportedAssets = true;

public:
	// UInterchangeImportTestStepBase interface
	virtual TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr>
		StartStep(FInterchangeImportTestData& Data) override;
	virtual FTestStepResults FinishStep(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo) override;
	virtual FString GetContextString() const override;
};
