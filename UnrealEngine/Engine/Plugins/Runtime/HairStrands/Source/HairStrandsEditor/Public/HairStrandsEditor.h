// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"
#include "HAL/LowLevelMemTracker.h"

#define HAIRSTRANDSEDITOR_MODULE_NAME TEXT("HairStrandsEditor")

class IGroomTranslator;

LLM_DECLARE_TAG_API(GroomEditor, HAIRSTRANDSEDITOR_API);

/** Implements the HairStrands module  */
class HAIRSTRANDSEDITOR_API FGroomEditor : public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	static inline FGroomEditor& Get()
	{
		LLM_SCOPE_BYTAG(GroomEditor);
		return FModuleManager::LoadModuleChecked<FGroomEditor>(HAIRSTRANDSEDITOR_MODULE_NAME);
	}

	/** Register HairStrandsTranslator to add support for import by the HairStandsFactory */
	template <typename TranslatorType>
	void RegisterHairTranslator()
	{
		TranslatorSpawners.Add([]
		{
			return MakeShared<TranslatorType>();
		});
	}

	/** Get new instances of HairStrandsTranslators */
	TArray<TSharedPtr<IGroomTranslator>> GetHairTranslators();

private:
	void RegisterMenus();

	/** The collection of registered asset type actions. */
	TArray<TSharedRef<class IAssetTypeActions>> RegisteredAssetTypeActions;

	TArray<TFunction<TSharedPtr<IGroomTranslator>()>> TranslatorSpawners;

	TSharedPtr<FSlateStyleSet> StyleSet;

	FDelegateHandle TrackEditorBindingHandle;

public:

	static FName GroomEditorAppIdentifier;
};
