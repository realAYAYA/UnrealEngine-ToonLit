// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Sound/DialogueWave.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IToolkitHost;
class UClass;
class UObject;


class FAssetTypeActions_DialogueWave : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueWave", "Dialogue Wave"); }
	virtual FColor GetTypeColor() const override { return FColor(97, 85, 212); }
	virtual UClass* GetSupportedClass() const override { return UDialogueWave::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual bool CanFilter() override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;

private:

	bool CanExecutePlayCommand(TArray<TWeakObjectPtr<UDialogueWave>> Objects) const;

	void ExecutePlaySound(TArray<TWeakObjectPtr<UDialogueWave>> Objects);

	void ExecuteStopSound(TArray<TWeakObjectPtr<UDialogueWave>> Objects);

	void PlaySound(UDialogueWave* DialogueWave);

	void StopSound();

	void ExecuteCreateSoundCue(TArray<TWeakObjectPtr<UDialogueWave>> Objects, bool bCreateCueForEachDialogueWave = true);

};
