// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/EngineTypes.h"
#include "DataValidationManager.generated.h"

class FLogWindowManager;
class SLogWindow;
class SWindow;
class UUnitTest;

/**
 * Manages centralized execution and tracking of data validation, as well as handling console commands,
 * and some misc tasks like local log hooking
 */

UCLASS(config=Editor, Deprecated)
class DATAVALIDATION_API UDEPRECATED_DataValidationManager : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Static getter for the data validation manager
	 *
	 * @return	Returns the data validation manager
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use the functions on the subsystem instead.")
	static UDEPRECATED_DataValidationManager* Get();

	/**
	 * Initialize the data validation manager
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use the functions on the subsystem instead.")
	virtual void Initialize();

	/**
	 * Destructor for handling removal of log registration
	 */
	virtual ~UDEPRECATED_DataValidationManager() override;

	/**
	 * @return Returns Valid if the object contains valid data; returns Invalid if the object contains invalid data; returns NotValidated if no validations was performed on the object
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use IsObjectValid on the subsystem instead.")
	virtual EDataValidationResult IsObjectValid(UObject* InObject, TArray<FText>& ValidationErrors) const;

	/**
	 * @return Returns Valid if the object pointed to by AssetData contains valid data; returns Invalid if the object contains invalid data or does not exist; returns NotValidated if no validations was performed on the object
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use IsAssetValid on the subsystem instead.")
	virtual EDataValidationResult IsAssetValid(FAssetData& AssetData, TArray<FText>& ValidationErrors) const;

	/**
	 * Called to validate assets from either the UI or a commandlet
	 * @param bSkipExcludedDirectories If true, will not validate files in excluded directories
	 * @param bShowIfNoFailures If true, will add notifications for files with no validation and display even if everything passes
	 * @returns Number of assets with validation failures
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use IsAssetValid on the subsystem instead.")
	virtual int32 ValidateAssets(TArray<FAssetData> AssetDataList, bool bSkipExcludedDirectories = true, bool bShowIfNoFailures = true) const;

	/**
	 * Called to validate from an interactive save
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use IsAssetValid on the subsystem instead.")
	virtual void ValidateOnSave(TArray<FAssetData> AssetDataList) const;

	/**
	 * Schedule a validation of a saved package, this will activate next frame by default so it can combine them
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use ValidateSavedPackage on the subsystem instead.")
	virtual void ValidateSavedPackage(FName PackageName);

protected:
	/** 
	 * @return Returns true if the current Path should be skipped for validation. Returns false otherwise.
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use IsPathExcludedFromValidation on the subsystem instead.")
	virtual bool IsPathExcludedFromValidation(const FString& Path) const;

	/**
	 * Handles validating all pending save packages
	 */
	UE_DEPRECATED(4.23, "Use GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() and use ValidateAllSavedPackages on the subsystem instead.")
	void ValidateAllSavedPackages();

	/**
	 * Directories to ignore for data validation. Useful for test assets
	 */
	UPROPERTY(config, meta = (Deprecated, DeprecationMessage = "UDataValidationManager's ExcludedDirectories is deprecated, use UEditorValidatorSubsystem's ExcludedDirectories instead."))
	TArray<FDirectoryPath> ExcludedDirectories;

	/**
	 * Rather it should validate assets on save inside the editor
	 */
	UPROPERTY(config, meta = (Deprecated, DeprecationMessage = "UDataValidationManager's bValidateOnSave is deprecated, use UEditorValidatorSubsystem's bValidateOnSave instead."))
	bool bValidateOnSave;

	/** List of saved package names to validate next frame */
	TArray<FName> SavedPackagesToValidate;

private:

	/** The class to instantiate as the manager object. Defaults to this class but can be overridden */
	UPROPERTY(config)
	FSoftClassPath DataValidationManagerClassName;
};
