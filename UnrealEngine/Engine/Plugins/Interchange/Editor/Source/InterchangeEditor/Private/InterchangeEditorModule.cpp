// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeEditorModule.h"

#include "InterchangeEditorLog.h"

#include "InterchangeManager.h"
#include "InterchangeFbxAssetImportDataConverter.h"

#include "Engine/Engine.h"
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "InterchangeEditorModule"

DEFINE_LOG_CATEGORY(LogInterchangeEditor);

namespace UE::Interchange::InterchangeEditorModule
{
	bool HasErrorsOrWarnings(TStrongObjectPtr<UInterchangeResultsContainer> InResultsContainer)
	{
		for (UInterchangeResult* Result : InResultsContainer->GetResults())
		{
			if (Result->GetResultType() != EInterchangeResultType::Success)
			{
				return true;
			}
		}

		return false;
	}

	void LogErrors(TStrongObjectPtr<UInterchangeResultsContainer> InResultsContainer)
	{
		// Only showing when we have errors or warnings for now
		if (FApp::IsUnattended() || !HasErrorsOrWarnings(InResultsContainer))
		{
			return;
		}

		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		TSharedPtr<IMessageLogListing> LogListing = MessageLogModule.GetLogListing(FName("Interchange"));

		if (ensure(LogListing))
		{
			const FText LogListingLabel = NSLOCTEXT("InterchangeImport", "Label", "Interchange Import");
			LogListing->SetLabel(LogListingLabel);

			TArray<TSharedRef<FTokenizedMessage>> TokenizedMessages;
			bool bHasErrors = false;

			for (UInterchangeResult* Result : InResultsContainer->GetResults())
			{
				const EInterchangeResultType ResultType = Result->GetResultType();
				if (ResultType != EInterchangeResultType::Success)
				{
					TokenizedMessages.Add(FTokenizedMessage::Create(ResultType == EInterchangeResultType::Error ? EMessageSeverity::Error : EMessageSeverity::Warning, Result->GetMessageLogText()));
					bHasErrors |= ResultType == EInterchangeResultType::Error;
				}
			}

			LogListing->AddMessages(TokenizedMessages);
			LogListing->NotifyIfAnyMessages(NSLOCTEXT("Interchange", "LogAndNotify", "There were issues with the import."), EMessageSeverity::Info);
		}
	}
}

FInterchangeEditorModule& FInterchangeEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked< FInterchangeEditorModule >(INTERCHANGEEDITOR_MODULE_NAME);
}

bool FInterchangeEditorModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(INTERCHANGEEDITOR_MODULE_NAME);
}

void FInterchangeEditorModule::StartupModule()
{
	using namespace UE::Interchange;

	auto RegisterItems = [this]()
	{
		FDelegateHandle InterchangeEditorModuleDelegate;

		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
		InterchangeEditorModuleDelegate = InterchangeManager.OnBatchImportComplete.AddStatic(&InterchangeEditorModule::LogErrors);
		InterchangeManager.RegisterImportDataConverter(UInterchangeFbxAssetImportDataConverter::StaticClass());

		auto UnregisterItems = [InterchangeEditorModuleDelegate]()
		{
			UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
			InterchangeManager.OnBatchImportComplete.Remove(InterchangeEditorModuleDelegate);
		};

		InterchangeManager.OnPreDestroyInterchangeManager.AddLambda(UnregisterItems);
	};

	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}
}

IMPLEMENT_MODULE(FInterchangeEditorModule, InterchangeEditor)

#undef LOCTEXT_NAMESPACE // "InterchangeEditorModule"

