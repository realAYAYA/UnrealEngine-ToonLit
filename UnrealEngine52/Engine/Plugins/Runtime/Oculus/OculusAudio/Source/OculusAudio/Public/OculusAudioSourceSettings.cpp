// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudioSourceSettings.h"

UOculusAudioSourceSettings::UOculusAudioSourceSettings() :
	 EarlyReflectionsEnabled(true)
	, AttenuationEnabled(true)
	, AttenuationRangeMinimum(0.25f)
	, AttenuationRangeMaximum(250.0f)
	, VolumetricRadius(0.0f)
	, ReverbSendLevel(0.0f)
{

}

#if WITH_EDITOR
bool UOculusAudioSourceSettings::CanEditChange(const FProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UOculusAudioSourceSettings, AttenuationRangeMinimum)))
	{
		return ParentVal && AttenuationEnabled;
	}
	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UOculusAudioSourceSettings, AttenuationRangeMaximum)))
	{
		return ParentVal && AttenuationEnabled;
	}

	return ParentVal;
}
#endif