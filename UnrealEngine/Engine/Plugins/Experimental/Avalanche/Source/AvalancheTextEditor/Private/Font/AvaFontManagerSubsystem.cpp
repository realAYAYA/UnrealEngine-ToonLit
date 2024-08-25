// Copyright Epic Games, Inc. All Rights Reserved.

#include "Font/AvaFontManagerSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AutomatedAssetImportData.h"
#include "AvaFontView.h"
#include "Editor.h"
#include "Factories/FontFileImportFactory.h"
#include "Font/AvaFont.h"
#include "Font/AvaFontObject.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ScopedSlowTask.h"
#include "PropertyHandle.h"
#include "SystemFontLoading.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SavePackage.h"

/** ThirdParty */
#include "EditorAssetLibrary.h"
#include "freetype/freetype.h"

DEFINE_LOG_CATEGORY(LogAvaFont);

#define LOCTEXT_NAMESPACE "AvaFontManagerSubsystem"

UAvaFontManagerSubsystem* UAvaFontManagerSubsystem::Get()
{
	if (!GEditor)
	{
		return nullptr;
	}

	return GEditor->GetEditorSubsystem<UAvaFontManagerSubsystem>();
}

void UAvaFontManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Initialize call below ends up calling PECP on newly created UFontFace objects
	// which thus trying to access FSlateApplication::Get() which might be invalid for headless setups
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	Initialize();

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// Initialize project fonts as soon as AssetRegistry module is ready for that
	// Project fonts will be found, compared to System fonts to avoid duplicates, and then added to the list including both system and project fonts
	AssetRegistryModule.Get().OnFilesLoaded().AddUObject(this, &UAvaFontManagerSubsystem::InitializeProjectFonts);

}

void UAvaFontManagerSubsystem::Deinitialize()
{
	UnregisterDefaultAutoImportCallbacks();
	UnregisterAssetsCallbacks();

	ClearFontsData();

	CleanFreeTypeLibrary();

	Super::Deinitialize();
}

void UAvaFontManagerSubsystem::ClearFontsData()
{
	OSFontsMap.Empty();
	ProjectFontsMap.Empty();
	FontsMap.Empty();
	FontsOptions.Empty();
	FontsToImportOnSave.Empty();
}

void UAvaFontManagerSubsystem::RegisterDefaultAutoImportCallbacks()
{
	// auto saving non imported fonts which are in use in level editor
	FEditorDelegates::PreSaveWorldWithContext.AddUObject(this, &UAvaFontManagerSubsystem::FontsAutoImportPreSave);
}

void UAvaFontManagerSubsystem::UnregisterDefaultAutoImportCallbacks() const
{
	FEditorDelegates::PreSaveWorldWithContext.RemoveAll(this);
	FEditorDelegates::PostSaveWorldWithContext.RemoveAll(this);
}

void UAvaFontManagerSubsystem::Initialize()
{
	UnregisterAssetsCallbacks();

	LoadFavorites();

	// init OS fonts only, project fonts can be loaded lately with InitializeProjectFonts()
	InitializeOSFonts();

	RefreshFontsMap();
	RefreshFontsOptions();

	RegisterAssetsCallbacks();

	RegisterDefaultAutoImportCallbacks();
}

bool UAvaFontManagerSubsystem::IsSupportedFontFile(const FString& InFontFilePath)
{
	const FString FileName = FPaths::GetCleanFilename(InFontFilePath);
	const FString Extension = FPaths::GetExtension(FileName).ToLower();

	return Extension == TEXT("ttf") || Extension == TEXT("otf");
}

void UAvaFontManagerSubsystem::OnAssetsAdded(const FAssetData& InAssetData)
{
	if (InAssetData.AssetClassPath != UFont::StaticClass()->GetClassPathName())
	{
		return;
	}

	if (UFont* CurrFont = Cast<UFont>(InAssetData.GetAsset()))
	{
		CreateProjectFont(CurrFont);

		RefreshFontsMap();
		RefreshFontsOptions();

		OnProjectFontCreatedDelegate.Broadcast(CurrFont);
	}
}

void UAvaFontManagerSubsystem::OnAssetDeleted(UObject* InObject)
{
	if (!IsValid(InObject))
	{
		return;
	}

	if (InObject->GetClass()->IsChildOf<UFont>())
	{
		const UFont* const FontToBeDeleted = Cast<UFont>(InObject);

		if (IsValid(FontToBeDeleted))
		{
			OnProjectFontDeletedDelegate.Broadcast(FontToBeDeleted);
			RemoveProjectFont(FontToBeDeleted);
		}

		// this should allow the assets to still point to the same font, in case the deleted font is a system one
		LoadOSFonts();

		RefreshFontsMap();
		RefreshFontsOptions();
	}
}

void UAvaFontManagerSubsystem::LoadFavorites()
{
	if (!IsValid(FontManagerConfig))
	{
		static const FString PackageName = GetFontConfigPackageName();
		UPackage* NewPackage = CreatePackage(*PackageName);
		NewPackage->SetFlags(RF_Transient);
		NewPackage->AddToRoot();
		FontManagerConfig = NewObject<UAvaFontConfig>(NewPackage, "MotionDesignFontConfig", RF_Transient | RF_Transactional | RF_Standalone);
	}
}

void UAvaFontManagerSubsystem::SanitizeString(FString& OutSanitizedName)
{
	// spaces would be also handled by code below, but spaces used to be removed
	// so let's to this anyway in order to avoid any potential issues with new vs. old imported fonts

	const TCHAR* InvalidObjectChar = INVALID_OBJECTNAME_CHARACTERS;
	while (*InvalidObjectChar)
	{
		OutSanitizedName.ReplaceCharInline(*InvalidObjectChar, TCHAR(' '), ESearchCase::CaseSensitive);
		++InvalidObjectChar;
	}

	const TCHAR* InvalidPackageChar = INVALID_LONGPACKAGE_CHARACTERS;
	while (*InvalidPackageChar)
	{
		OutSanitizedName.ReplaceCharInline(*InvalidPackageChar, TCHAR(' '), ESearchCase::CaseSensitive);
		++InvalidPackageChar;
	}

	OutSanitizedName.RemoveSpacesInline();
}

bool UAvaFontManagerSubsystem::ImportAsAsset(UAvaFontObject* InFontToImport)
{
	// Importing ends up calling PECP on newly created UFontFace objects
	// which thus trying to access FSlateApplication::Get() which might be invalid for headless setups
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	if (IsValid(InFontToImport))
	{
		if (InFontToImport->GetSource() == EAvaFontSource::System)
		{
			const FString& FontToImportName = InFontToImport->GetFontName();

			// system font name might include invalid characters, let's handle them
			FString SanitizedFontFamilyName(FontToImportName);
			SanitizeString(SanitizedFontFamilyName);

			const FString FontPackageName = GetImportFontPackageNameRoot() + TEXT("/Fonts/") + SanitizedFontFamilyName;

			UPackage* FontPackage = CreatePackage(*FontPackageName);

			if (IsValid(FontPackage))
			{
				const UFont* Font = InFontToImport->GetFont();
				if (!IsValid(Font))
				{
					return false;
				}

				const FString FontAssetFileName = FPackageName::LongPackageNameToFilename(FontPackage->GetPathName(), FPackageName::GetAssetPackageExtension());

				FontPackage->AddToRoot();
				FontPackage->FullyLoad();

				UFont* NewFont = NewObject<UFont>(FontPackage, *Font->GetName(), RF_Public | RF_Standalone);
				NewFont->ImportOptions.FontName = FontToImportName;
				NewFont->LegacyFontName = FName(FontToImportName);
				NewFont->FontCacheType = EFontCacheType::Runtime;

				SetupFontFamilyTypefaces(NewFont, InFontToImport->GetFontParameters());

				FAssetRegistryModule::AssetCreated(NewFont);
				FontPackage->SetDirtyFlag(true);

				InFontToImport->InitProjectFont(NewFont, InFontToImport->GetFontName());

				if (!FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*FontAssetFileName))
				{
					FSavePackageArgs FontSavePackageArgs;
					FontSavePackageArgs.TopLevelFlags = RF_Public | RF_Standalone;
					UPackage::SavePackage(FontPackage, NewFont, *FontAssetFileName, FontSavePackageArgs);
				}

				if (OSFontsMap.Contains(FontToImportName))
				{
					OSFontsMap.Remove(FontToImportName);

					// Creates a new AvaFontObject and updates the ProjectsFontsMap as well
					CreateProjectFont(NewFont);
				}

				RefreshFontsMap();
				RefreshFontsOptions();
				OnSystemFontsUpdatedDelegate.Broadcast();

				return true;
			}
		}
	}

	return false;
}

bool UAvaFontManagerSubsystem::ImportFont(const TSharedPtr<IPropertyHandle>& InAvaFontPropertyHandle)
{
	bool bSuccess = false;

	if (const TSharedPtr<FAvaFontView>& AvaFont = GetFontViewFromPropertyHandle(InAvaFontPropertyHandle))
	{
		// we don't want to trigger automatic behavior for manually imported fonts
		UnregisterAssetsCallbacks();

		bSuccess = ImportAsAsset(AvaFont->GetFontObject());

		if (bSuccess)
		{
			ForceFontAutoImportUnmark(InAvaFontPropertyHandle);
			InAvaFontPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}

		// register callbacks again
		RegisterAssetsCallbacks();
	}

	return bSuccess;
}

void UAvaFontManagerSubsystem::RefreshSystemFont(UFont* InFont) const
{
	if (!InFont)
	{
		return;
	}

	const FString& FontFamilyName = InFont->GetName();

	FScopedSlowTask DeleteReimportTypefaceSlowTask(1
		, FText::Format(LOCTEXT("DeleteAndReimportTypeface", "Refreshing {0} font.")
		, FText::FromString(FontFamilyName)));

	DeleteReimportTypefaceSlowTask.MakeDialog();
	DeleteReimportTypefaceSlowTask.EnterProgressFrame();

	// We cannot update fonts which are not available on this system
	if (!FontsInfoMap.Contains(FontFamilyName))
	{
		return;
	}

	// We remove old Font Faces
	for (const FTypefaceEntry& TypefaceEntry : InFont->CompositeFont.DefaultTypeface.Fonts)
	{
		if (const UFontFace* FontFace = Cast<UFontFace>(TypefaceEntry.Font.GetFontFaceAsset()))
		{
			UEditorAssetLibrary::DeleteAsset(FontFace->GetPathName());
		}
	}

	// We empty font faces from font
	InFont->CompositeFont.DefaultTypeface.Fonts.Reset();

	// Setup font typefaces
	SetupFontFamilyTypefaces(InFont, FontsInfoMap[FontFamilyName]);

	if (UPackage* FontPackage = InFont->GetPackage())
	{
		FontPackage->SetDirtyFlag(true);

		const FString FontAssetFileName = FPackageName::LongPackageNameToFilename(FontPackage->GetPathName(), FPackageName::GetAssetPackageExtension());

		if (!FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*FontAssetFileName))
		{
			FSavePackageArgs SavePackageArgs;
        	SavePackageArgs.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(FontPackage, InFont, *FontAssetFileName, SavePackageArgs);
		}
	}
}

bool UAvaFontManagerSubsystem::IsFontAvailableOnLocalOS(const UFont* InFont) const
{
	if (!InFont)
	{
		return false;
	}

	const FString& FontFamilyName = InFont->GetName();

	// We cannot update fonts which are not available on this system
	return FontsInfoMap.Contains(FontFamilyName);
}

UFontFace* UAvaFontManagerSubsystem::GetFallbackFontFace()
{
	if (!IsValid(DefaultFallbackFontFace))
	{
		DefaultFallbackFontFace = LoadObject<UFontFace>(nullptr, GetFallbackFontFaceReference());
	}

	return DefaultFallbackFontFace;
}

TArray<FString> UAvaFontManagerSubsystem::GetSystemFontNames() const
{
	TArray<FString> OutFontsNames;

	FontsInfoMap.GetKeys(OutFontsNames);

	return OutFontsNames;
}

TArray<FString> UAvaFontManagerSubsystem::GetSystemFontTypefaces(const FString& InFontFamilyName) const
{
	TArray<FString> OutTypefaces;
	if (FontsInfoMap.Contains(InFontFamilyName))
	{
		OutTypefaces = FontsInfoMap[InFontFamilyName].GetFontFaceNames();
	}

	return OutTypefaces;
}

void UAvaFontManagerSubsystem::SetupFontFamilyTypefaces(UFont* InFont, const FSystemFontsRetrieveParams& InFontParams)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	const FString& FontFamilyName = InFontParams.FontFamilyName;

	const bool bIsTempAsset = InFont->GetPackage()->GetName().Contains(GetTempFontPackageNameRoot());
	const FString PackagePath = bIsTempAsset ? GetTempFontPackageNameRoot() : GetImportFontPackageNameRoot();

	UPackage* ImportPackage = CreatePackage(*PackagePath);
	ImportPackage->AddToRoot();

	UFontFileImportFactory* FontFaceFactory = NewObject<UFontFileImportFactory>();
	FontFaceFactory->AddToRoot();

	const UAutomatedAssetImportData* AutomatedAssetImportData = NewObject<UAutomatedAssetImportData>();
	FontFaceFactory->SetAutomatedAssetImportData(AutomatedAssetImportData);

	constexpr EObjectFlags ImportFontAssetFlags = RF_Public | RF_Standalone;
	constexpr EObjectFlags TempFontAssetFlags = ImportFontAssetFlags | RF_Transient;

	const EObjectFlags NewFontFaceFlags = bIsTempAsset ? TempFontAssetFlags : ImportFontAssetFlags;

	for (int32 Index = 0; Index < InFontParams.GetFontFaceNames().Num(); Index++)
	{
		const FString& FontPath = InFontParams.GetFontFacePaths()[Index];
		const FString& FontFaceName = InFontParams.GetFontFaceNames()[Index];
		if (!IsSupportedFontFile(FontPath))
		{
			continue;
		}

		FString FontFaceAssetName = FontFamilyName + "_" + FontFaceName;
		SanitizeString(FontFaceAssetName);
		UPackage* FontFacePackage = CreatePackage(*(ImportPackage->GetName() + TEXT("/FontFaces/") + FontFaceAssetName));
		FontFacePackage->AddToRoot();
		FontFacePackage->FullyLoad();

		bool bCanceled = false;
		UObject* NewFontFaceAsObject = FontFaceFactory->ImportObject(UFontFace::StaticClass(), FontFacePackage, *FontFaceAssetName, NewFontFaceFlags, FontPath, TEXT(""), bCanceled);
		UFontFace* NewFontFace = Cast<UFontFace>(NewFontFaceAsObject);

		if (!NewFontFace)
		{
			continue;
		}

		if (!bIsTempAsset)
		{
			FAssetRegistryModule::AssetCreated(NewFontFace);
			FontFacePackage->SetDirtyFlag(true);

			const FString StyleFontFaceAssetFileName = FPackageName::LongPackageNameToFilename(FontFacePackage->GetPathName(), FPackageName::GetAssetPackageExtension());

			if (!FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*StyleFontFaceAssetFileName))
			{
				FSavePackageArgs SavePackageArgs;
				SavePackageArgs.TopLevelFlags = RF_Public | RF_Standalone;
				UPackage::SavePackage(FontFacePackage, NewFontFace, *StyleFontFaceAssetFileName, SavePackageArgs);
			}
		}

		if (NewFontFace)
		{
			FTypefaceEntry& Entry = InFont->CompositeFont.DefaultTypeface.Fonts[InFont->CompositeFont.DefaultTypeface.Fonts.AddDefaulted()];
			Entry.Name = FName(FName::NameToDisplayString(FontFaceName, false));
			Entry.Font = FFontData(NewFontFace);
		}
	}

	FontFaceFactory->RemoveFromRoot();
}

bool UAvaFontManagerSubsystem::Exec(class UWorld* InWorld, const TCHAR* InCmd, FOutputDevice& InAr)
{
	if (FParse::Command(&InCmd, TEXT("fontmanager")))
	{
		if (FParse::Command(&InCmd, TEXT("list")))
		{
			UE::Ava::Private::Fonts::ListAvailableFontFiles();
			return true;
		}
		else if (FParse::Command(&InCmd, TEXT("refresh")))
		{
			if (!HasAnyFlags(RF_ClassDefaultObject))
			{
				RefreshAllSystemFonts();
				return true;
			}
		}
	}

	return false;
}

void UAvaFontManagerSubsystem::CombineAllFonts()
{
	FontsMap.Empty();
	FontsMap.Append(ProjectFontsMap);
	FontsMap.Append(OSFontsMap);
}

void UAvaFontManagerSubsystem::UnregisterAssetsCallbacks() const
{
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");

		if (AssetRegistryModule.IsValid())
		{
			AssetRegistryModule.Get().OnAssetAdded().RemoveAll(this);
			AssetRegistryModule.Get().OnInMemoryAssetDeleted().RemoveAll(this);
		}
	}
}

void UAvaFontManagerSubsystem::RegisterAssetsCallbacks()
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	if (AssetRegistryModule.IsValid())
	{
		AssetRegistryModule.Get().OnAssetAdded().AddUObject(this, &UAvaFontManagerSubsystem::OnAssetsAdded);
		AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddUObject(this, &UAvaFontManagerSubsystem::OnAssetDeleted);
	}
}

void UAvaFontManagerSubsystem::InitializeProjectFonts()
{
	ProjectFontsMap.Empty();

	LoadProjectFonts();

	RemoveProjectFontsFromOSFonts();
}

void UAvaFontManagerSubsystem::InitializeOSFonts()
{
	OSFontsMap.Empty();
	LoadOSFonts();
}

void UAvaFontManagerSubsystem::LoadProjectFonts()
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	if (!AssetRegistryModule.IsValid())
	{
		return;
	}

	TArray<FAssetData> AssetData;
	const UClass* Class = UFont::StaticClass();

	const FTopLevelAssetPath AssetPath(Class->GetPathName());
	AssetRegistryModule.Get().GetAssetsByClass(AssetPath, AssetData);

	// we don't want to go through temp system fonts, so we filter them based on their Package Path since we know where we put them
	TArray<FAssetData> AssetDataArrayForPackageName = AssetData.FilterByPredicate([&](const FAssetData& AssetDataItem) -> bool
		{
			const FName PathToExcludeTemp = *(GetTempFontPackageNameRoot()/TEXT("Fonts"));
			return (AssetDataItem.PackagePath != PathToExcludeTemp);
		});

	const FString& TempPackageName = UAvaFontManagerSubsystem::GetFontObjPackageNameRoot();
	UPackage* ProjectFontsPackage = CreatePackage(*TempPackageName);
	ProjectFontsPackage->AddToRoot();

	TArray<FString> CurrFontAssetsNamesArray;

	// check if there are new fonts to be added from Project Assets
	for (const FAssetData& Elem : AssetDataArrayForPackageName)
	{
		UFont* CurrFont = Cast<UFont>(Elem.GetAsset());

		if (IsValid(CurrFont))
		{
			if (const UAvaFontObject* const NewAvaFontObj = CreateProjectFont(CurrFont, ProjectFontsPackage))
			{
				CurrFontAssetsNamesArray.Add(NewAvaFontObj->GetFontName());
				CurrFont->GetPackage()->FullyLoad();
			}
		}
	}
}
void UAvaFontManagerSubsystem::RefreshAllSystemFonts() const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// Refreshing might end up calling PECP on newly created UFontFace objects
	// which thus trying to access FSlateApplication::Get() which might be invalid for headless setups
	if (!AssetRegistryModule.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	TArray<FAssetData> AssetData;
	const UClass* Class = UFont::StaticClass();

	const FTopLevelAssetPath AssetPath(Class->GetPathName());
	AssetRegistryModule.Get().GetAssetsByClass(AssetPath, AssetData);

	// We want to go through imported system fonts, so we filter them based on their Package Path since we know where we put them
	TArray<FAssetData> AssetDataArrayForPackageName = AssetData.FilterByPredicate([&](const FAssetData& AssetDataItem) -> bool
		{
			const FName PathToExcludeTemp = *(GetImportFontPackageNameRoot()/TEXT("Fonts"));
			return (AssetDataItem.PackagePath == PathToExcludeTemp);
		});
		TArray<FString> CurrFontAssetsNamesArray;

	if (AssetDataArrayForPackageName.IsEmpty())
	{
		return;
	}

	if (FontsInfoMap.IsEmpty())
	{
		return;
	}

	FScopedSlowTask FixTypefacesSlowTask(AssetDataArrayForPackageName.Num(), LOCTEXT("FixingTypefaces", "Refreshing typefaces for imported system fonts"));
	FixTypefacesSlowTask.MakeDialog();

	// Check if there are new fonts to be added from Project Assets
	for (const FAssetData& AssetDataItem : AssetDataArrayForPackageName)
	{
		FixTypefacesSlowTask.EnterProgressFrame();

		UFont* Font = Cast<UFont>(AssetDataItem.GetAsset());
		if (!Font)
		{
			continue;
		}

		RefreshSystemFont(Font);
	}
}

bool UAvaFontManagerSubsystem::RemoveProjectFont(const UFont* InSourceFont)
{
	if (IsValid(InSourceFont))
	{
		FString SourceFontName;
		GetSanitizedFontName(InSourceFont, SourceFontName);

		if (ProjectFontsMap.Contains(SourceFontName))
		{
			if (UAvaFontObject* AvaFontObj = ProjectFontsMap.FindAndRemoveChecked(SourceFontName))
			{
				AvaFontObj->Invalidate();
			}

			return true;
		}
	}
	return false;
}

UAvaFontObject* UAvaFontManagerSubsystem::CreateProjectFont(UFont* InSourceFont, UPackage* InProjectFontsPackage)
{
	if (IsValid(InSourceFont))
	{
		if (!InSourceFont->ImportOptions.bUseDistanceFieldAlpha) // ignoring distance field fonts
		{
			if (InSourceFont->GetCompositeFont())
			{
				FString SanitizedFontName;
				FString FontName;
				GetFontName(InSourceFont, FontName);
				GetSanitizedFontName(InSourceFont, SanitizedFontName);

				if (ProjectFontsMap.Contains(SanitizedFontName))
				{
					return nullptr;
				}

				UPackage* ProjectFontsPackage = InProjectFontsPackage;
				if (!IsValid(ProjectFontsPackage))
				{
					const FString TempPackageName = UAvaFontManagerSubsystem::GetFontObjPackageNameRoot();
					ProjectFontsPackage = CreatePackage(*TempPackageName);
					ProjectFontsPackage->AddToRoot();
				}

				UAvaFontObject* NewAvaFont = NewObject<UAvaFontObject>(ProjectFontsPackage, *SanitizedFontName, RF_Public | RF_Standalone | RF_Transient);
				NewAvaFont->AddToRoot();

				NewAvaFont->InitProjectFont(InSourceFont, FontName);
				UAvaFontManagerSubsystem::SetupMetrics(NewAvaFont);

				ProjectFontsMap.Add(SanitizedFontName, NewAvaFont);

				return NewAvaFont;
			}
			else
			{
				UE_LOG(LogAvaFont, Warning, TEXT("No composite font found for font %s"), *InSourceFont->GetName());
			}
		}
	}

	return nullptr;
}

void UAvaFontManagerSubsystem::LoadOSFonts()
{
	UE::Ava::Private::Fonts::GetSystemFontInfo(FontsInfoMap);

	if (FontsInfoMap.IsEmpty())
	{
		return;
	}

	UPackage* OSFontsPackage = CreatePackage(*GetTempFontPackageNameRoot());
	OSFontsPackage->AddToRoot();

	for (const TPair<FString, FSystemFontsRetrieveParams>& FontParamsPair : FontsInfoMap)
	{
		const FSystemFontsRetrieveParams& FontParams = FontParamsPair.Value;
		ImportSystemFontFamily(FontParams, OSFontsPackage);
	}

	OnSystemFontsUpdatedDelegate.Broadcast();
}

void UAvaFontManagerSubsystem::RemoveProjectFontsFromOSFonts()
{
	for (const TPair<FString, TObjectPtr<UAvaFontObject>>& ProjectFontPair : ProjectFontsMap)
	{
		const FString& FontName = ProjectFontPair.Key;
		if (OSFontsMap.Contains(FontName))
		{
			if (const TObjectPtr<UAvaFontObject> FontToMove = OSFontsMap[FontName])
			{
				OSFontsMap.Remove(FontName);
				FontToMove->SwitchToProjectFont();
			}
		}
	}

	RefreshFontsMap();
	RefreshFontsOptions();
	OnSystemFontsUpdatedDelegate.Broadcast();
}

void UAvaFontManagerSubsystem::ImportSystemFontFamily(const FSystemFontsRetrieveParams& InFontParams, UPackage* InImportPackage)
{
	UPackage* ImportPackage;

	if (IsValid(InImportPackage))
	{
		ImportPackage = InImportPackage;
	}
	else
	{
		ImportPackage = CreatePackage(*GetTempFontPackageNameRoot());
		ImportPackage->AddToRoot();
	}

	if (InFontParams.GetFontFaceNames().IsEmpty())
	{
		return;
	}

	const FString& FontFamilyName = InFontParams.FontFamilyName;

	// this font is already in the map, just ignore and return
	if (OSFontsMap.Contains(FontFamilyName) || ProjectFontsMap.Contains(FontFamilyName))
	{
		return;
	}

	FString SanitizedFontFamilyName(FontFamilyName);
	SanitizeString(SanitizedFontFamilyName);
	UPackage* FontPackage = CreatePackage(*(ImportPackage->GetName() + TEXT("/Fonts/") + SanitizedFontFamilyName));
	FontPackage->AddToRoot();

	constexpr EObjectFlags TempFontAssetFlags = RF_Public | RF_Standalone | RF_Transient;

	UFont* NewFont = NewObject<UFont>(FontPackage, *SanitizedFontFamilyName, TempFontAssetFlags);
	NewFont->AddToRoot();
	NewFont->ImportOptions.FontName = FontFamilyName;
	NewFont->LegacyFontName = FName(FontFamilyName);
	NewFont->FontCacheType = EFontCacheType::Runtime;

	SetupFontFamilyTypefaces(NewFont, InFontParams);
	UAvaFontObject* NewAvaFont = NewObject<UAvaFontObject>(ImportPackage,  *SanitizedFontFamilyName, TempFontAssetFlags);
	NewAvaFont->AddToRoot();

	FTypefaceEntry& FallbackTypefaceEntry = NewFont->CompositeFont.FallbackTypeface.Typeface.Fonts[NewFont->CompositeFont.FallbackTypeface.Typeface.Fonts.AddDefaulted()];
	FallbackTypefaceEntry.Font = FFontData(GetFallbackFontFace());
	FallbackTypefaceEntry.Name = FName("Regular");

	NewAvaFont->InitSystemFont(InFontParams, NewFont);
	UAvaFontManagerSubsystem::SetupMetrics(NewAvaFont);
	OSFontsMap.Add(SanitizedFontFamilyName, NewAvaFont);
}

void UAvaFontManagerSubsystem::SortFontsMap()
{
	FontsMap.KeySort([](const FString& A, const FString& B)
	{
		return A < B;
	});
}

void UAvaFontManagerSubsystem::RefreshFontsMap()
{
	CombineAllFonts();
	SortFontsMap();
}

TConstArrayView<const TSharedPtr<FAvaFontView>> UAvaFontManagerSubsystem::GetFontOptions()
{
	return FontsOptions;
}

TSharedPtr<FAvaFontView> UAvaFontManagerSubsystem::GetFontViewFromName(const FString& InName)
{
	TSharedPtr<FAvaFontView>* FontView = FontsOptions.FindByPredicate([&InName](const TSharedPtr<FAvaFontView>& AvaFontValue)
	{
		if (AvaFontValue.IsValid() && AvaFontValue->HasValidFont())
		{
			return AvaFontValue->GetFontNameAsString().Equals(InName);
		}

		return false;
	});

	if (FontView)
	{
		return *FontView;
	}

	return nullptr;
}

TSharedPtr<FAvaFontView> UAvaFontManagerSubsystem::GetFontViewFromPropertyHandle(const TSharedPtr<IPropertyHandle>& InFontPropertyHandle, FPropertyAccess::Result& OutAccessResult)
{
	if (!InFontPropertyHandle)
	{
		OutAccessResult = FPropertyAccess::Fail;
		return nullptr;
	}

	FString FontName;

	void* FontRawData;
	OutAccessResult = InFontPropertyHandle->GetValueData(FontRawData);

	if (OutAccessResult == FPropertyAccess::Fail)
	{
		return nullptr;
	}

	if (OutAccessResult == FPropertyAccess::Success)
	{
		const FAvaFont* ActualFontRaw = static_cast<FAvaFont*>(FontRawData);
		if (!ActualFontRaw)
		{
			OutAccessResult = FPropertyAccess::Fail;
			return nullptr;
		}

		FontName = ActualFontRaw->GetFontNameAsString();
	}
	else if (OutAccessResult == FPropertyAccess::MultipleValues)
	{
		OutAccessResult = FPropertyAccess::MultipleValues;
		FAvaMultiFontSelectionData MultiFontSelectionData;
		GetMultipleSelectionInformation(InFontPropertyHandle, MultiFontSelectionData);
		if (!MultiFontSelectionData.bMultipleValues)
		{
			FontName = MultiFontSelectionData.FirstSelectionFontName;
		}
	}

	if (FontName.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FAvaFontView> FontView = GetFontViewFromName(FontName);

	// If result is Multiple Values, we don't consider it a Fail
	if (OutAccessResult != FPropertyAccess::MultipleValues)
	{
		// If font is nullptr, it's a Fail
		if (!FontView)
		{
			OutAccessResult = FPropertyAccess::Fail;
		}
	}

	return FontView;
}

FAvaFont* UAvaFontManagerSubsystem::GetFontFromPropertyHandle(const TSharedPtr<IPropertyHandle>& InFontPropertyHandle, FPropertyAccess::Result& OutAccessResult)
{
	if (!InFontPropertyHandle)
	{
		OutAccessResult = FPropertyAccess::Fail;
		return nullptr;
	}

	FAvaFont* AvaFont = nullptr;

	FString FontName;

	void* FontRawData;
	OutAccessResult = InFontPropertyHandle->GetValueData(FontRawData);

	if (OutAccessResult == FPropertyAccess::Fail)
	{
		return nullptr;
	}

	if (OutAccessResult == FPropertyAccess::Success)
	{
		FAvaFont* ActualFontRaw = static_cast<FAvaFont*>(FontRawData);
		if (!ActualFontRaw)
		{
			OutAccessResult = FPropertyAccess::Fail;
			return nullptr;
		}

		return ActualFontRaw;
	}
	else if (OutAccessResult == FPropertyAccess::MultipleValues)
	{
		OutAccessResult = FPropertyAccess::MultipleValues;
		FAvaMultiFontSelectionData MultiFontSelectionData;
		GetMultipleSelectionInformation(InFontPropertyHandle, MultiFontSelectionData);
		if (!MultiFontSelectionData.bMultipleValues)
		{
			FontName = MultiFontSelectionData.FirstSelectionFontName;
		}
	}

	if (FontName.IsEmpty())
	{
		return nullptr;
	}

	return AvaFont;
}

void UAvaFontManagerSubsystem::GetMultipleSelectionInformation(const TSharedPtr<IPropertyHandle>& InPropertyHandle
	, FAvaMultiFontSelectionData& OutMultiFontSelectionData)
{
	bool bMultipleObjectsSelected = false;
	bool bMultipleValues = false;
	FString FirstSelectionFontName = TEXT("");

	if (InPropertyHandle)
	{
		if (InPropertyHandle->GetNumOuterObjects() > 1)
		{
			bMultipleObjectsSelected = true;

			TArray<FString> ObjectValues;
			InPropertyHandle->GetPerObjectValues(ObjectValues);

			if (!ObjectValues.IsEmpty())
			{
				const FString FirstObjectValueString = ObjectValues[0];

				if (FParse::Value(*FirstObjectValueString, TEXT("FontName="), FirstSelectionFontName))
				{
					for (int32 ObjectIndex = 1; ObjectIndex < ObjectValues.Num(); ++ObjectIndex)
					{
						const FString CurrentValueString = ObjectValues[ObjectIndex];
						FString CurrentValueFontName;

						if (FParse::Value(*CurrentValueString, TEXT("FontName="), CurrentValueFontName))
						{
							if (FirstSelectionFontName != CurrentValueFontName)
							{
								bMultipleValues = true;
								break;
							}
						}
					}
				}
			}
		}
	}

	OutMultiFontSelectionData.bMultipleObjectsSelected = bMultipleObjectsSelected;
	OutMultiFontSelectionData.bMultipleValues = bMultipleValues;
	OutMultiFontSelectionData.FirstSelectionFontName = FirstSelectionFontName;
}

void UAvaFontManagerSubsystem::GetFontName(const UFont* InFont, FString& OutFontName)
{
	// this function used to be defined here, but it has been moved to the Runtime module for easier maintenance
	UE::Ava::FontUtilities::GetFontName(InFont, OutFontName);
}

void UAvaFontManagerSubsystem::GetSanitizedFontName(const UFont* InFont, FString& OutFontName)
{
	if (InFont)
	{
		GetFontName(InFont, OutFontName);
		SanitizeString(OutFontName);
	}
}

void UAvaFontManagerSubsystem::RefreshFontsOptions()
{
	FontsOptions.Empty();

	int32 Count = 0;

	for (const TPair<FString, TObjectPtr<UAvaFontObject>>& Elem : FontsMap)
	{
		FString ElemFontName = Elem.Key;
		const TObjectPtr<UAvaFontObject>& FontObject = Elem.Value;

		if (IsValid(FontObject->GetFont()))
		{
			FontObject->GetFont()->GetPackage()->FullyLoad();
			FAvaFont FontOption = FAvaFont(FontObject.Get());
			const bool bFontIsFavorite = IsFavoriteFont(FontOption.GetFontNameAsString());
			FontOption.SetFavorite(bFontIsFavorite);
			FontsOptions.Add(FAvaFontView::Make(FontOption));

			if (FontObject->GetFont() == FAvaFont::GetDefaultFont())
			{
				DefaultFontIndex = Count; 
			}
		}

		Count++;
	}
}

void UAvaFontManagerSubsystem::AddFavorite(const FString& InFontName)
{
	// does something only if needed
	LoadFavorites();

	if (IsValid(FontManagerConfig))
	{
		FontManagerConfig->AddFavoriteFont(InFontName);
	}
}

void UAvaFontManagerSubsystem::RemoveFavorite(const FString& InFontName)
{
	// does something only if needed
	LoadFavorites();

	if (IsValid(FontManagerConfig))
	{
		FontManagerConfig->RemoveFavoriteFont(InFontName);
	}
}

bool UAvaFontManagerSubsystem::IsFavoriteFont(const FString& InFontName)
{
	// does something only if needed
	LoadFavorites();

	if (IsValid(FontManagerConfig))
	{
		return FontManagerConfig->IsFavoriteFont(InFontName);
	}

	return false;
}

TSharedPtr<FAvaFontView> UAvaFontManagerSubsystem::GetFontViewFromPropertyHandle(const TSharedPtr<IPropertyHandle>& InFontPropertyHandle)
{
	FPropertyAccess::Result AccessResult;
	return GetFontViewFromPropertyHandle(InFontPropertyHandle, AccessResult);
}

void UAvaFontManagerSubsystem::MarkFontForAutoImport(const TSharedPtr<IPropertyHandle>& InFontToImportOnSave)
{
	if (const TSharedPtr<FAvaFontView>& AvaFont = GetFontViewFromPropertyHandle(InFontToImportOnSave))
	{
		FontsToImportOnSave.Add(AvaFont, InFontToImportOnSave);

		TArray<UObject*> OuterObjects;
		InFontToImportOnSave->GetOuterObjects(OuterObjects);
	}
}

void UAvaFontManagerSubsystem::UnmarkFontFromAutoImport(const TSharedPtr<IPropertyHandle>& InFontToUnregister)
{
	if (const TSharedPtr<FAvaFontView>& AvaFont = GetFontViewFromPropertyHandle(InFontToUnregister))
	{
		if (InFontToUnregister)
		{
			if (FontsToImportOnSave.Contains(AvaFont))
			{
				FontsToImportOnSave.Remove(AvaFont);
			}
		}
	}
}

void UAvaFontManagerSubsystem::ForceFontAutoImportUnmark(const TSharedPtr<IPropertyHandle>& InFontToUnregister)
{
	if (const TSharedPtr<FAvaFontView>& AvaFont = GetFontViewFromPropertyHandle(InFontToUnregister))
	{
		if (FontsToImportOnSave.Contains(AvaFont))
		{
			FontsToImportOnSave.Remove(AvaFont);
		}
	}
}

void UAvaFontManagerSubsystem::FontsAutoImport()
{
	UnregisterAssetsCallbacks();

	for (const TPair<TSharedPtr<FAvaFontView>, TSharedPtr<IPropertyHandle>>& Pair : FontsToImportOnSave)
	{
		if (const TSharedPtr<FAvaFontView>& AvaFont = Pair.Key)
		{
			UAvaFontObject* FontObjForImport = AvaFont->GetFontObject();

			if (IsValid(FontObjForImport))
			{
				if (ImportAsAsset(FontObjForImport))
				{
					Pair.Value->NotifyPostChange(EPropertyChangeType::ValueSet);
				}
			}
		}
	}

	// not needed anymore
	FontsToImportOnSave.Empty();

	RegisterAssetsCallbacks();
}

void UAvaFontManagerSubsystem::FontsAutoImportPreSave(UWorld* InWorld, FObjectPreSaveContext InObjectPreSaveContext)
{
	FontsAutoImport();
}

UAvaFontConfig* UAvaFontManagerSubsystem::GetFontManagerConfig()
{
	return FontManagerConfig;
}

bool UAvaFontManagerSubsystem::IsFontMonospaced(const UFont* const InFont)
{
	if (InFont)
	{
		int32 Height;

		int32 SpaceWidth;
		InFont->GetStringHeightAndWidth(TEXT(" "), Height, SpaceWidth);

		int32 LWidth;
		InFont->GetStringHeightAndWidth(TEXT("l"), Height, LWidth);

		int32 WWidth;
		InFont->GetStringHeightAndWidth(TEXT("W"), Height, WWidth);

		if ((SpaceWidth == LWidth) && (SpaceWidth == WWidth))
		{
			return true;
		}
	}

	return false;
}

void UAvaFontManagerSubsystem::SetupMetrics(UAvaFontObject* InFontObject)
{
	if (!InFontObject)
	{
		return;
	}

	if (const UFont* const Font = InFontObject->GetFont())
	{
		const FCompositeFont* const CompositeFont = Font->GetCompositeFont();
		if (!CompositeFont || CompositeFont->DefaultTypeface.Fonts.Num() == 0)
		{
			return;
		}

		bool bItalic = false;
		bool bBold = false;

		for (const FTypefaceEntry& TypefaceEntry : CompositeFont->DefaultTypeface.Fonts)
		{
			const FFontFaceDataConstPtr FaceData = TypefaceEntry.Font.GetFontFaceData();

			FT_Face FreeTypeFace;

			if (FaceData.IsValid() && FaceData->HasData() && FaceData->GetData().Num() > 0)
			{
				const TArray<uint8> Data = FaceData->GetData();

				FT_New_Memory_Face(GetFreeTypeLibrary(), Data.GetData(), Data.Num(), 0, &FreeTypeFace);

				if (FreeTypeFace)
				{
					bItalic = FreeTypeFace->style_flags & FT_STYLE_FLAG_ITALIC;
					bBold = FreeTypeFace->style_flags & FT_STYLE_FLAG_BOLD;
				}
			}
		}

		FAvaSystemFontMetrics Metrics;
		Metrics.bIsMonospaced = IsFontMonospaced(Font);
		Metrics.bIsItalic = bItalic;
		Metrics.bIsBold = bBold;

		InFontObject->SetMetrics(Metrics);
	}
}

FT_Library UAvaFontManagerSubsystem::GetFreeTypeLibrary()
{
	InitFreeTypeLibrary();
	return FreeTypeLib;
}

void UAvaFontManagerSubsystem::InitFreeTypeLibrary()
{
	if (!FreeTypeLib)
	{
		FT_Init_FreeType(&FreeTypeLib);
	}
}

void UAvaFontManagerSubsystem::CleanFreeTypeLibrary()
{
	if (FreeTypeLib)
	{
		FT_Done_FreeType(FreeTypeLib);
		FreeTypeLib = nullptr;
	}
}

//////// UAvaFontConfig ////////

bool UAvaFontConfig::IsFavoriteFont(const FString& InFontName) const
{
	return FavoriteFonts.Contains(InFontName);
}

bool UAvaFontConfig::RemoveFavoriteFont(const FString& InFontName)
{
	if (IsFavoriteFont(InFontName))
	{
		FavoriteFonts.Remove(InFontName);
		SaveConfig();
		return true;
	}

	return false;
}

bool UAvaFontConfig::AddFavoriteFont(const FString& InFontName)
{
	if (!IsFavoriteFont(InFontName))
	{
		FavoriteFonts.AddUnique(InFontName);
		SaveConfig();
		return true;
	}

	return false;
}

void UAvaFontConfig::ToggleShowMonospaced()
{
	bShowOnlyMonospaced = !bShowOnlyMonospaced;
	SaveConfig();
}

void UAvaFontConfig::ToggleShowBold()
{
	bShowOnlyBold = !bShowOnlyBold;
	SaveConfig();
}

void UAvaFontConfig::ToggleShowItalic()
{
	bShowOnlyItalic = !bShowOnlyItalic;
	SaveConfig();
}

#undef LOCTEXT_NAMESPACE
