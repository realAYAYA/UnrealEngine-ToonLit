// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAPIOpenAPIFactory.h"

#include "WebAPIOpenAPILog.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "WebAPIOpenAPIFactory"

UWebAPIOpenAPIFactory::UWebAPIOpenAPIFactory()
	: Provider(MakeShared<FWebAPIOpenAPIProvider>())
{
}

bool UWebAPIOpenAPIFactory::CanImportWebAPI(const FString& InFileName, const FString& InFileContents)
{
	TSharedPtr<FJsonObject> JsonObject;
	if(!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(InFileContents), JsonObject))
	{
		return false;
	}

	return JsonObject->HasField(TEXT("openapi"));
}

TFuture<bool> UWebAPIOpenAPIFactory::ImportWebAPI(UWebAPIDefinition* InDefinition, const FString& InFileName, const FString& InFileContents)
{
	TSharedPtr<FJsonObject> JsonObject;
	if(!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(InFileContents), JsonObject))
	{
		UE_LOG(LogWebAPIOpenAPI, Error, TEXT("Couldn't deserialize file contents as Json"));
		return MakeFulfilledPromise<bool>(false).GetFuture();
	}

	// Write raw spec contents
	UWebAPIOpenAPIAssetData* ImportData = InDefinition->AddOrGetImportedDataCache<UWebAPIOpenAPIAssetData>("OpenAPI");
	ImportData->FileContents = InFileContents;

	// Parse spec
	return Provider->ConvertToWebAPISchema(InDefinition)
	.Next([](EWebAPIConversionResult bInConversionResult)
	{
		return bInConversionResult == EWebAPIConversionResult::Succeeded;
	});
}

#undef LOCTEXT_NAMESPACE
