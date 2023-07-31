// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFTranslator.h"

#include "DatasmithGLTFTranslatorModule.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithSceneSource.h"
#include "IDatasmithSceneElements.h"

#include "Algo/Count.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"
#include "Logging/TokenizedMessage.h"
#include "Templates/TypeHash.h"
#if WITH_EDITOR
#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#endif //WITH_EDITOR

#include "GLTFAsset.h"
#include "GLTFNode.h"
#include "DatasmithGLTFImporter.h"

void ShowLogMessages(const TArray<GLTF::FLogMessage>& Errors)
{
	if (Errors.Num() > 0)
	{
#if WITH_EDITOR
		if (IsInGameThread())
		{
			FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			TSharedRef<IMessageLogListing> LogListing = MessageLogModule.GetLogListing("LoadErrors");
			LogListing->ClearMessages();
			for (const GLTF::FLogMessage& Error : Errors)
			{
				EMessageSeverity::Type Severity = Error.Get<0>() == GLTF::EMessageSeverity::Error ? EMessageSeverity::Error : EMessageSeverity::Warning;
				LogListing->AddMessage(FTokenizedMessage::Create(Severity, FText::FromString(Error.Get<1>())));
			}
			MessageLogModule.OpenMessageLog("LoadErrors");
		}
		else
		{
			for (const GLTF::FLogMessage& LogError : Errors)
			{
				if (LogError.Get<0>() == GLTF::EMessageSeverity::Error)
				{
					UE_LOG(LogDatasmithGLTFImport, Error, TEXT("%s"), *LogError.Get<1>());
				}
				else
				{
					UE_LOG(LogDatasmithGLTFImport, Warning, TEXT("%s"), *LogError.Get<1>());
				}
			}
		}
#else
		for (const GLTF::FLogMessage& LogError : Errors)
		{
			if (LogError.Get<0>() == GLTF::EMessageSeverity::Error)
			{
				UE_LOG(LogDatasmithGLTFImport, Error, TEXT("%s"), *LogError.Get<1>());
			}
			else
			{
				UE_LOG(LogDatasmithGLTFImport, Warning, TEXT("%s"), *LogError.Get<1>());
			}
		}
#endif //!WITH_EDIOR
	}
}

void FDatasmithGLTFTranslator::Initialize(FDatasmithTranslatorCapabilities& OutCapabilities)
{
	OutCapabilities.bIsEnabled = true;
	OutCapabilities.bParallelLoadStaticMeshSupported = true;

	TArray<FFileFormatInfo>& Formats = OutCapabilities.SupportedFileFormats;
    Formats.Emplace(TEXT("gltf"), TEXT("GL Transmission Format"));
    Formats.Emplace(TEXT("glb"), TEXT("GL Transmission Format (Binary)"));
}

bool FDatasmithGLTFTranslator::LoadScene(TSharedRef<IDatasmithScene> OutScene)
{
	OutScene->SetHost(TEXT("GLTFTranslator"));

    Importer = MakeShared<FDatasmithGLTFImporter>(OutScene, GetOrCreateGLTFImportOptions().Get());

	const FString& FilePath = GetSource().GetSourceFile();
	if(!Importer->OpenFile(FilePath))
	{
		ShowLogMessages(Importer->GetLogMessages());
		return false;
	}

	const GLTF::FAsset& GLTFAsset = Importer->GetAsset();

	// Show statistics on opened scene
	{
		FGLTFImporterStats Stats;

		Stats.MaterialCount = GLTFAsset.Materials.Num();
		Stats.NodeCount     = GLTFAsset.Nodes.Num();
		Stats.MeshCount     = GLTFAsset.Meshes.Num();
		Stats.GeometryCount = Algo::CountIf(GLTFAsset.Nodes, [](const GLTF::FNode& Node)
		{
			return Node.MeshIndex != INDEX_NONE;
		});

		UE_LOG(LogDatasmithGLTFImport, Log, TEXT("Scene %s has %d nodes, %d geometries, %d meshes, %d materials"),
			*FilePath, Stats.NodeCount, Stats.GeometryCount, Stats.MeshCount, Stats.MaterialCount);
	}

	// Add scene metadata
	{
		const GLTF::FMetadata& Metadata = GLTFAsset.Metadata;

		OutScene->SetProductName(*Metadata.GeneratorName);
		OutScene->SetProductVersion(*LexToString(Metadata.Version));

		FString UserID;
		if (const GLTF::FMetadata::FExtraData* Extra = Metadata.GetExtraData(TEXT("author")))
		{
			UserID += Extra->Value;
		}
		if (const GLTF::FMetadata::FExtraData* Extra = Metadata.GetExtraData(TEXT("license")))
		{
			if (!UserID.IsEmpty())
			{
				UserID += "|";
			}

			UserID += Extra->Value;
		}
		OutScene->SetUserID(*UserID);

		if (const GLTF::FMetadata::FExtraData* Extra = Metadata.GetExtraData(TEXT("source")))
		{
			OutScene->SetVendor(*Extra->Value);
		}
	}

	bool bSuccess = Importer->SendSceneToDatasmith();
	return bSuccess;
}

void FDatasmithGLTFTranslator::UnloadScene()
{
	if (Importer)
	{
		ShowLogMessages(Importer->GetLogMessages());
		Importer->UnloadScene();
	}
}

bool FDatasmithGLTFTranslator::LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload)
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

bool FDatasmithGLTFTranslator::LoadLevelSequence(const TSharedRef<IDatasmithLevelSequenceElement> LevelSequenceElement, FDatasmithLevelSequencePayload& OutLevelSequencePayload)
{
	if (ensure(Importer.IsValid()))
	{
		const TArray<TSharedRef<IDatasmithLevelSequenceElement>>& ImportedSequences = Importer->GetImportedSequences();
		if (ImportedSequences.Contains(LevelSequenceElement))
		{
			// #ueent_todo: move data to OutLevelSequencePayload
			// Right now the LevelSequenceElement is imported out of the IDatasmithScene
			return true;
		}
	}

	return false;
}

void FDatasmithGLTFTranslator::GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	Options.Add(GetOrCreateGLTFImportOptions().Get());
}

void FDatasmithGLTFTranslator::SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	for (const TObjectPtr<UDatasmithOptionsBase>& OptionPtr : Options)
	{
		if (UDatasmithGLTFImportOptions* InImportOptions = Cast<UDatasmithGLTFImportOptions>(OptionPtr))
		{
			ImportOptions.Reset(InImportOptions);
		}
	}

	if (Importer.IsValid())
	{
		Importer->SetImportOptions(GetOrCreateGLTFImportOptions().Get());
	}
}

TStrongObjectPtr<UDatasmithGLTFImportOptions>& FDatasmithGLTFTranslator::GetOrCreateGLTFImportOptions()
{
	if (!ImportOptions.IsValid() && IsInGameThread())
	{
		ImportOptions = Datasmith::MakeOptions<UDatasmithGLTFImportOptions>();
	}
	return ImportOptions;
}
