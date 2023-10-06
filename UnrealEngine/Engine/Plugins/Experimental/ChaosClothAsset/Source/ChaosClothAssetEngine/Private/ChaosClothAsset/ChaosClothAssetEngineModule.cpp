// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAssetPrivate.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ChaosClothAsset/ClothComponentCacheAdapter.h"

DEFINE_LOG_CATEGORY(LogChaosClothAsset);

namespace UE::Chaos::ClothAsset
{
	class FClothAssetEngineModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			ClothComponentCacheAdapter = MakeUnique<FClothComponentCacheAdapter>();
			RegisterAdapter(ClothComponentCacheAdapter.Get());
		}

		virtual void ShutdownModule() override
		{
		}
	private:
		TUniquePtr<FClothComponentCacheAdapter> ClothComponentCacheAdapter;
	};
}
IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FClothAssetEngineModule, ChaosClothAssetEngine);
