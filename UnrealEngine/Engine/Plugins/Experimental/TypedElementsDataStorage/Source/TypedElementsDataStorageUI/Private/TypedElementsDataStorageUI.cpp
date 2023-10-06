// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementsDataStorageUI.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "FTypedElementsDataStorageModule"

FAutoConsoleCommandWithOutputDevice PrintWidgetPurposesConsoleCommand(
	TEXT("TEDS.UI.PrintWidgetPurposes"),
	TEXT("Prints a list of all the known widget purposes."),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Output)
		{
			UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
			if (ITypedElementDataStorageUiInterface* UiStorage = Registry->GetMutableDataStorageUi())
			{
				Output.Log(TEXT("The Typed Elements Data Storage has recorded the following widget purposes:"));
				UiStorage->ListWidgetPurposes(
					[&Output](FName Purpose, ITypedElementDataStorageUiInterface::EPurposeType, const FText& Description)
					{
						Output.Logf(TEXT("    %s - %s"), *Purpose.ToString(), *Description.ToString());
					});
				Output.Log(TEXT("End of Typed Elements Data Storage widget purpose list."));
			}
		}));

void FTypedElementsDataStorageUiModule::StartupModule()
{
}

void FTypedElementsDataStorageUiModule::ShutdownModule()
{
}

void FTypedElementsDataStorageUiModule::AddReferencedObjects(FReferenceCollector& Collector)
{
}

FString FTypedElementsDataStorageUiModule::GetReferencerName() const
{
	return TEXT("Typed Elements: Data Storage UI Module");
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTypedElementsDataStorageUiModule, TypedElementsDataStorageUI)