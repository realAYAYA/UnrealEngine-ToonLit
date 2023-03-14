// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_HapticFeedback.h"
#include "Haptics/HapticFeedbackEffect_Buffer.h"
#include "Haptics/HapticFeedbackEffect_Curve.h"
#include "Haptics/HapticFeedbackEffect_SoundWave.h"

UClass* FAssetTypeActions_HapticFeedbackEffectBuffer::GetSupportedClass() const
{
	return UHapticFeedbackEffect_Buffer::StaticClass();
}

UClass* FAssetTypeActions_HapticFeedbackEffectCurve::GetSupportedClass() const
{
	return UHapticFeedbackEffect_Curve::StaticClass();
}

UClass* FAssetTypeActions_HapticFeedbackEffectSoundWave::GetSupportedClass() const
{
	return UHapticFeedbackEffect_SoundWave::StaticClass();
}
