// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCPropertyUtilities.h"

#include "CoreMinimal.h"

#include "RCTypeUtilities.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR

template <>
bool RemoteControlPropertyUtilities::Deserialize<FProperty>(const FRCPropertyVariant& InSrc, FRCPropertyVariant& OutDst)
{
	const FProperty* Property = OutDst.GetProperty();
	FOREACH_CAST_PROPERTY(Property, Deserialize<CastPropertyType>(InSrc, OutDst))

	return true;
}

template <>
bool RemoteControlPropertyUtilities::Serialize<FProperty>(const FRCPropertyVariant& InSrc, FRCPropertyVariant& OutDst)
{
	const FProperty* Property = InSrc.GetProperty();
	FOREACH_CAST_PROPERTY(Property, Serialize<CastPropertyType>(InSrc, OutDst))

	return true;
}

#endif
