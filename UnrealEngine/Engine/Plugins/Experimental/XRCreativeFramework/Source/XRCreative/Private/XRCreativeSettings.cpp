// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCreativeSettings.h"


UXRCreativeSettings* UXRCreativeSettings::GetXRCreativeSettings()
{
	return GetMutableDefault<UXRCreativeSettings>();
}
