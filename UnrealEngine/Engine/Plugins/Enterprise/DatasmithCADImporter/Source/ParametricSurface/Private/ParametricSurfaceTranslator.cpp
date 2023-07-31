// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametricSurfaceTranslator.h"

#include "ParametricSurfaceData.h"
#include "ParametricSurfaceModule.h"

#include "DatasmithImportOptions.h"
#include "IDatasmithSceneElements.h"

#include "Misc/FileHelper.h"

void FParametricSurfaceTranslator::GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	FString Extension = GetSource().GetSourceFileExtension();
	if (Extension == TEXT("cgr") || Extension == TEXT("3dxml"))
	{
		return;
	}

	TObjectPtr<UDatasmithCommonTessellationOptions> CommonTessellationOptionsPtr = Datasmith::MakeOptionsObjectPtr<UDatasmithCommonTessellationOptions>();
	check(CommonTessellationOptionsPtr);
	InitCommonTessellationOptions(CommonTessellationOptionsPtr->Options);

	Options.Add(CommonTessellationOptionsPtr);
}

void FParametricSurfaceTranslator::SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	for (const TObjectPtr<UDatasmithOptionsBase>& OptionPtr : Options)
	{
		if (UDatasmithCommonTessellationOptions* TessellationOptionsObject = Cast<UDatasmithCommonTessellationOptions>(OptionPtr))
		{
			CommonTessellationOptions = TessellationOptionsObject->Options;
		}
	}
}

bool ParametricSurfaceUtils::AddSurfaceData(const TCHAR* MeshFilePath, const CADLibrary::FImportParameters& ImportParameters, const CADLibrary::FMeshParameters& InMeshParameters, const FDatasmithTessellationOptions& InCommonTessellationOptions, FDatasmithMeshElementPayload& OutMeshPayload)
{
	if (MeshFilePath && IFileManager::Get().FileExists(MeshFilePath))
	{
		UParametricSurfaceData* ParametricSurfaceData = FParametricSurfaceModule::CreateParametricSurface();

		if (!ParametricSurfaceData || !ParametricSurfaceData->SetFile(MeshFilePath))
		{
			return false;
		}

		ParametricSurfaceData->SetImportParameters(ImportParameters);
		ParametricSurfaceData->SetMeshParameters(InMeshParameters);
		ParametricSurfaceData->SetLastTessellationOptions(InCommonTessellationOptions);

		OutMeshPayload.AdditionalData.Add(ParametricSurfaceData);

		// Remove the file because it is temporary since caching is disabled.
		if (!CADLibrary::FImportParameters::bGEnableCADCache)
		{
			IFileManager::Get().Delete(MeshFilePath);
		}

		return true;
	}

	return false;
}
