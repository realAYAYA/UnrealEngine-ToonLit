// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Font/AvaFontObject.h"
#include "Misc/CoreMisc.h"
#include "PropertyEditorModule.h"

THIRD_PARTY_INCLUDES_START
#include "ft2build.h"
#include FT_FREETYPE_H
THIRD_PARTY_INCLUDES_END

#include "AvaFontManagerSubsystem.generated.h"

class FAvaFontView;
class IPropertyHandle;
class UAvaFontObject;
class UFactory;
struct FAvaFont;

DECLARE_LOG_CATEGORY_EXTERN(LogAvaFont, Log, All);

/** Facilitator struct to handle multiple selection */
struct FAvaMultiFontSelectionData
{
	/** true if multiple objects are selected */
	bool bMultipleObjectsSelected;

	/** true if selection has multiple values - implies bAreMultipleItemsSelected is true */
	bool bMultipleValues;

	/** the name of the first selected font. Useful when all items reference the same font */
	FString FirstSelectionFontName;
};

UCLASS(Config=Editor)
class UAvaFontConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, BlueprintReadOnly, Category = "Motion Design")
	TArray<FString> FavoriteFonts;

	UPROPERTY(Config, BlueprintReadOnly, Category = "Motion Design")
	bool bShowOnlyMonospaced;

	UPROPERTY(Config, BlueprintReadOnly, Category = "Motion Design")
	bool bShowOnlyBold;

	UPROPERTY(Config, BlueprintReadOnly, Category = "Motion Design")
	bool bShowOnlyItalic;

	bool IsFavoriteFont(const FString& InFontName) const;
	bool RemoveFavoriteFont(const FString& InFontName);
	bool AddFavoriteFont(const FString& InFontName);

	void ToggleShowMonospaced();
	void ToggleShowBold();
	void ToggleShowItalic();
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnProjectFontsChange, const UFont*)
DECLARE_MULTICAST_DELEGATE(FOnSystemFontsUpdated)

UCLASS()
class UAvaFontManagerSubsystem : public UEditorSubsystem, public FSelfRegisteringExec
{
	friend class FAvaTextEditorModule;

	GENERATED_BODY()
public:
	/**
	 * Gets the singleton instance of the fonts manager.
	 * @return The FontImportManager instance.
	 */
	static UAvaFontManagerSubsystem* Get();

	/** Is this font .ttf or .otf? */
	static bool IsSupportedFontFile(const FString& InFontFilePath);

	/**
	 * Retrieve information about multiple selection
	 * @param InPropertyHandle The property handle to check
	 * @param OutMultiFontSelectionData Will store selection data
	 */
	static void GetMultipleSelectionInformation(const TSharedPtr<IPropertyHandle>& InPropertyHandle, FAvaMultiFontSelectionData& OutMultiFontSelectionData);

	/**
	 * Returns a raw pointer to the FAvaFont property handled by the specified property handle (if valid).
	 * @param InFontPropertyHandle A property handle dealing with a property of type FAvaFont
	 * @param OutAccessResult Result of the access operation
	 * @return The handled FAvaFont, as a pointer.
	 */
	static FAvaFont* GetFontFromPropertyHandle(const TSharedPtr<IPropertyHandle>& InFontPropertyHandle, FPropertyAccess::Result& OutAccessResult);

	static void SanitizeString(FString& OutSanitizedName);

	// ~Begin UEditorSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UEditorSubsystem

	// ~Begin FSelfRegisteringExec
	virtual bool Exec(class UWorld* InWorld, const TCHAR* InCmd, FOutputDevice& InAr) override;
	// ~End FSelfRegisteringExec

	void MarkFontForAutoImport(const TSharedPtr<IPropertyHandle>& InFontToImportOnSave);
	void AddFavorite(const FString& InFontName);
	void RemoveFavorite(const FString& InFontName);

	UAvaFontConfig* GetFontManagerConfig();

	TConstArrayView<const TSharedPtr<FAvaFontView>> GetFontOptions();

	int32 GetDefaultFontIndex() const { return DefaultFontIndex; }

	/** Given a Property Handle, tries to retrieve and return a font view from FontsOptions */
	TSharedPtr<FAvaFontView> GetFontViewFromPropertyHandle(const TSharedPtr<IPropertyHandle>& InFontPropertyHandle, FPropertyAccess::Result& OutAccessResult);

	FOnProjectFontsChange& OnProjectFontCreated() { return OnProjectFontCreatedDelegate; }
	FOnProjectFontsChange& OnProjectFontDeleted() { return OnProjectFontDeletedDelegate; }
	FOnSystemFontsUpdated& OnSystemFontsUpdated() { return OnSystemFontsUpdatedDelegate; }
	/**
	 * Import the font contained in the UAvaFontObject as .uasset, if yet to be imported
	 * @param InAvaFontPropertyHandle a property handle to the FAvaFont referencing the UFont to be saved
	 * @return success
	 */
	bool ImportFont(const TSharedPtr<IPropertyHandle>& InAvaFontPropertyHandle);

	void RefreshSystemFont(UFont* InFont) const;

	bool IsFontAvailableOnLocalOS(const UFont* InFont) const;

private:
	static void SetupFontFamilyTypefaces(UFont* InFont, const FSystemFontsRetrieveParams& InFontParams);

	/**
	 * Initializes OS fonts and Motion Design fonts data structures
	 * Since this is called before project assets are available, it will NOT initialize project fonts.
	 * That is done later by InitializeProjectFonts function.
	 */
	void Initialize();

	/**
	 * Adds project fonts (uassets) to the list of fonts handled by the font manager
	 * It will also handle duplicates, e.g. system fonts which have already been imported
	 * Note: make sure to call this when AssetRegistry module is ready.
	 */
	void InitializeProjectFonts();

	bool IsFavoriteFont(const FString& InFontName);
	bool ImportAsAsset(UAvaFontObject* InFontToImport);
	bool RemoveProjectFont(const UFont* InSourceFont);

	UAvaFontObject* CreateProjectFont(UFont* InSourceFont, UPackage* InProjectFontsPackage = nullptr);

	/** Given a font name, tries to retrieve and return a font from FontsOptions */
	TSharedPtr<FAvaFontView> GetFontViewFromName(const FString& InName);

	/** Given a Property Handle, tries to retrieve and return a font from FontsOptions as a raw pointer */
	TSharedPtr<FAvaFontView> GetFontViewFromPropertyHandle(const TSharedPtr<IPropertyHandle>& InFontPropertyHandle);

	void ClearFontsData();
	void LoadFavorites();
	void OnAssetsAdded(const FAssetData& InAssetData);
	void OnAssetDeleted(UObject* InObject);
	void LoadOSFonts();
	void LoadProjectFonts();
	void RemoveProjectFontsFromOSFonts();
	void RefreshFontsMap();
	void RefreshFontsOptions();
	void SortFontsMap();
	void ImportSystemFontFamily(const FSystemFontsRetrieveParams& InFontParams, UPackage* InImportPackage);
	void UnmarkFontFromAutoImport(const TSharedPtr<IPropertyHandle>& InFontToUnregister);
	void ForceFontAutoImportUnmark(const TSharedPtr<IPropertyHandle>& InFontToUnregister);
	void FontsAutoImport();
	void FontsAutoImportPreSave(UWorld* InWorld, FObjectPreSaveContext InObjectPreSaveContext);
	void RegisterDefaultAutoImportCallbacks();
	void UnregisterDefaultAutoImportCallbacks() const;
	void RefreshAllSystemFonts() const;

	static void GetFontName(const UFont* InFont, FString& OutFontName);
	static void GetSanitizedFontName(const UFont* InFont, FString& OutFontName);
	static void SetupMetrics(UAvaFontObject* InFontObject);

	static bool IsFontMonospaced(const UFont* const InFont);
	static FString GetImportFontPackageNameRoot() { return TEXT("/Game/SystemFonts"); }
	static FString GetTempFontPackageNameRoot() { return TEXT("/Temp/SystemFonts"); }
	static FString GetFontObjPackageNameRoot() { return TEXT("/Temp/SystemFonts/FontObjs"); }
	static FString GetFontConfigPackageName() { return TEXT("/Temp/MotionDesignEditor/FontConfig"); }

	static void InitFreeTypeLibrary();
	static void CleanFreeTypeLibrary();
	static FT_Library GetFreeTypeLibrary();

	static const TCHAR* GetFallbackFontFaceReference() { return FallbackFontReference; }

	/** Used internally to load system fonts for the first time */
	void InitializeOSFonts();

	/** updates the entire fonts list by combining OS and Project fonts */
	void CombineAllFonts();

	void RegisterAssetsCallbacks();
	void UnregisterAssetsCallbacks() const;

	UFontFace* GetFallbackFontFace();

	TArray<FString> GetSystemFontNames() const;
	TArray<FString> GetSystemFontTypefaces(const FString& InFontFamilyName) const;
	inline static FT_Library FreeTypeLib;

	UPROPERTY(Transient)
	TObjectPtr<UFontFace> DefaultFallbackFontFace;

	/** used for custom config - e.g. favorite fonts */
	UPROPERTY(Transient)
	TObjectPtr<UAvaFontConfig> FontManagerConfig;

	/** Fonts available from the OS */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAvaFontObject>> OSFontsMap;

	/** Fonts available from current project */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAvaFontObject>> ProjectFontsMap;

	/** OS + current project fonts combined */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UAvaFontObject>> FontsMap;

	/** Caching font options here, so that details panel can get them right away */
	TArray<TSharedPtr<FAvaFontView>> FontsOptions;

	/** Used to allow auto font import on Motion Design asset save */
	TMap<TSharedPtr<FAvaFontView>, TSharedPtr<IPropertyHandle>> FontsToImportOnSave;

	TMap<FString, FSystemFontsRetrieveParams> FontsInfoMap;

	int32 DefaultFontIndex = -1;

	FOnProjectFontsChange OnProjectFontCreatedDelegate;
	FOnProjectFontsChange OnProjectFontDeletedDelegate;
	FOnSystemFontsUpdated OnSystemFontsUpdatedDelegate;

	static constexpr TCHAR FallbackFontReference[] = TEXT("/Engine/EngineFonts/Faces/DroidSansFallback.DroidSansFallback");
};
