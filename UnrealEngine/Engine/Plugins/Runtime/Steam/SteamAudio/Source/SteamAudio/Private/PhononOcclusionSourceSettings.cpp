//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononOcclusionSourceSettings.h"

UPhononOcclusionSourceSettings::UPhononOcclusionSourceSettings()
	: DirectOcclusionMode(EIplDirectOcclusionMode::NONE)
	, DirectOcclusionMethod(EIplDirectOcclusionMethod::RAYCAST)
	, DirectOcclusionSourceRadius(100.0f)
	, DirectAttenuation(true)
	, AirAbsorption(true)
{}

#if WITH_EDITOR
bool UPhononOcclusionSourceSettings::CanEditChange(const FProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPhononOcclusionSourceSettings, DirectOcclusionSourceRadius)))
	{
		return ParentVal && DirectOcclusionMode != EIplDirectOcclusionMode::NONE && DirectOcclusionMethod == EIplDirectOcclusionMethod::VOLUMETRIC;
	}
	else if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPhononOcclusionSourceSettings, DirectOcclusionMethod)))
	{
		return ParentVal && DirectOcclusionMode != EIplDirectOcclusionMode::NONE;
	}

	return ParentVal;
}
#endif
