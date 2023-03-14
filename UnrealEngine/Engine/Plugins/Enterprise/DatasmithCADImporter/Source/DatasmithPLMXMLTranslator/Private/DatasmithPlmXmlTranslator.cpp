// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithPlmXmlTranslator.h"
#include "DatasmithPlmXmlImporter.h"
#include "DatasmithPlmXmlTranslatorModule.h"

#include "CADInterfacesModule.h"

#include "DatasmithSceneFactory.h"
#include "DatasmithSceneSource.h"
#include "DatasmithImportOptions.h"
#include "IDatasmithSceneElements.h"

#include "CoreGlobals.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDatasmithXMLPLMTranslator, Log, All)

void FDatasmithPlmXmlTranslator::Initialize(FDatasmithTranslatorCapabilities& OutCapabilities)
{
#if WITH_EDITOR
	if (GIsEditor && !GEditor->PlayWorld && !GIsPlayInEditorWorld)
	{
		TFunction<bool()> GetCADInterfaceAvailability = []() -> bool
		{
			if (ICADInterfacesModule::GetAvailability() == ECADInterfaceAvailability::Unavailable)
			{
				UE_LOG(LogDatasmithXMLPLMTranslator, Warning, TEXT("CAD Interface module is unavailable. Most of CAD formats (except to Rhino and Alias formats) cannot be imported."));
				return false;
			}
			return true;
		};
		static bool bIsCADInterfaceAvailable = GetCADInterfaceAvailability();

		OutCapabilities.bIsEnabled = true;
		OutCapabilities.bParallelLoadStaticMeshSupported = true;

		TArray<FFileFormatInfo>& Formats = OutCapabilities.SupportedFileFormats;
		Formats.Emplace(TEXT("plmxml"), TEXT("PLMXML"));
		Formats.Emplace(TEXT("xml"), TEXT("PLMXML"));

		return;
	}
#endif

	OutCapabilities.bIsEnabled = false;
}

bool FDatasmithPlmXmlTranslator::IsSourceSupported(const FDatasmithSceneSource& Source)
{
	if (Source.GetSourceFileExtension() != TEXT("xml"))
	{
		return true;
	}

	return Datasmith::CheckXMLFileSchema(Source.GetSourceFile(), TEXT("PLMXML"));
}

bool FDatasmithPlmXmlTranslator::LoadScene(TSharedRef<IDatasmithScene> OutScene)
{
	OutScene->SetHost(TEXT("PlmXmlTranslator"));
	OutScene->SetProductName(TEXT("PlmXml"));

    Importer = MakeUnique<FDatasmithPlmXmlImporter>(OutScene);

	const FString& FilePath = GetSource().GetSourceFile();
	if (!Importer->OpenFile(FilePath, GetSource(), CommonTessellationOptions))
	{
		return false;
	}

	return true;
}

void FDatasmithPlmXmlTranslator::UnloadScene()
{
	if (Importer)
	{
		Importer->UnloadScene();
		Importer.Reset();
	}
}

bool FDatasmithPlmXmlTranslator::LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload)
{
	if (ensure(Importer.IsValid()))
	{
		return Importer->LoadStaticMesh(MeshElement, OutMeshPayload);
	}

	return false;
}

void FDatasmithPlmXmlTranslator::GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	TObjectPtr<UDatasmithCommonTessellationOptions> CommonTessellationOptionsPtr = Datasmith::MakeOptionsObjectPtr<UDatasmithCommonTessellationOptions>();

	CommonTessellationOptionsPtr->Options = CommonTessellationOptions;

	Options.Add(CommonTessellationOptionsPtr);
}

void FDatasmithPlmXmlTranslator::SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	for (const TObjectPtr<UDatasmithOptionsBase>& OptionPtr : Options)
	{
		if (UDatasmithCommonTessellationOptions* TessellationOptionsObject = Cast<UDatasmithCommonTessellationOptions>(OptionPtr))
		{
			CommonTessellationOptions = TessellationOptionsObject->Options;
		}
	}
}
