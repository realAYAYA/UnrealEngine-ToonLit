// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "V2/WebAPISwaggerFactory.h"
#include "UObject/Object.h"
#include "V3/WebAPIOpenAPIProvider.h"

#include "WebAPIOpenAPIFactory.generated.h"

UCLASS()
class WEBAPIOPENAPI_API UWebAPIOpenAPIAssetData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString FileContents;
};

/** Handles importing for OpenAPI V2 files. */
UCLASS()
class WEBAPIOPENAPI_API UWebAPIOpenAPIFactory
	: public UWebAPIOpenAPIFactoryBase
{
	GENERATED_BODY()

public:
	UWebAPIOpenAPIFactory();

	virtual bool CanImportWebAPI(const FString& InFileName, const FString& InFileContents) override;
	virtual TFuture<bool> ImportWebAPI(UWebAPIDefinition* InDefinition, const FString& InFileName, const FString& InFileContents) override;

protected:
	TSharedPtr<FWebAPIOpenAPIProvider> Provider;
};
