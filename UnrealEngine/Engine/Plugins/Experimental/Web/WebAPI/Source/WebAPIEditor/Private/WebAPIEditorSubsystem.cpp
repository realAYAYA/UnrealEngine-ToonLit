// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAPIEditorSubsystem.h"

#include "IWebAPIEditorModule.h"
#include "WebAPIEditorLog.h"

UWebAPIEditorSubsystem::UWebAPIEditorSubsystem()
{
}

void UWebAPIEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	OnDefinitionPreImport().AddUObject(this, &UWebAPIEditorSubsystem::HandleDefinitionPreImport);
	OnDefinitionPostImport().AddUObject(this, &UWebAPIEditorSubsystem::HandleDefinitionPostImport);
}

void UWebAPIEditorSubsystem::Deinitialize()
{
	OnDefinitionPreImport().RemoveAll(this);
	OnDefinitionPostImport().RemoveAll(this);
	
	Super::Deinitialize();
}

UWebAPIEditorSubsystem::FOnWebAPIDefinitionPreImport& UWebAPIEditorSubsystem::OnDefinitionPreImport()
{ 
	return OnDefinitionPreImportDelegate;
}

UWebAPIEditorSubsystem::FOnWebAPIDefinitionPostImport& UWebAPIEditorSubsystem::OnDefinitionPostImport()
{
	return OnDefinitionPostImportDelegate;
}

UWebAPIEditorSubsystem::FOnModuleCreated& UWebAPIEditorSubsystem::OnModuleCreated()
{
	return OnModuleCreatedDelegate;
}

void UWebAPIEditorSubsystem::HandleDefinitionPreImport(UWebAPIDefinition* InDefinition)
{
}

void UWebAPIEditorSubsystem::HandleDefinitionPostImport(bool bInImportSuccessful, FName InFactoryName, UWebAPIDefinition* InDefinition)
{
	if(!bInImportSuccessful)
	{
		return;
	}

	checkf(InFactoryName != NAME_None, TEXT("The importing Factory must provide a name."));
	
	if(const TSharedPtr<IWebAPIProviderInterface> NamedProvider = IWebAPIEditorModuleInterface::Get().GetProvider(InFactoryName))
	{
		NamedProvider->ConvertToWebAPISchema(InDefinition)
		.Next([](EWebAPIConversionResult bInConversionResult)
		{
			
		});
		return;
	}

	UE_LOG(LogWebAPIEditor, Error, TEXT("UWebAPIEditorSubsystem::HandleDefinitionPostImport(): Provider \"%s\" wasn't found!"), *InFactoryName.ToString());
}
