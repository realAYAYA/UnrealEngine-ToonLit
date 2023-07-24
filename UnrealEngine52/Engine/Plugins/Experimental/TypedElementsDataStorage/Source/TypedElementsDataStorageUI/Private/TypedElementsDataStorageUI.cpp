// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementsDataStorageUI.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/CoreDelegates.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "TypedElementCounterWidgetConstructor.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "FTypedElementsDataStorageModule"

void FTypedElementsDataStorageUiModule::StartupModule()
{	
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda(
		[this]()
		{
			UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
			checkf(Registry, TEXT(
				"FTypedElementsDataStorageUiModule tried to retrieve the UTypedElementRegistry "
				"before it was available."));
			if (Registry->AreDataStorageInterfacesSet())
			{
				LateInitialization();
			}
			else
			{
				Registry->OnDataStorageInterfacesSet().AddStatic(&FTypedElementsDataStorageUiModule::LateInitialization);
			}
		});
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

void FTypedElementsDataStorageUiModule::LateInitialization()
{
	RegisterWidgetFactories();
	IMainFrameModule::Get().OnMainFrameCreationFinished().AddStatic(&FTypedElementsDataStorageUiModule::SetupMainWindowIntegrations);
}

void FTypedElementsDataStorageUiModule::RegisterWidgetFactories()
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT(
		"FTypedElementsDataStorageUiModule didn't find the UTypedElementRegistry during widget registration when it should be available."));

	ITypedElementDataStorageInterface* DataInterface = Registry->GetMutableDataStorage();
	checkf(DataInterface, TEXT(
		"FTypedElementsDataStorageUiModule tried to register widgets before the "
		"Typed Elements Data Storage interface is available."));
	ITypedElementDataStorageUiInterface* UiInterface = Registry->GetMutableDataStorageUi();
	checkf(UiInterface, TEXT(
		"FTypedElementsDataStorageUiModule tried to register widgets before the "
		"Typed Elements Data Storage UI interface is available."));

	FTypedElementCounterWidgetConstructor::Register(*DataInterface, *UiInterface);
}

void FTypedElementsDataStorageUiModule::SetupMainWindowIntegrations(TSharedPtr<SWindow> ParentWindow, bool bIsRunningStartupDialog)
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
	checkf(Registry, TEXT(
		"FTypedElementsDataStorageUiModule didn't find the UTypedElementRegistry during main window integration when it should be available."));

	ITypedElementDataStorageUiInterface* UiInterface = Registry->GetMutableDataStorageUi();
	checkf(UiInterface, TEXT(
		"FTypedElementsDataStorageUiModule tried to integrate with the main window before the "
		"Typed Elements Data Storage UI interface is available."));
	
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.StatusBar.ToolBar");

	TArray<TSharedRef<SWidget>> Widgets;
	UiInterface->ConstructWidgets("LevelEditor.StatusBar.ToolBar", {},
		[&Widgets](const TSharedRef<SWidget>& NewWidget, TypedElementRowHandle Row)
		{
			Widgets.Add(NewWidget);
		});

	if (!Widgets.IsEmpty())
	{
		FToolMenuSection& Section = Menu->AddSection("DataStorageSection");
		int32 WidgetCount = Widgets.Num();

		Section.AddEntry(FToolMenuEntry::InitWidget("DataStorageStatusBarWidget_0", MoveTemp(Widgets[0]), FText::GetEmpty()));
		for (int32 I = 1; I < WidgetCount; ++I)
		{
			Section.AddSeparator(FName(*FString::Format(TEXT("DataStorageStatusBarWidgetDivider_{0}"), { FString::FromInt(I) })));
			Section.AddEntry(FToolMenuEntry::InitWidget(
				FName(*FString::Format(TEXT("DataStorageStatusBarWidget_{0}"), { FString::FromInt(I) })),
				MoveTemp(Widgets[I]), FText::GetEmpty()));
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTypedElementsDataStorageUiModule, TypedElementsDataStorageUI)