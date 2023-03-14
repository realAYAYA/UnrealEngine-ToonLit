// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/WebAPIOpenAPIFactoryBase.h"
#include "UObject/Object.h"
#include "V2/WebAPISwaggerProvider.h"

#include "WebAPISwaggerFactory.generated.h"

UCLASS()
class WEBAPIOPENAPI_API UWebAPISwaggerAssetData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString FileContents;
};

/** Handles importing for Swagger (OpenAPI V2) files. */
UCLASS()
class WEBAPIOPENAPI_API UWebAPISwaggerFactory
	: public UWebAPIOpenAPIFactoryBase
{
	GENERATED_BODY()

public:
	UWebAPISwaggerFactory();
	
	virtual bool CanImportWebAPI(const FString& InFileName, const FString& InFileContents) override;
	virtual TFuture<bool> ImportWebAPI(UWebAPIDefinition* InDefinition, const FString& InFileName, const FString& InFileContents) override;

protected:
	TSharedPtr<FWebAPISwaggerProvider> Provider;	
};
