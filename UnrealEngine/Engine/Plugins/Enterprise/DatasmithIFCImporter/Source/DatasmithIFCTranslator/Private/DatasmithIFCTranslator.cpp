// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithIFCTranslator.h"

#include "DatasmithIFCTranslatorModule.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithSceneSource.h"
#include "IDatasmithSceneElements.h"

#include "Algo/Count.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Logging/TokenizedMessage.h"
#include "Templates/TypeHash.h"

#if WITH_EDITOR
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#endif //WITH_EDITOR

#include "DatasmithIFCImporter.h"

void ShowLogMessages(const TArray<IFC::FLogMessage>& Errors)
{
	if (Errors.Num() > 0)
	{
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		TSharedRef<IMessageLogListing> LogListing = (MessageLogModule.GetLogListing("LoadErrors"));
		LogListing->ClearMessages();

		for (const IFC::FLogMessage& Error : Errors)
		{
			EMessageSeverity::Type Severity = Error.Get<0>();
			LogListing->AddMessage(FTokenizedMessage::Create(Severity, FText::FromString(Error.Get<1>())));
		}

		MessageLogModule.OpenMessageLog("LoadErrors");
#else
		for (const IFC::FLogMessage& LogError : Errors)
		{
			switch (LogError.Get<0>())
			{
			case EMessageSeverity::Error:
				UE_LOG(LogDatasmithIFCImport, Error, TEXT("%s"), *LogError.Get<1>());
				break;
			case EMessageSeverity::Info:
				UE_LOG(LogDatasmithIFCImport, Log, TEXT("%s"), *LogError.Get<1>());
				break;
			case EMessageSeverity::Warning:
			default:
				UE_LOG(LogDatasmithIFCImport, Warning, TEXT("%s"), *LogError.Get<1>());
				break;
			}
		}
#endif //!WITH_EDIOR
	}
}

void FDatasmithIFCTranslator::Initialize(FDatasmithTranslatorCapabilities& OutCapabilities)
{
	OutCapabilities.bParallelLoadStaticMeshSupported = true;

	IConsoleVariable* CVarIfcTranslatorEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("ds.IFC.EnableNativeTranslator"));
	if (CVarIfcTranslatorEnabled)
	{
		OutCapabilities.bIsEnabled = CVarIfcTranslatorEnabled->GetBool();
		if (!OutCapabilities.bIsEnabled)
		{
			return;
		}
	}
	else
	{
		OutCapabilities.bIsEnabled = true;
	}

	TArray<FFileFormatInfo>& Formats = OutCapabilities.SupportedFileFormats;
	Formats.Emplace(TEXT("ifc"), TEXT("IFC (Industry Foundation Classes)"));
}

bool FDatasmithIFCTranslator::LoadScene(TSharedRef<IDatasmithScene> OutScene)
{
	OutScene->SetHost(TEXT("IFCTranslator"));
	OutScene->SetProductName(TEXT("IFC"));

    Importer = MakeShared<FDatasmithIFCImporter>(OutScene, ImportOptions.Get());

	const FString& FilePath = GetSource().GetSourceFile();
	if(!Importer->OpenFile(FilePath))
	{
		ShowLogMessages(Importer->GetLogMessages());
		return false;
	}

	bool bSuccess = Importer->SendSceneToDatasmith();
	ShowLogMessages(Importer->GetLogMessages());
	return bSuccess;
}

void FDatasmithIFCTranslator::UnloadScene()
{
	if (Importer)
	{
		Importer->UnloadScene();
	}
}

bool FDatasmithIFCTranslator::LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload)
{
	if (ensure(Importer.IsValid()))
	{
		TArray<FMeshDescription> MeshDescriptions;
		Importer->GetGeometriesForMeshElementAndRelease(MeshElement, MeshDescriptions);
		if (MeshDescriptions.Num() > 0)
		{
			OutMeshPayload.LodMeshes.Add(MoveTemp(MeshDescriptions[0]));
			return true;
		}
	}

	return false;
}

bool FDatasmithIFCTranslator::LoadLevelSequence(const TSharedRef<IDatasmithLevelSequenceElement> LevelSequenceElement, FDatasmithLevelSequencePayload& OutLevelSequencePayload)
{
	return false;
}

void FDatasmithIFCTranslator::GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	if (!ImportOptions.IsValid())
	{
		ImportOptions = Datasmith::MakeOptions<UDatasmithIFCImportOptions>();
	}

	Options.Add(ImportOptions.Get());
}

void FDatasmithIFCTranslator::SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	for (const TObjectPtr<UDatasmithOptionsBase>& OptionPtr : Options)
	{
		if (UDatasmithIFCImportOptions* InImportOptions = Cast<UDatasmithIFCImportOptions>(OptionPtr))
		{
			ImportOptions.Reset(InImportOptions);
		}
	}

	if (Importer.IsValid())
	{
		Importer->SetImportOptions(ImportOptions.Get());
	}
}
