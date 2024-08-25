// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once



#include "ISourceControlChangelist.h"
#include "ISourceControlChangelistState.h"

#include "DataValidationChangelist.generated.h"

/**
 * Changelist abstraction to allow changelist-level data validation
 */
UCLASS(config = Editor)
class DATAVALIDATION_API UDataValidationChangelist : public UObject
{
	GENERATED_BODY()

public:
	/** Default constructor with nothing */
	UDataValidationChangelist() = default;

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	/** Initializes from a list of file states as a pseudo-changelist */
	void Initialize(TConstArrayView<FSourceControlStateRef> InFileStates);
	/** Initializes from a changelist reference, querying the state from the provider */
	void Initialize(FSourceControlChangelistPtr InChangelist);
	/** Initializes from an already-queried changelist state */
	void Initialize(FSourceControlChangelistStateRef InChangelistState);

	static void GatherDependencies(const FName& InPackageName, TSet<FName>& OutDependencies);
	static FString GetPrettyPackageName(const FName& InPackageName);

	/** Changelist to validate - may be null if this was constructed from a list of files */
	FSourceControlChangelistPtr Changelist;
	
	// Asset files in the changelist 
	TArray<FName> ModifiedPackageNames;
	TArray<FName> DeletedPackageNames;
	
	// Non-asset files in the changelist
	TArray<FString> ModifiedFiles;
	TArray<FString> DeletedFiles;
	
	FText Description;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "EditorSubsystem.h"
#include "Engine/EngineTypes.h"
#include "UObject/Interface.h"
#endif
