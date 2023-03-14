// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FMenuBuilder;
class IToolkitHost;
class UClass;
class UCustomizableObjectPopulation;
class UObject;


class FAssetTypeActions_CustomizableObjectPopulation : public FAssetTypeActions_Base
{
	// IAssetTypeActions Implementation
	FText GetName() const override;
	FColor GetTypeColor() const override;
	UClass* GetSupportedClass() const override;
	uint32 GetCategories();

	void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;

	//Actions
	void RecompilePopulations(TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Populations);
	//void CreatePopulationGeneratorAsset(TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Objects);
	//void RecompilePopulationGenerators(TArray<TWeakObjectPtr<UCustomizableObjectPopulation>> Objects);

};
