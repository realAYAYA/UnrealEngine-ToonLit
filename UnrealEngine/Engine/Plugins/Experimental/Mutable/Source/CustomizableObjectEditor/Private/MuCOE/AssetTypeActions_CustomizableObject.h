// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "IAssetTypeActions.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "MuCO/CustomizableObject.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMenuBuilder;
class IToolkitHost;
class UClass;
class UObject;

enum class ERecompileCO
{
	RCO_All,
	RCO_AllRootObjects,
	RCO_Selected,
	RCO_InMemory
};

class FAssetTypeActions_CustomizableObject : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CustomizableObject", "Customizable Object"); }
	FColor GetTypeColor() const override { return FColor(234, 255, 0); }
	UClass* GetSupportedClass() const override { return UCustomizableObject::StaticClass(); }
	void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	uint32 GetCategories() override;


	bool AssetsActivatedOverride( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType ) override;

	bool CanFilter() override { return true; }
	bool ShouldForceWorldCentric() override { return false; }
	void PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override {}

private:

	void ExecuteEdit(TArray<TWeakObjectPtr<UCustomizableObject>> Objects);	
	void ExecuteDebug(TArray<TWeakObjectPtr<UCustomizableObject>> Objects);
	void ExecuteNewInstance(TArray<TWeakObjectPtr<UCustomizableObject>> Objects);

	void MakeRecompileSubMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UCustomizableObject>> InObjects);
	void RecompileObjects(ERecompileCO Mode, TArray<TWeakObjectPtr<UCustomizableObject>> InObjects);
	
};
