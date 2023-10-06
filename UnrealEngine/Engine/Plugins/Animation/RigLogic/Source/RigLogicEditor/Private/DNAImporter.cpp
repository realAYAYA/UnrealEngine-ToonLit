// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Main implementation of FDNAImporter : import DNA data to Skeletal Mesh
=============================================================================*/

#include "DNAImporter.h"
#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IMainFrameModule.h"
#include "AssetRegistry/ARFilter.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DNAAssetImportWindow.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"

TSharedPtr<FDNAImporter> FDNAImporter::StaticInstance;
TSharedPtr<FDNAImporter> FDNAImporter::StaticPreviewInstance;

#define LOCTEXT_NAMESPACE "DNAAssetImport"

FDNAAssetImportOptions* GetImportOptions(FDNAImporter * DNAImporter, UDNAAssetImportUI * ImportUI, bool bShowOptionDialog, bool bIsAutomated, const FString & FullPath, bool & OutOperationCanceled, const FString & InFilename)
{
	OutOperationCanceled = false;

	if (bShowOptionDialog)
	{
		FDNAAssetImportOptions* ImportOptions = DNAImporter->GetImportOptions();

		// if SkeletalMesh was set by outside, please make sure copy back to UI
		if (ImportOptions->SkeletalMesh)
		{
			ImportUI->SkeletalMesh = ImportOptions->SkeletalMesh;
		}
		else
		{
			// Look in the current target directory to see if we have a skeleton
			FARFilter Filter;
			Filter.PackagePaths.Add(*FPaths::GetPath(FullPath));
			Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());

			IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			TArray<FAssetData> SkeletalMeshAssets;
			AssetRegistry.GetAssets(Filter, SkeletalMeshAssets);
			if (SkeletalMeshAssets.Num() > 0)
			{
				ImportUI->SkeletalMesh = CastChecked<USkeletalMesh>(SkeletalMeshAssets[0].GetAsset());
			}
			else
			{
				ImportUI->SkeletalMesh = NULL;
			}
		}

		DNAImporter->SetDNAFileName(*FPaths::GetCleanFilename(FullPath));

		//This option must always be the same value has the skeletalmesh one.
		

		//////////////////////////////////////////////////////////////////////////
		// Set the information section data

		//Make sure the file is open to be able to read the header before showing the options
		//If the file is already open it will simply return false.
		// do analytics on getting DNA data
		ImportUI->FileName = DNAImporter->GetDNAFileName();

		TSharedPtr<SWindow> ParentWindow;

		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		// Compute centered window position based on max window size, which include when all categories are expanded
		const float DNAImportWindowWidth = 410.0f;
		const float DNAImportWindowHeight = 750.0f;
		FVector2D DNAImportWindowSize = FVector2D(DNAImportWindowWidth, DNAImportWindowHeight); // Max window size it can get based on current slate


		FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
		FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
		FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

		float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
		DNAImportWindowSize *= ScaleFactor;

		FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - DNAImportWindowSize) / 2.0f) / ScaleFactor;


		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(NSLOCTEXT("UnrealEd", "DNAImportOpionsTitle", "DNA Import Options"))
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::None)
			.ClientSize(DNAImportWindowSize)
			.ScreenPosition(WindowPosition);

		TSharedPtr<SDNAAssetImportWindow> DNAImportWindow;
		Window->SetContent
		(
			SAssignNew(DNAImportWindow, SDNAAssetImportWindow)
			.ImportUI(ImportUI)
			.WidgetWindow(Window)
			.FullPath(FText::FromString(FullPath))
			.MaxWindowHeight(DNAImportWindowHeight)
			.MaxWindowWidth(DNAImportWindowWidth)
		);

		// @todo: we can make this slow as showing progress bar later
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		ImportUI->SaveConfig();

		if (DNAImportWindow->ShouldImport())
		{
			// open dialog
			// see if it's canceled
			ApplyImportUIToImportOptions(ImportUI, *ImportOptions);

			return ImportOptions;
		}
		else
		{
			OutOperationCanceled = true;
		}
	}
	else if (bIsAutomated)
	{
		//Automation tests set ImportUI settings directly.  Just copy them over
		FDNAAssetImportOptions* ImportOptions = DNAImporter->GetImportOptions();
		//Clean up the options
		FDNAAssetImportOptions::ResetOptions(ImportOptions);
		ApplyImportUIToImportOptions(ImportUI, *ImportOptions);
		return ImportOptions;
	}
	else
	{
		return DNAImporter->GetImportOptions();
	}

	return NULL;
}

void ApplyImportUIToImportOptions(UDNAAssetImportUI* ImportUI, FDNAAssetImportOptions& InOutImportOptions)
{
	check(ImportUI);
	InOutImportOptions.SkeletalMesh = ImportUI->SkeletalMesh;
}

FDNAImporter::~FDNAImporter()
{
	CleanUp();
}

FDNAImporter* FDNAImporter::GetInstance()
{
	if (!StaticInstance.IsValid())
	{
		StaticInstance = MakeShareable(new FDNAImporter());
	}
	return StaticInstance.Get();
}

void FDNAImporter::DeleteInstance()
{
	StaticInstance.Reset();
}

FDNAAssetImportOptions* FDNAImporter::GetImportOptions() const
{
	return ImportOptions;
}

FDNAImporter::FDNAImporter()
	: ImportOptions(NULL)
{
	ImportOptions = new FDNAAssetImportOptions();
	FMemory::Memzero(*ImportOptions);

	CurPhase = NOTSTARTED;
}

void FDNAImporter::CleanUp()
{
	PartialCleanUp();

	delete ImportOptions;
	ImportOptions = NULL;
}

void FDNAImporter::PartialCleanUp()
{
	// reset
	CurPhase = NOTSTARTED;
}

FString FDNAImporter::PromptForDNAImportFile()
{
	const FText PromptTitle = LOCTEXT("DNAPromptTitle", "Choose a file to import for DNA");

	FString ChosenFilename("");

	FString ExtensionStr;
	ExtensionStr += TEXT("All model files|*.dna|");
	ExtensionStr += TEXT("DNA files|*.dna|");
	ExtensionStr += TEXT("All files|*.*");

	// First, display the file open dialog for selecting the file.
	TArray<FString> OpenFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpen = false;
	if (DesktopPlatform)
	{
		bOpen = DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			PromptTitle.ToString(),
			*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR),
			TEXT(""),
			*ExtensionStr,
			EFileDialogFlags::None,
			OpenFilenames
		);
	}

	if (bOpen)
	{
		if (OpenFilenames.Num() == 1)
		{
			ChosenFilename = OpenFilenames[0];
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::UNR, FPaths::GetPath(ChosenFilename));
		}
	}

	return ChosenFilename;
}

#undef LOCTEXT_NAMESPACE
