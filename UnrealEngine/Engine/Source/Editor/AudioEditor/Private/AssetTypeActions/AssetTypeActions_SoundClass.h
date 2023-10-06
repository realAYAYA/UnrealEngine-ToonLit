// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMenuBuilder;
class IToolkitHost;
class UClass;
class UObject;
class USoundClass;

class FAssetTypeActions_SoundClass : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundClass", "Sound Class"); }
	virtual FColor GetTypeColor() const override { return FColor(255, 175, 0); }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
private:
	
	/** Handler for Mute is selected  */
	void ExecuteMute(TArray<TWeakObjectPtr<USoundClass>> Objects) const;

	/** Handler for Solo is selected  */
	void ExecuteSolo(TArray<TWeakObjectPtr<USoundClass>> Objects) const;

	/** Returns true if the mute state is set.  */
	bool IsActionCheckedMute(TArray<TWeakObjectPtr<USoundClass>> Objects) const;

	/** Returns true if the solo state is set.  */
	bool IsActionCheckedSolo(TArray<TWeakObjectPtr<USoundClass>> Objects) const;

	/** Returns true if its possible to mute a sound */
	bool CanExecuteMuteCommand(TArray<TWeakObjectPtr<USoundClass>> Objects) const;

	/** Returns true if its possible to solo a sound */
	bool CanExecuteSoloCommand(TArray<TWeakObjectPtr<USoundClass>> Objects) const;
};
