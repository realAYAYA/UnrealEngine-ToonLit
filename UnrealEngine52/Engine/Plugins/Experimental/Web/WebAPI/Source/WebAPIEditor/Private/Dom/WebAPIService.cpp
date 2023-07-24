// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dom/WebAPIService.h"

#include "Dom/WebAPIOperation.h"
#include "Dom/WebAPIType.h"

#define LOCTEXT_NAMESPACE "WebAPIService"

void UWebAPIService::SetNamespace(const FString& InNamespace)
{
	Super::SetNamespace(InNamespace);
	check(Name.HasTypeInfo());

	if(Name.HasTypeInfo() && !Name.TypeInfo->bIsBuiltinType)
	{
		Name.TypeInfo->Namespace = InNamespace;
	}
}

void UWebAPIService::Visit(const TFunctionRef<void(IWebAPISchemaObjectInterface*&)> InVisitor)
{
	Super::Visit(InVisitor);

	for(const TObjectPtr<UWebAPIOperation>& Operation : Operations)
	{
		Operation->Visit(InVisitor);
	}
}

#if WITH_EDITOR
EDataValidationResult UWebAPIService::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult ValidationResult = Super::IsDataValid(ValidationErrors);

	for(const TObjectPtr<UWebAPIOperation>& Operation : Operations)
	{
		ValidationResult = CombineDataValidationResults(Operation->IsDataValid(ValidationErrors), ValidationResult);
	}

	return ValidationResult;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
