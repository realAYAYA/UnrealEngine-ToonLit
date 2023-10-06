// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeImportTestStepBase.h"
#include "InterchangeImportTestStepImport.generated.h"


class UInterchangePipelineBase;


UCLASS(BlueprintType, Meta = (DisplayName = "Import a file"))
class INTERCHANGETESTS_API UInterchangeImportTestStepImport : public UInterchangeImportTestStepBase
{
	GENERATED_BODY()

public:

	/** The source file to import (path relative to the json script) */
	UPROPERTY(EditAnywhere, Category = General)
	FFilePath SourceFile;

	/** The pipeline stack to use when importing (an empty array will use the defaults) */
	UPROPERTY(EditAnywhere, Instanced, Category = General)
	TArray<TObjectPtr<UInterchangePipelineBase>> PipelineStack;

	/** Whether the destination folder should be emptied prior to import */
	UPROPERTY(EditAnywhere, Category = General)
	bool bEmptyDestinationFolderPriorToImport = true;

	/** Whether imported assets should be saved and freshly reloaded after import */
	UPROPERTY(EditAnywhere, Category = General, Meta=(EditCondition="!bImportIntoLevel"))
	bool bSaveThenReloadImportedAssets = true;

	/** Whether we should use the import into level workflow */
	UPROPERTY(EditAnywhere, Category = General)
	bool bImportIntoLevel = false;

public:
	// UInterchangeImportTestStepBase interface
	virtual TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr>
		StartStep(FInterchangeImportTestData& Data) override;
	virtual FTestStepResults FinishStep(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo) override;
	virtual FString GetContextString() const override;
};
