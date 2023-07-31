// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions/AssetTypeActions_SoundBase.h"
#include "Containers/Array.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMenuBuilder;
class FString;
class IToolkitHost;
class SWidget;
class UClass;
class UObject;
class USoundWave;
struct FAssetData;

class FAssetTypeActions_SoundWave : public FAssetTypeActions_SoundBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundWave", "Sound Wave"); }
	virtual FColor GetTypeColor() const override { return FColor(97, 85, 212); }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;

private:

	/** Creates a SoundCue of the same name for the sound, if one does not already exist */
	void ExecuteCreateSoundCue(TArray<TWeakObjectPtr<USoundWave>> Objects, bool bCreateCueForEachSoundWave = true);

	/** Creates a DialogueWave of the same name for the sound, if one does not already exist */
	void ExecuteCreateDialogueWave(const struct FAssetData& AssetData, TArray<TWeakObjectPtr<USoundWave>> Objects);

	void FillVoiceMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USoundWave>> Objects);
};
