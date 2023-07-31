// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundClassTemplates.h"

#include "Components/SynthComponent.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundEffectSubmix.h"

USoundEffectSourcePresetClassTemplate::USoundEffectSourcePresetClassTemplate(const FObjectInitializer& ObjectInitializer)
	: UClassTemplate(ObjectInitializer)
{
	SetGeneratedBaseClass(USoundEffectSourcePreset::StaticClass());
}

USoundEffectSubmixPresetClassTemplate::USoundEffectSubmixPresetClassTemplate(const FObjectInitializer& ObjectInitializer)
	: UClassTemplate(ObjectInitializer)
{
	SetGeneratedBaseClass(USoundEffectSubmixPreset::StaticClass());
}

USynthComponentClassTemplate::USynthComponentClassTemplate(const FObjectInitializer& ObjectInitializer)
	: UClassTemplate(ObjectInitializer)
{
	SetGeneratedBaseClass(USynthComponent::StaticClass());
}
