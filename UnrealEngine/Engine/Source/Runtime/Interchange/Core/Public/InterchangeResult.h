// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangeResult.generated.h"

class UClass;
class UInterchangePipelineBase;


UENUM()
enum class EInterchangeResultType
{
	Success,
	Warning,
	Error
};


/**
 * Base class for the result from an Interchange operation.
 */
UCLASS(Experimental)
class INTERCHANGECORE_API UInterchangeResult : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Return the severity of the message: success, warning, error.
	 */
	virtual EInterchangeResultType GetResultType() const { return EInterchangeResultType::Success; }

	/**
	 * Return a user-readable message.
	 */
	virtual FText GetText() const { return FText(); }

	/**
	 * Serialize this UInterchangeResult object to Json.
	 */
	FString ToJson();

	/**
	 * Create a new UInterchangeResult-derived object from a Json representation.
	 */
	static UInterchangeResult* FromJson(const FString& JsonString);


	UPROPERTY()
	FString SourceAssetName;

	UPROPERTY()
	FString DestinationAssetName;

	UPROPERTY()
	TObjectPtr<const UClass> AssetType;

	UPROPERTY()
	FString InterchangeKey;
};


/**
 * Class representing a succesful result.
 */
UCLASS()
class INTERCHANGECORE_API UInterchangeResultSuccess : public UInterchangeResult
{
	GENERATED_BODY()

public:

	virtual EInterchangeResultType GetResultType() const override { return EInterchangeResultType::Success; }
	virtual FText GetText() const override { return FText(); }
};


/**
 * Base class representing a warning result.
 */
UCLASS()
class INTERCHANGECORE_API UInterchangeResultWarning : public UInterchangeResult
{
	GENERATED_BODY()

public:

	virtual EInterchangeResultType GetResultType() const override { return EInterchangeResultType::Warning; }
};


/**
 * Base class representing a error result.
 */
UCLASS()
class INTERCHANGECORE_API UInterchangeResultError : public UInterchangeResult
{
	GENERATED_BODY()

public:

	virtual EInterchangeResultType GetResultType() const override { return EInterchangeResultType::Error; }
};


UCLASS()
class INTERCHANGECORE_API UInterchangeResultWarning_Generic : public UInterchangeResultWarning
{
	GENERATED_BODY()

public:

	virtual FText GetText() const override { return Text; }

	UPROPERTY()
	FText Text;
};


UCLASS()
class INTERCHANGECORE_API UInterchangeResultError_Generic : public UInterchangeResultError
{
	GENERATED_BODY()

public:

	virtual FText GetText() const override { return Text; }

	UPROPERTY()
	FText Text;
};

UCLASS()
class INTERCHANGECORE_API UInterchangeResultError_ReimportFail : public UInterchangeResultError
{
	GENERATED_BODY()

public:

	virtual FText GetText() const override { return NSLOCTEXT("InterchangeResultNS", "UInterchangeResultError_ReimportFail_GetText", "Re-import Fail, see log for more detail."); }
};
