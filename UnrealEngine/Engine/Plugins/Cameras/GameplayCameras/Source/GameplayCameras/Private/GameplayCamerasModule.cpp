// Copyright Epic Games, Inc. All Rights Reserved.

#include "IGameplayCamerasModule.h"

#include "Camera/CameraModularFeature.h"
#include "CameraAnimationCameraModifier.h"
#include "CameraAnimationSequencePlayer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

IGameplayCamerasModule& IGameplayCamerasModule::Get()
{
	return FModuleManager::LoadModuleChecked<IGameplayCamerasModule>("GameplayCameras");
}

class FGameplayCamerasModule : public IGameplayCamerasModule
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override
	{
		CameraModularFeature = MakeShared<FCameraModularFeature>();
		if (CameraModularFeature.IsValid())
		{
			IModularFeatures::Get().RegisterModularFeature(ICameraModularFeature::GetModularFeatureName(), CameraModularFeature.Get());
		}
	}

	virtual void ShutdownModule() override
	{
		if (CameraModularFeature.IsValid())
		{
			IModularFeatures::Get().UnregisterModularFeature(ICameraModularFeature::GetModularFeatureName(), CameraModularFeature.Get());
			CameraModularFeature = nullptr;
		}
	}

	// IGameplayCamerasModule interface
#if WITH_EDITOR
	virtual TSharedPtr<IGameplayCamerasLiveEditManager> GetLiveEditManager() const override
	{
		return LiveEditManager;
	}

	virtual void SetLiveEditManager(TSharedPtr<IGameplayCamerasLiveEditManager> InLiveEditManager) override
	{
		LiveEditManager = InLiveEditManager;
	}
#endif

private:
	class FCameraModularFeature : public ICameraModularFeature
	{
		// ICameraModularFeature interface
		virtual void GetDefaultModifiers(TArray<TSubclassOf<UCameraModifier>>& ModifierClasses) const override
		{
			ModifierClasses.Add(UCameraAnimationCameraModifier::StaticClass());
		}
	};

	TSharedPtr<FCameraModularFeature> CameraModularFeature;

#if WITH_EDITOR
	TSharedPtr<IGameplayCamerasLiveEditManager> LiveEditManager;
#endif
};

IMPLEMENT_MODULE(FGameplayCamerasModule, GameplayCameras);
