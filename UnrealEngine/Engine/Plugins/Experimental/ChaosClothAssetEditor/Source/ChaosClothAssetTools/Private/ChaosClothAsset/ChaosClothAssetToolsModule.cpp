// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "ClothAssetEditorStyle.h"
#include "ClothComponentEditorStyle.h"
#include "ClothingAssetToClothAssetExporter.h"
#include "ChaosClothAsset/ClothAssetBuilderEditor.h"

namespace UE::Chaos::ClothAsset
{
	class FClothAssetEditorModule : public IModuleInterface, public IClothingAssetExporterClassProvider, public IClothAssetBuilderClassProvider
	{
	public:
		// IModuleInterface implementation
		virtual void StartupModule() override
		{
			// Register asset icons
			FClothAssetEditorStyle::Get();
			FClothComponentEditorStyle::Get();

			// Register modular features
			IModularFeatures::Get().RegisterModularFeature(IClothingAssetExporterClassProvider::FeatureName, static_cast<IClothingAssetExporterClassProvider*>(this));
			IModularFeatures::Get().RegisterModularFeature(IClothAssetBuilderClassProvider::FeatureName, static_cast<IClothAssetBuilderClassProvider*>(this));
		}

		virtual void ShutdownModule() override
		{
			if (UObjectInitialized())
			{
				// Unregister modular features
				IModularFeatures::Get().UnregisterModularFeature(IClothingAssetExporterClassProvider::FeatureName, static_cast<IClothingAssetExporterClassProvider*>(this));
				IModularFeatures::Get().UnregisterModularFeature(IClothAssetBuilderClassProvider::FeatureName, static_cast<IClothAssetBuilderClassProvider*>(this));
			}
		}

		// IClothingSimulationFactoryClassProvider implementation
		virtual TSubclassOf<UClothingAssetExporter> GetClothingAssetExporterClass() const override
		{
			return UClothingAssetToChaosClothAssetExporter::StaticClass();
		}

		// IClothAssetBuilderClassProvider implementation
		virtual TSubclassOf<UClothAssetBuilder> GetClothAssetBuilderClass() const override
		{
			return UClothAssetBuilderEditor::StaticClass();
		}
	};
}

IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FClothAssetEditorModule, ChaosClothAssetEditor);
