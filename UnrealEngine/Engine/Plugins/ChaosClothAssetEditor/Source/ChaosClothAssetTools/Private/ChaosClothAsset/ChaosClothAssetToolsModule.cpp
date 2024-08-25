// Copyright Epic Games, Inc. All Rights Reserved.

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "ChaosClothAsset/ClothAssetEditorStyle.h"
#include "ClothComponentEditorStyle.h"
#include "ClothingAssetToClothAssetExporter.h"

namespace UE::Chaos::ClothAsset
{
	class FChaosClothAssetToolsModule : public IModuleInterface, public IClothingAssetExporterClassProvider
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
		}

		virtual void ShutdownModule() override
		{
			if (UObjectInitialized())
			{
				// Unregister modular features
				IModularFeatures::Get().UnregisterModularFeature(IClothingAssetExporterClassProvider::FeatureName, static_cast<IClothingAssetExporterClassProvider*>(this));
			}
		}

		// IClothingSimulationFactoryClassProvider implementation
		virtual TSubclassOf<UClothingAssetExporter> GetClothingAssetExporterClass() const override
		{
			return UClothingAssetToChaosClothAssetExporter::StaticClass();
		}
	};
}

IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FChaosClothAssetToolsModule, ChaosClothAssetTools);
