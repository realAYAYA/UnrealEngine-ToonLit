// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXLibraryFromMVRFactory.h"

#include "DMXEditorLog.h"
#include "DMXEditorModule.h"
#include "DMXEditorSettings.h"
#include "DMXEditorUtils.h"
#include "DMXInitializeFixtureTypeFromGDTFHelper.h"
#include "DMXZipper.h"
#include "Factories/DMXGDTFFactory.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXGDTFAssetImportData.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"
#include "MVR/DMXMVRAssetImportData.h"
#include "MVR/DMXMVRGeneralSceneDescription.h"
#include "MVR/Types/DMXMVRFixtureNode.h"

#include "Algo/Find.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "XmlFile.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"


#define LOCTEXT_NAMESPACE "DMXLibraryFromMVRFactory"

namespace UE::DMX::DMXLibraryFromMVRFactory::Private
{
	/** Finds the file corresponding to the GDTF Spec in an MVR Zip file. Returns true if the file was found.  */
	bool FindGDTFFilenameInMVRZip(const TSharedRef<FDMXZipper>& MVRZip, const FString& GDTFSpec, FString& OutGDTFFilename)
	{
		OutGDTFFilename.Reset();

		// Some softwares store the GDTF spec with, some without the .gdtf extension. 
		TArray<FString> Filenames = MVRZip->GetFiles();
		const FString* FilenamePtr = Algo::FindByPredicate(Filenames, [GDTFSpec](const FString& Filename)
			{
				if (GDTFSpec == Filename)
				{
					return true;
				}
				else if (GDTFSpec + TEXT(".gdtf") == Filename)
				{
					return true;
				}
				return false;
			});

		if (FilenamePtr)
		{
			OutGDTFFilename = *FilenamePtr;
			return true;
		}
		return false;
	}
}

const FString UDMXLibraryFromMVRFactory::MVRFileExtension = TEXT("MVR");

UDMXLibraryFromMVRFactory::UDMXLibraryFromMVRFactory()
{
	bEditorImport = true;
	bEditAfterNew = true;
	SupportedClass = UDMXLibrary::StaticClass();

	Formats.Add(TEXT("mvr;My Virtual Rig"));
}

UObject* UDMXLibraryFromMVRFactory::FactoryCreateFile(UClass* InClass, UObject* Parent, FName InName, EObjectFlags Flags, const FString& InFilename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;
	CurrentFilename = InFilename;

	if (!FPaths::FileExists(InFilename))
	{
		UE_LOG(LogDMXEditor, Error, TEXT("Failed to create DMX Library for MVR '%s'. Cannot find file."), *InFilename);
		return nullptr;
	}
	UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
	check(DMXEditorSettings);
	DMXEditorSettings->LastMVRImportPath = FPaths::GetPath(InFilename);

	UDMXLibrary* NewDMXLibrary = CreateDMXLibraryAsset(Parent, InName, Flags, InFilename);
	if (!NewDMXLibrary)
	{
		UE_LOG(LogDMXEditor, Error, TEXT("Failed to create DMX Library for MVR '%s'."), *InFilename);
		return nullptr;
	}

	const TSharedRef<FDMXZipper> Zip = MakeShared<FDMXZipper>();
	if (!Zip->LoadFromFile(InFilename))
	{
		UE_LOG(LogDMXEditor, Error, TEXT("Cannot read MVR '%s'. File is not a valid MVR."), *InFilename);
		return nullptr;
	}

	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = CreateGeneralSsceneDescription(*NewDMXLibrary, Zip);
	if (!GeneralSceneDescription)
	{
		UE_LOG(LogDMXEditor, Error, TEXT("Cannot read General Scene Description from MVR '%s'. File is not a valid MVR."), *InFilename);
		return nullptr;
	}

	UDMXMVRAssetImportData* MVRAssetImportData = GeneralSceneDescription->GetMVRAssetImportData();
	MVRAssetImportData->SetSourceFile(InFilename);

	TArray<UDMXImportGDTF*> GDTFs = CreateGDTFAssets(Parent, Flags, Zip, *GeneralSceneDescription);
	InitDMXLibrary(NewDMXLibrary, GDTFs, GeneralSceneDescription);

	return NewDMXLibrary;
}

bool UDMXLibraryFromMVRFactory::FactoryCanImport(const FString& Filename)
{
	const FString TargetExtension = FPaths::GetExtension(Filename);
	if (TargetExtension.Equals(UDMXLibraryFromMVRFactory::MVRFileExtension, ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}

bool UDMXLibraryFromMVRFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UDMXMVRAssetImportData* MVRAssetImportData = GetMVRAssetImportData(Obj);
	if (!MVRAssetImportData)
	{
		return false;
	}
	
	const FString SourceFilename = MVRAssetImportData->GetFilePathAndName();
	OutFilenames.Add(SourceFilename);
	return true;
}

void UDMXLibraryFromMVRFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UDMXMVRAssetImportData* MVRAssetImportData = GetMVRAssetImportData(Obj);
	if (!ensureMsgf(MVRAssetImportData, TEXT("Invalid MVR Asset Import Data for General Scene Description %s.")))
	{
		return;
	}

	if (!ensureMsgf(NewReimportPaths.Num() == 1, TEXT("Unexpected, more than one or no new reimport path when reimporting DMX Library from MVR.")))
	{
		return;
	}

	MVRAssetImportData->SetSourceFile(NewReimportPaths[0]);
}

EReimportResult::Type UDMXLibraryFromMVRFactory::Reimport(UObject* Obj)
{
	UDMXMVRAssetImportData* MVRAssetImportData = GetMVRAssetImportData(Obj);
	if (!ensureMsgf(MVRAssetImportData, TEXT("Invalid MVR Asset Import Data for General Scene Description.")))
	{
		return EReimportResult::Failed;
	}
	const FString SourceFilename = MVRAssetImportData->GetFilePathAndName();

	UDMXLibrary* DMXLibrary = Cast<UDMXLibrary>(Obj);
	if (!ensureMsgf(DMXLibrary, TEXT("Invalid DMX Library when trying to reimport DMX Library.")))
	{
		return EReimportResult::Failed;
	}

	if (!DMXLibrary->GetEntities().IsEmpty())
	{
		const FText MessageText = LOCTEXT("MVRReimportDialog", "DMX Library already contains data. Reimporting MVR will clear existing data. Do you want to proceed?");
		if (FMessageDialog::Open(EAppMsgType::YesNo, MessageText) == EAppReturnType::No)
		{
			return EReimportResult::Cancelled;
		}
	}

	const TSharedRef<FDMXZipper> Zip = MakeShared<FDMXZipper>();
	if (!Zip->LoadFromFile(SourceFilename))
	{
		UE_LOG(LogDMXEditor, Error, TEXT("Cannot read MVR '%s'. File is not a valid MVR."), *SourceFilename);
		return EReimportResult::Failed;
	}

	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = CreateGeneralSsceneDescription(*DMXLibrary, Zip);
	if (!GeneralSceneDescription)
	{
		UE_LOG(LogDMXEditor, Error, TEXT("Cannot read General Scene Description from MVR '%s'. File is not a valid MVR."), *SourceFilename);
		return EReimportResult::Failed;
	}

	// Since a new General Scene Description was created, the source file has to be set anew, 
	MVRAssetImportData->SetSourceFile(SourceFilename);

	TArray<UDMXImportGDTF*> GDTFs = CreateGDTFAssets(DMXLibrary->GetOuter(), RF_NoFlags, Zip, *GeneralSceneDescription);
	InitDMXLibrary(DMXLibrary, GDTFs, GeneralSceneDescription);

	return EReimportResult::Succeeded;
}

int32 UDMXLibraryFromMVRFactory::GetPriority() const
{
	return ImportPriority;
}

UDMXLibrary* UDMXLibraryFromMVRFactory::CreateDMXLibraryAsset(UObject* Parent, const FName& Name, EObjectFlags Flags, const FString& InFilename)
{
	UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>();
	ImportSubsystem->BroadcastAssetPreImport(this, UDMXLibrary::StaticClass(), Parent, Name, *MVRFileExtension);

	UDMXLibrary* NewDMXLibrary = NewObject<UDMXLibrary>(Parent, Name, Flags | RF_Public);
	if (!NewDMXLibrary)
	{
		return nullptr;
	}

	ImportSubsystem->BroadcastAssetPostImport(this, NewDMXLibrary);
	
	return NewDMXLibrary;
}

UDMXMVRGeneralSceneDescription* UDMXLibraryFromMVRFactory::CreateGeneralSsceneDescription(UDMXLibrary& DMXLibrary, const TSharedRef<FDMXZipper>& Zip) const
{
	TArray64<uint8> XMLData;
	if (!Zip->GetFileContent(TEXT("GeneralSceneDescription.xml"), XMLData))
	{
		return nullptr;
	}

	// MVR implicitly adpots UTF-8 encoding of Xml Files by adopting the GDTF standard (DIN-15800).
	// Content is NOT null-terminated; we need to specify lengths here.
	const FUTF8ToTCHAR TCHARData(reinterpret_cast<const ANSICHAR*>(XMLData.GetData()), XMLData.Num());

	const TSharedRef<FXmlFile> GeneralSceneDescriptionXml = MakeShared<FXmlFile>(FString(TCHARData.Length(), TCHARData.Get()), EConstructMethod::ConstructFromBuffer);

	const FName GeneralSceneDescriptionName = FName(GetName() + TEXT("_MVRGeneralSceneDescription"));
	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = UDMXMVRGeneralSceneDescription::CreateFromXmlFile(GeneralSceneDescriptionXml, &DMXLibrary, GeneralSceneDescriptionName);
	
	return GeneralSceneDescription;
}

TArray<UDMXImportGDTF*> UDMXLibraryFromMVRFactory::CreateGDTFAssets(UObject* Parent, EObjectFlags Flags, const TSharedRef<FDMXZipper>& Zip, const UDMXMVRGeneralSceneDescription& GeneralSceneDescription)
{
	const FString Path = FPaths::GetPath(Parent->GetName()) + TEXT("/GDTFs");

	TArray<FAssetData> ExistingGDTFAssets;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssetsByPath(FName(Path), ExistingGDTFAssets);

	TArray<UDMXImportGDTF*> ExistingGDTFs;
	for (const FAssetData& AssetData : ExistingGDTFAssets)
	{
		if (UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(AssetData.GetAsset()))
		{
			ExistingGDTFs.AddUnique(GDTF);
		}
	}

	UDMXGDTFFactory* GDTFFactory = NewObject<UDMXGDTFFactory>();
	TArray<UDMXImportGDTF*> ImportedGDTFs;
	TArray<FString> ImportedGDTFNames;
	if (ExistingGDTFs.Num() > 0)
	{
		const FText MessageText = LOCTEXT("MVRImportReimportsGDTFDialog", "MVR contains existing GDTFs. Do you want to reimport the existing GDTF assets?");
		if (FMessageDialog::Open(EAppMsgType::YesNo, MessageText) == EAppReturnType::Yes)
		{
			for (UDMXImportGDTF* GDTFAsset : ExistingGDTFs)
			{
				if (GDTFFactory->Reimport(GDTFAsset) == EReimportResult::Succeeded)
				{
					ImportedGDTFs.Add(GDTFAsset);

					if (UDMXGDTFAssetImportData* GDTFAssetImportData = GDTFAsset->GetGDTFAssetImportData())
					{
						const FString SourceFilename = FPaths::GetCleanFilename(GDTFAssetImportData->GetFilePathAndName());
						ImportedGDTFNames.Add(SourceFilename);
					}
				}
			}
		}
	}

	// Import GDTF Assets that aren't yet imported in the reimport procedure
	TArray<UDMXMVRFixtureNode*> FixtureNodes;
	GeneralSceneDescription.GetFixtureNodes(FixtureNodes);
	for (const UDMXMVRFixtureNode* FixtureNode : FixtureNodes)
	{
		// Don't import the same GDTF twice
		if (!ImportedGDTFNames.Contains(FixtureNode->GDTFSpec))
		{
			using namespace UE::DMX::DMXLibraryFromMVRFactory::Private;

			FString GDTFFilename;
			if (!FindGDTFFilenameInMVRZip(Zip, FixtureNode->GDTFSpec, GDTFFilename))
			{
				UE_LOG(LogDMXEditor, Warning, TEXT("Cannot find a GDTF file that corresponds to GDTF Spec '%s' of Fixture Node '%s'. Ignoring Fixture."), *FixtureNode->GDTFSpec, *FixtureNode->Name)
				{
					continue;
				}
			}

			const FDMXZipper::FDMXScopedUnzipToTempFile ScopedUnzipGDTF(Zip, GDTFFilename);
			if (!ScopedUnzipGDTF.TempFilePathAndName.IsEmpty())
			{
				constexpr bool bRemovePathFromDesiredName = true;
				const FString BaseFileName = FPaths::GetBaseFilename(ScopedUnzipGDTF.TempFilePathAndName, bRemovePathFromDesiredName);
				FString AssetName = ObjectTools::SanitizeObjectName(BaseFileName);

				FString GDTFPackageName;
				FString GDTFAssetName;
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
				AssetToolsModule.Get().CreateUniqueAssetName(Path / AssetName, TEXT(""), GDTFPackageName, GDTFAssetName);

				UPackage* Package = CreatePackage(*GDTFPackageName);
				check(Package);
				Package->FullyLoad();

				bool bCanceled;
				UObject* NewGDTFObject = GDTFFactory->FactoryCreateFile(UDMXImportGDTF::StaticClass(), Package, FName(*FixtureNode->GDTFSpec), Flags | RF_Public, ScopedUnzipGDTF.TempFilePathAndName, nullptr, GWarn, bCanceled);
				
				if (UDMXImportGDTF* NewGDTF = Cast<UDMXImportGDTF>(NewGDTFObject))
				{
					ImportedGDTFs.Add(NewGDTF);
					ImportedGDTFNames.Add(FixtureNode->GDTFSpec);

					FAssetRegistryModule::AssetCreated(NewGDTF);
					Package->MarkPackageDirty();
				}
			}
		}
	}

	return ImportedGDTFs;
}

void UDMXLibraryFromMVRFactory::InitDMXLibrary(UDMXLibrary* DMXLibrary, const TArray<UDMXImportGDTF*>& GDTFAssets, UDMXMVRGeneralSceneDescription* GeneralSceneDescription) const
{
	if (!ensureAlwaysMsgf(DMXLibrary, TEXT("Trying to initialize a DMX Library from MVR, but the DMX Library is not valid.")))
	{
		return;
	}
	if (!ensureAlwaysMsgf(GeneralSceneDescription, TEXT("Trying to initialize a DMX Library '%s' from MVR, but the DMX Library is not valid."), *DMXLibrary->GetName()))
	{
		return;
	}
	DMXLibrary->SetMVRGeneralSceneDescription(GeneralSceneDescription);

	// Get or create a Fixture Type for each GDTF
	TArray<UDMXEntityFixtureType*> FixtureTypes = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixtureType>();
	TMap<FString, UDMXEntityFixtureType*> GDTFSpecToFixtureTypeMap;
	TMap<UDMXEntityFixtureType*, FLinearColor> FixtureTypeToColorMap;
	for (UDMXImportGDTF* GDTF : GDTFAssets)
	{
		UDMXGDTFAssetImportData* GDTFAssetImportData = GDTF->GetGDTFAssetImportData();
		if (!GDTFAssetImportData)
		{
			continue;
		}
		const FString GDTFSourceFilename = GDTFAssetImportData->GetFilePathAndName();
		const FString GDTFFilename = FPaths::GetCleanFilename(GDTFSourceFilename);

		UDMXEntityFixtureType** ExistingFixtureTypePtr = Algo::FindByPredicate(FixtureTypes, [GDTF](const UDMXEntityFixtureType* FixtureType)
			{
				return FixtureType->DMXImport == GDTF;
			});
		if (ExistingFixtureTypePtr)
		{
			GDTFSpecToFixtureTypeMap.Add(GDTFFilename, *ExistingFixtureTypePtr);
		}
		else
		{
			FDMXEntityFixtureTypeConstructionParams FixtureTypeConstructionParams;
			FixtureTypeConstructionParams.DMXCategory = FDMXFixtureCategory(FDMXFixtureCategory::GetFirstValue());
			FixtureTypeConstructionParams.ParentDMXLibrary = DMXLibrary;

			UDMXEntityFixtureType* NewFixtureType = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(FixtureTypeConstructionParams, FPaths::GetBaseFilename(GDTFFilename));
			const bool bAdvancedImportSuccess = FDMXInitializeFixtureTypeFromGDTFHelper::GenerateModesFromGDTF(*NewFixtureType, *GDTF);
			if (!bAdvancedImportSuccess)
			{
				UE_LOG(LogDMXEditor, Warning, TEXT("Failed to initialize Fixture Type '%s', falling back to legacy method that doesn't support matrix fixtures."), *NewFixtureType->GetName());
				NewFixtureType->SetModesFromDMXImport(GDTF);
			}
			NewFixtureType->DMXImport = GDTF;

			GDTFSpecToFixtureTypeMap.Add(GDTFFilename, NewFixtureType);
		}
	}

	// Remove Fixture Patches that have no corresponding Fixture Node in the General Scene Description
	TArray<UDMXMVRFixtureNode*> FixtureNodes;
	GeneralSceneDescription->GetFixtureNodes(FixtureNodes);
	TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		UDMXMVRFixtureNode** CorrespondingFixtureNodePtr = Algo::FindByPredicate(FixtureNodes, [FixturePatch](const UDMXMVRFixtureNode* FixtureNode)
			{
				return FixturePatch->GetMVRFixtureUUID() == FixtureNode->UUID;
			});
		if (!CorrespondingFixtureNodePtr)
		{
			checkNoEntry();
			UDMXEntityFixturePatch::RemoveFixturePatchFromLibrary(FDMXEntityFixturePatchRef(FixturePatch));
		}
	}
	FixturePatches = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	// Update or create new Fixture Patches for the MVR Fixtures
	for (const UDMXMVRFixtureNode* FixtureNode : FixtureNodes)
	{
		// Find the GDTF Filename, irregardless if it has the .gdtf extension
		FString GDTFFilename = FixtureNode->GDTFSpec;
		if (!GDTFSpecToFixtureTypeMap.Contains(GDTFFilename))
		{
			GDTFFilename = FixtureNode->GDTFSpec + TEXT(".gdtf");
		}
		if (!GDTFSpecToFixtureTypeMap.Contains(GDTFFilename))
		{
			continue;
		}

		UDMXEntityFixtureType* FixtureType = GDTFSpecToFixtureTypeMap[GDTFFilename];
		if (!FixtureTypeToColorMap.Contains(FixtureType))
		{
			FLinearColor FixtureTypeColor = FLinearColor::MakeRandomColor();

			// Avoid dominant red values for a bit more of a professional feel
			if (FixtureTypeColor.R > 0.75f)
			{
				FixtureTypeColor.R = FMath::Abs(FixtureTypeColor.R - 1.0f);
			}

			FixtureTypeToColorMap.Add(FixtureType, FixtureTypeColor);
		}

		int32 ActiveModeIndex = FixtureType->Modes.IndexOfByPredicate([FixtureNode](const FDMXFixtureMode& Mode)
			{
				return Mode.ModeName == FixtureNode->GDTFMode;
			});

		if (ActiveModeIndex != INDEX_NONE)
		{
			UDMXEntityFixturePatch* const* ExistingFixturePatchPtr = Algo::FindByPredicate(FixturePatches, [FixtureNode](const UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch->GetMVRFixtureUUID() == FixtureNode->UUID;
				});
			if (ExistingFixturePatchPtr)
			{			
				// Update the existing patch (matching MVR UUID) if possible
				UDMXEntityFixturePatch& ExistingFixturePatch = **ExistingFixturePatchPtr;

				ExistingFixturePatch.SetFixtureType(FixtureType);
				ExistingFixturePatch.SetActiveModeIndex(ActiveModeIndex);
				ExistingFixturePatch.SetUniverseID(FixtureNode->GetUniverseID());
				ExistingFixturePatch.SetStartingChannel(FixtureNode->GetStartingChannel());
				ExistingFixturePatch.EditorColor = FixtureTypeToColorMap.FindChecked(FixtureType);
			}
			else
			{
				// Create a new patch
				FDMXEntityFixturePatchConstructionParams FixturePatchConstructionParams;
				FixturePatchConstructionParams.ActiveMode = ActiveModeIndex;
				FixturePatchConstructionParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureType);
				FixturePatchConstructionParams.UniverseID = FixtureNode->GetUniverseID();
				FixturePatchConstructionParams.StartingAddress = FixtureNode->GetStartingChannel();
				FixturePatchConstructionParams.MVRFixtureUUID = FixtureNode->UUID;

				UDMXEntityFixturePatch* FixturePatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(FixturePatchConstructionParams, FixtureNode->Name);
				FixturePatch->EditorColor = FixtureTypeToColorMap.FindChecked(FixtureType);
			}
		}
		else
		{
			UE_LOG(LogDMXEditor, Warning, TEXT("Skipped creating a Fixture Patch for '%s', cannot find Mode '%s' in '%s'."), *FixtureNode->Name, *FixtureNode->GDTFMode, *FixtureNode->GDTFSpec)
		}
	}

	DMXLibrary->UpdateGeneralSceneDescription();
}

UDMXMVRAssetImportData* UDMXLibraryFromMVRFactory::GetMVRAssetImportData(UObject* DMXLibraryObject) const
{
	UDMXLibrary* DMXLibrary = Cast<UDMXLibrary>(DMXLibraryObject);
	if (!DMXLibrary)
	{
		return nullptr;
	}

	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = DMXLibrary->GetLazyGeneralSceneDescription();
	if (!ensureMsgf(GeneralSceneDescription, TEXT("Invalid General Scene Description in DMX Library.")))
	{
		return nullptr;
	}

	return GeneralSceneDescription->GetMVRAssetImportData();
}

#undef LOCTEXT_NAMESPACE
