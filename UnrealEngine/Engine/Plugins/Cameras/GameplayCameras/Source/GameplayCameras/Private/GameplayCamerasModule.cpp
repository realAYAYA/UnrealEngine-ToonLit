// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCamerasModule.h"
#include "Camera/CameraModularFeature.h"
#include "CameraAnimationCameraModifier.h"
#include "CameraAnimationSequencePlayer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

IGameplayCamerasModule& IGameplayCamerasModule::Get()
{
	return FModuleManager::LoadModuleChecked<IGameplayCamerasModule>("Camera");
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

		UCameraAnimationSequenceCameraStandIn::RegisterCameraStandIn();
	}

	virtual void ShutdownModule() override
	{
		if (CameraModularFeature.IsValid())
		{
			IModularFeatures::Get().UnregisterModularFeature(ICameraModularFeature::GetModularFeatureName(), CameraModularFeature.Get());
			CameraModularFeature = nullptr;
		}

		UCameraAnimationSequenceCameraStandIn::UnregisterCameraStandIn();
	}

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
};

IMPLEMENT_MODULE(FGameplayCamerasModule, GameplayCameras);
