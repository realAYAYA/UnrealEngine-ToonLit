// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorUtils.h"
#include "LandscapeSettings.h"

#include "LandscapeEditorModule.h"
#include "LandscapeEdit.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorObject.h"
#include "LandscapeTiledImage.h"
#include "LandscapeStreamingProxy.h"

#include "DesktopPlatformModule.h"
#include "EditorModeManager.h"
#include "EditorModes.h"

#include "Algo/Transform.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"

namespace LandscapeEditorUtils
{
	int32 GetMaxSizeInComponents()
	{
		const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
		return Settings->MaxComponents;
	}


	TOptional<FString> GetImportExportFilename(const FString& InDialogTitle, const FString& InStartPath, const FString& InDialogTypeString, bool bInImporting)
	{
		FString Filename;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		if (DesktopPlatform == nullptr)
		{
			return TOptional<FString>();
		}

		TArray<FString> Filenames;
		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		FEdModeLandscape* LandscapeEdMode = static_cast<FEdModeLandscape*>(GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape));

		bool bSuccess;
		
		if (bInImporting)
		{
			bSuccess = DesktopPlatform->OpenFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				InDialogTitle,
				InStartPath,
				TEXT(""),
				InDialogTypeString,
				EFileDialogFlags::None,
				Filenames);
		}
		else
		{
			bSuccess = DesktopPlatform->SaveFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				InDialogTitle,
				InStartPath,
				TEXT(""),
				InDialogTypeString,
				EFileDialogFlags::None,
				Filenames);
		}

		if (bSuccess)
		{
			FString TiledFileNamePattern;

			if (bInImporting && FLandscapeTiledImage::CheckTiledNamePath(Filenames[0], TiledFileNamePattern) && FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(FString::Format(TEXT("Use '{0}' Tiled Image?"), { TiledFileNamePattern }))) == EAppReturnType::Yes)
			{
				Filename = TiledFileNamePattern;
			}
			else
			{
				Filename = Filenames[0];
			}

		}

		return TOptional<FString>(Filename);

	}

	void SaveLandscapeProxies(TArrayView<ALandscapeProxy*> Proxies)
	{
		// Save the proxies
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SaveCreatedActors);
			LandscapeEditorUtils::SaveObjects(Proxies);
		}

		// Grab references to proxies so they get unloaded after this function returns
		TArray<FWorldPartitionReference> ProxyReferences;
		Algo::Transform(Proxies, ProxyReferences, [](ALandscapeProxy* Proxy) { return FWorldPartitionReference(FWorldPartitionHelpers::GetWorldPartition(Proxy), Proxy->GetActorGuid()); });
	}
}


