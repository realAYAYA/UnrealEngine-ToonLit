// Copyright Epic Games, Inc. All Rights Reserved.

#include "Retargeter/IKRetargetSettings.h"
#include "Retargeter/IKRetargeter.h"

bool FTargetChainSpeedPlantSettings::operator==(const FTargetChainSpeedPlantSettings& Other) const
{
	return EnableSpeedPlanting == Other.EnableSpeedPlanting
	&& SpeedCurveName == Other.SpeedCurveName
	&& FMath::IsNearlyEqualByULP(SpeedThreshold, Other.SpeedThreshold)
	&& FMath::IsNearlyEqualByULP(UnplantStiffness, Other.UnplantStiffness)
	&& FMath::IsNearlyEqualByULP(UnplantCriticalDamping, Other.UnplantCriticalDamping);
}

bool FTargetChainFKSettings::operator==(const FTargetChainFKSettings& Other) const
{
	return EnableFK == Other.EnableFK
	   && RotationMode == Other.RotationMode
	   && FMath::IsNearlyEqualByULP(RotationAlpha,Other.RotationAlpha)
	   && TranslationMode == Other.TranslationMode
	   && FMath::IsNearlyEqualByULP(TranslationAlpha, Other.TranslationAlpha)
	   && PoleVectorMatching == Other.PoleVectorMatching
	   && FMath::IsNearlyEqualByULP(PoleVectorOffset, Other.PoleVectorOffset);
}

bool FTargetChainIKSettings::operator==(const FTargetChainIKSettings& Other) const
{
	return EnableIK == Other.EnableIK
	&& FMath::IsNearlyEqualByULP(BlendToSource, Other.BlendToSource)
	&& BlendToSourceWeights.Equals(Other.BlendToSourceWeights)
	&& StaticOffset.Equals(Other.StaticOffset)
	&& StaticLocalOffset.Equals(Other.StaticLocalOffset)
	&& StaticRotationOffset.Equals(Other.StaticRotationOffset)
	&& FMath::IsNearlyEqualByULP(Extension, Other.Extension)
	&& bAffectedByIKWarping == Other.bAffectedByIKWarping;
}

void FTargetChainSettings::CopySettingsFromAsset(const URetargetChainSettings* AssetChainSettings)
{
	*this = AssetChainSettings->Settings;
}

bool FTargetChainSettings::operator==(const FTargetChainSettings& Other) const
{
	return FK == Other.FK && IK == Other.IK && SpeedPlanting == Other.SpeedPlanting;
}

void FTargetRootSettings::CopySettingsFromAsset(const URetargetRootSettings* AssetRootSettings)
{
	*this = AssetRootSettings->Settings;
}
