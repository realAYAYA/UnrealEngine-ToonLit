// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RenderingThread.h"
#endif
#include "RenderDeferredCleanup.h"
#include "UObject/GCObject.h"

struct FSlateDynamicImageBrush;
struct FCompositeFont;
struct FStandaloneCompositeFont;

//This is a helper class that we use to hold values we parse from the .ini. Clean way to access things like dynamic image brushes / fonts / etc used in our UI that
//we want to be somewhat data driven but we can't rely on UObject support to implement(as the PreLoad stuff happens too early for UObject support)
//This lets us set easy to change values in our .ini that are parsed at runtime and stored in this container
class PRELOADSCREEN_API FPreLoadSettingsContainerBase : public FDeferredCleanupInterface, public FGCObject
{
public:

    //Helper struct to store groups of things we want to display together in the UI so that we can parse it easily in the .ini. 
    //IE: Show this background, with this text at this font size
    struct FScreenGroupingBase
    {
    public:
        
        FString ScreenBackgroundIdentifer;
        FString TextIdentifier;
        float FontSize;

        FScreenGroupingBase(const FString& ScreenBackgroundIdentifierIn, const FString& TextIdentifierIn, float FontSizeIn)
            : ScreenBackgroundIdentifer(ScreenBackgroundIdentifierIn)
            , TextIdentifier(TextIdentifierIn)
            , FontSize(FontSizeIn)
        {}
    };

	//This is a listing of ScreenGroupings (stored by Identifier) that should be displayed in this order during a particular LoadingGroup.
	//@TODO: TRoss, possible to move these loading groups into their own DeferreedCleanupInterface instead of the entire container being the DeferredCleanupInterface,
	//if we want to support unloading them. For right now though, we mostly just care about loading them selectively and are ok with keeping them in memory until we clean everything up.
	struct FScreenOrderByLoadingGroup
	{
	public:
		TArray<FName> ScreenGroupings;

		FScreenOrderByLoadingGroup()
			: ScreenGroupings ()
		{}
	};

	//Helper struct  to store information required to construct a CustomSlateImageBrush. Parsed from our .ini
	struct FCustomBrushDefine
	{
	public:
		FString BrushIdentifier;
		FString FilePath;
		FVector2D Size;

		FCustomBrushDefine(const FString& BrushIdentifierIn, const FString& FilePathIn, FVector2D SizeIn)
			: BrushIdentifier(BrushIdentifierIn)
			, FilePath(FilePathIn)
			, Size(SizeIn)
		{}
	};

	//Helper struct to store all BrushDefines we need to load for a given BrushLoadingGroup
	struct FCustomBrushLoadingGroup
	{
	public:
		TArray<FCustomBrushDefine> CustomBrushDefinesToLoad;
	};

public:

    static FPreLoadSettingsContainerBase& Get()
    {
        if (Instance == nullptr)
        {
            Instance = new FPreLoadSettingsContainerBase();
        }

        return *Instance;
    }

    static void Destroy()
    {
        if (Instance)
        {
            delete Instance;
            Instance = nullptr;
        }
    }

private:

    FPreLoadSettingsContainerBase() 
		: CurrentLoadGroup(NAME_None)
    {
		bShouldLoadBrushes = true;
		HasCreatedSystemFontFile = false;
    }

    virtual ~FPreLoadSettingsContainerBase();

public:

	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FPreLoadSettingsContainerBase"); }
	//~ End FGCObject interface

    virtual const FSlateDynamicImageBrush* GetBrush(const FString& Identifier);
    virtual FText GetLocalizedText(const FString& Identifier);
    virtual TSharedPtr<FCompositeFont> GetFont(const FString& Identifier);
    virtual FScreenGroupingBase* GetScreenGrouping(const FString& Identifier);

    int GetNumScreenGroupings() const { return ScreenGroupings.Num(); }

	virtual const FScreenGroupingBase* GetScreenAtIndex(int index) const;
	virtual bool IsValidScreenIndex(int index) const;

    virtual void CreateCustomSlateImageBrush(const FString& Identifier, const FString& TexturePath, const FVector2D& ImageDimensions);
    virtual void AddLocalizedText(const FString& Identifier, FText LocalizedText);
    virtual void AddScreenGrouping(const FString& Identifier, FScreenGroupingBase& ScreenGrouping);
    
    //Maps the given font file to the given language and stores it under the FontIdentifier.
    //Identifier maps the entire CompositeFont, so if you want to add multiple fonts  for multiple languages, just store them all under the same identifer
    virtual void BuildCustomFont(const FString& FontIdentifier, const FString& Language, const FString& FilePath);
	virtual bool BuildSystemFontFile();
	virtual const FString GetSystemFontFilePath() const;

    //Helper functions that parse a .ini config entry and call the appropriate create function to 
    virtual void ParseBrushConfigEntry(const FString& BrushConfigEntry);
    virtual void ParseFontConfigEntry(const FString&  SplitConfigEntry);
    virtual void ParseLocalizedTextConfigString(const FString&  SplitConfigEntry);
    virtual void ParseScreenGroupingConfigString(const FString&  SplitConfigEntry);

	//Helper function to parse all .ini entries for LoadingGroups and ScreenOrder. Do these together so we can assert if
	//we don't find a matching LoadingGroup identifier in the config. Should be run after we parse all screen groupings
	virtual void ParseLoadingGroups(TArray<FString>& LoadingGroupIdentifiers);
	virtual void ParseAllScreenOrderEntries(TArray<FString>& LoadingGroups, TArray<FString>& ScreenOrderEntries);
	virtual void ParseScreenOrderConfigString(const FString& ScreenOrderEntry);

    //Sets the PluginContent dir so that when parsing config entries we can accept plugin relative file paths
    virtual void SetPluginContentDir(const FString& PluginContentDirIn) { PluginContentDir = PluginContentDirIn; }

	//Tells the container rather it should actually load image brushes
	virtual void SetShouldLoadBrushes(bool bInShouldLoadBrushes);

    float TimeToDisplayEachBackground;
    
	FName GetCurrentLoadGrouping() const { return CurrentLoadGroup; }
	void LoadGrouping(FName Identifier);
	void PerformInitialAssetLoad();

    //Helper function that takes in a file path and tries to reconsile it to be Plugin Specific if applicable.
    //Ensures if file is not found in either Plugin's content dir or the original path
    virtual FString ConvertIfPluginRelativeContentPath(const FString& FilePath);

protected:

    //Helper functions that verify if the supplied .ini config entry is valid to create a resource out of it
    virtual bool IsValidBrushConfig(TArray<FString>& SplitConfigEntry);
    virtual bool IsValidFontConfigString(TArray<FString>& SplitConfigEntry);
    virtual bool IsValidLocalizedTextConfigString(TArray<FString>& SplitConfigEntry);
    virtual bool IsValidScreenGrooupingConfigString(TArray<FString>& SplitConfigEntry);

protected:
	TArray<FString> ParsedLoadingGroupIdentifiers;

	/* Property Storage. Ties FName to a particular resource so we can get it by identifier. */
    TMap<FName, FSlateDynamicImageBrush*> BrushResources;
    TMap<FName, FText> LocalizedTextResources;
    TMap<FName, TSharedPtr<FStandaloneCompositeFont>> FontResources;

	TMap<FName, FScreenOrderByLoadingGroup> ScreenOrderByLoadingGroups;
    TMap<FName, FScreenGroupingBase> ScreenGroupings;

	TMap<FName, FCustomBrushLoadingGroup> BrushLoadingGroups;

    //This string is used to make file paths relative to a particular Plugin's content directory when parsing file paths.
    FString PluginContentDir;

	// Rather we should load image brushes
	bool bShouldLoadBrushes;

	FName CurrentLoadGroup;

	bool HasCreatedSystemFontFile;

	//If our Font filepath is set to this, we use the system font instead of a custom font we load in
	static FString UseSystemFontOverride;

	//If we supply no loading groups, use this identifier by default
	static FString DefaultInitialLoadingGroupIdentifier;

    // Singleton Instance -- This is only not a TSharedPtr as it needs to be cleaned up by a deferredcleanup call which directly
    // destroys the underlying object, causing a SharedPtr crash at shutdown.
    static FPreLoadSettingsContainerBase* Instance;
};