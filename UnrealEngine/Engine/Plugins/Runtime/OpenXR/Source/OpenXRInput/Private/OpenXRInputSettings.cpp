// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRInputSettings.h"
#include "XRMotionControllerBase.h"
#include "PlayerMappableInputConfig.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Features/IModularFeatures.h"
#endif

UOpenXRInputSettings::UOpenXRInputSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UOpenXRInputSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FName MemberPropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UOpenXRInputSettings, MappableInputConfig))
	{
		TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
		for (auto MotionController : MotionControllers)
		{
			if (MotionController == nullptr)
			{
				continue;
			}

			MotionController->SetPlayerMappableInputConfig((UPlayerMappableInputConfig*)MappableInputConfig.TryLoad());
		}
	}
}
#endif
