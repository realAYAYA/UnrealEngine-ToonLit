// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeOpenFileDialog.h"

#include "Containers/UnrealString.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "InterchangeFilePickerBase.h"
#include "InterchangeManager.h"
#include "InterchangeTranslatorBase.h"
#include "ObjectTools.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeOpenFileDialog)

namespace UE::Interchange::Utilities::Private
{
	FString GetOpenFileDialogExtensions(const TArray<FString>& TranslatorFormats)
	{
		FString FileTypes;
		FString Extensions;

		ObjectTools::AppendFormatsFileExtensions(TranslatorFormats, FileTypes, Extensions);

		const FString FormatString = FString::Printf(TEXT("All Files (%s)|%s|%s"), *Extensions, *Extensions, *FileTypes);

		return FormatString;
	}

	bool FilePickerDialog(const FString& Extensions, FInterchangeFilePickerParameters& Parameters, TArray<FString>& OutFilenames)
	{
		// First, display the file open dialog for selecting the file.
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform)
		{
			FText PromptTitle = Parameters.Title.IsEmpty() ? NSLOCTEXT("InterchangeUtilities_OpenFileDialog", "FilePickerDialog", "Select a file") : Parameters.Title;

			const EFileDialogFlags::Type DialogFlags = Parameters.bAllowMultipleFiles ? EFileDialogFlags::Multiple : EFileDialogFlags::None;

			return DesktopPlatform->OpenFileDialog(
				FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
				PromptTitle.ToString(),
				Parameters.DefaultPath,
				TEXT(""),
				*Extensions,
				DialogFlags,
				OutFilenames
			);
		}

		return false;
	}
} //ns UE::Interchange::Utilities::Private

bool UInterchangeFilePickerGeneric::FilePickerForTranslatorAssetType(const EInterchangeTranslatorAssetType TranslatorAssetType, FInterchangeFilePickerParameters& Parameters, TArray<FString>& OutFilenames)
{
	FString Extensions = UE::Interchange::Utilities::Private::GetOpenFileDialogExtensions(UInterchangeManager::GetInterchangeManager().GetSupportedAssetTypeFormats(TranslatorAssetType));
	return UE::Interchange::Utilities::Private::FilePickerDialog(Extensions, Parameters, OutFilenames);
}

bool UInterchangeFilePickerGeneric::FilePickerForTranslatorType(const EInterchangeTranslatorType TranslatorType, FInterchangeFilePickerParameters& Parameters, TArray<FString>& OutFilenames)
{
	FString Extensions = UE::Interchange::Utilities::Private::GetOpenFileDialogExtensions(UInterchangeManager::GetInterchangeManager().GetSupportedFormats(TranslatorType));
	return UE::Interchange::Utilities::Private::FilePickerDialog(Extensions, Parameters, OutFilenames);
}

