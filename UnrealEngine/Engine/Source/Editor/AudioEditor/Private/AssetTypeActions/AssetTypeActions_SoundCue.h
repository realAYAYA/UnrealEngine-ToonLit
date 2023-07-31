// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions/AssetTypeActions_SoundBase.h"
#include "Containers/Array.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IToolkitHost;
class UClass;
class UObject;
class USoundCue;

class FAssetTypeActions_SoundCue : public FAssetTypeActions_SoundBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundCue", "Sound Cue"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 175, 255); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }
	virtual const TArray<FText>& GetSubMenus() const override;

private:
	/** Take selected SoundCues and combine, as much as possible, them to using shared attenuation settings */
	void ExecuteConsolidateAttenuation(TArray<TWeakObjectPtr<USoundCue>> Objects);

	/** Returns true if more than one cue is selected to consolidate */
	bool CanExecuteConsolidateCommand(TArray<TWeakObjectPtr<USoundCue>> Objects) const;
};
