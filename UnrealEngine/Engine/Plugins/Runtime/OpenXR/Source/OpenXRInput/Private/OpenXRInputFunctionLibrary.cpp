// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRInputFunctionLibrary.h"
#include "OpenXRInputSettings.h"

#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "PlayerMappableInputConfig.h"
#include "UObject/ObjectPtr.h"
#include "XRMotionControllerBase.h"

UOpenXRInputFunctionLibrary::UOpenXRInputFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UOpenXRInputFunctionLibrary::BeginXRSession(UPlayerMappableInputConfig* InputConfig)
{
	if (!InputConfig)
	{
		// If the input config is unset, then we fall back to either the input config from the project settings.
		// If the project settings is also unset, the input config will be set to null and we'll fall back to legacy input.
		UOpenXRInputSettings* InputSettings = GetMutableDefault<UOpenXRInputSettings>();
		if (InputSettings && InputSettings->MappableInputConfig.IsValid())
		{
			InputConfig = (UPlayerMappableInputConfig*)InputSettings->MappableInputConfig.TryLoad();
		}
	}

	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (auto MotionController : MotionControllers)
	{
		if (MotionController == nullptr)
		{
			continue;
		}

		MotionController->SetPlayerMappableInputConfig(InputConfig);
	}

	return UHeadMountedDisplayFunctionLibrary::EnableHMD(true);
}

void UOpenXRInputFunctionLibrary::EndXRSession()
{
	UHeadMountedDisplayFunctionLibrary::EnableHMD(false);
}
